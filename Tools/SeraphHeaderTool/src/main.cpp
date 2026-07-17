//
// SeraphHeaderTool (SHT) — SHT 2.
//
// Parses a C++ header with libclang (in -DSP_SHT_PARSE mode), discovers types /
// fields / enums carrying "sp:" annotate attributes (from the SCLASS / SPROPERTY
// / SENUM macros in Seraph/Reflection/Annotations.h), and EMITS a .gen.cpp of
// fluent registrations into the runtime reflection registry — exactly the calls
// a human would hand-write via Reflect.h. See Todo/plans/reflection-plan.md
// (SHT section + examples A, E).
//
//   Emit:  SeraphHeaderTool <header.h> -o <out.gen.cpp> [--include <inc>] [-- <clang args>]
//   Dump:  SeraphHeaderTool <header.h> [-- <clang args>]          (no codegen)
//
// Loud-failure rules (example E): an SPROPERTY on an unsupported construct
// (bitfield) or a private SPROPERTY without SP_REFLECT() in the class is a hard
// error with file:line + non-zero exit — never a silent skip.
//

#include <clang-c/Index.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace
{

std::string Str(CXString s)
{
    const char* c = clang_getCString(s);
    std::string out = c ? c : "";
    clang_disposeString(s);
    return out;
}

struct Location
{
    std::string File;
    unsigned Line = 0;
    unsigned Col = 0;
};

Location LocationOf(CXCursor c)
{
    Location loc;
    CXSourceLocation sl = clang_getCursorLocation(c);
    CXFile file;
    unsigned off;
    clang_getSpellingLocation(sl, &file, &loc.Line, &loc.Col, &off);
    if (file)
        loc.File = Str(clang_getFileName(file));
    return loc;
}

bool IsFromMainFile(CXCursor c)
{
    return clang_Location_isFromMainFile(clang_getCursorLocation(c)) != 0;
}

// The "sp:"-prefixed annotate payload on a cursor's direct children, or empty.
std::string SpAnnotation(CXCursor c)
{
    std::string result;
    clang_visitChildren(
        c,
        [](CXCursor child, CXCursor, CXClientData data) -> CXChildVisitResult
        {
            if (clang_getCursorKind(child) == CXCursor_AnnotateAttr)
            {
                std::string s = Str(clang_getCursorSpelling(child));
                if (s.rfind("sp:", 0) == 0)
                    *static_cast<std::string*>(data) = s;
            }
            return CXChildVisit_Continue;
        },
        &result);
    return result;
}

// Fully-qualified name of a type/enum cursor, e.g. "Seraph::PhysicsSettings".
std::string QualifiedName(CXCursor c)
{
    std::vector<std::string> parts;
    parts.push_back(Str(clang_getCursorSpelling(c)));
    for (CXCursor p = clang_getCursorSemanticParent(c);
         !clang_Cursor_isNull(p);
         p = clang_getCursorSemanticParent(p))
    {
        CXCursorKind k = clang_getCursorKind(p);
        if (k != CXCursor_Namespace && k != CXCursor_StructDecl
            && k != CXCursor_ClassDecl)
            break;
        parts.push_back(Str(clang_getCursorSpelling(p)));
    }
    std::string out;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it)
    {
        if (!out.empty())
            out += "::";
        out += *it;
    }
    return out;
}

std::string Payload(const std::string& annotation)
{
    auto pos = annotation.find(':', 3); // after "sp:"
    return pos == std::string::npos ? "" : annotation.substr(pos + 1);
}

std::string Trim(std::string s)
{
    auto notSpace = [](char c) { return c != ' ' && c != '\t'; };
    while (!s.empty() && !notSpace(s.front()))
        s.erase(s.begin());
    while (!s.empty() && !notSpace(s.back()))
        s.pop_back();
    return s;
}

// Split "Min = 0.f, Max = 1.f, Tooltip = \"a, b\"" into (key, value-expr) pairs.
// Commas inside string literals are not separators.
std::vector<std::pair<std::string, std::string>> SplitAttrs(const std::string& p)
{
    std::vector<std::pair<std::string, std::string>> out;
    std::string cur;
    bool inStr = false;
    auto flush = [&]()
    {
        std::string part = Trim(cur);
        cur.clear();
        if (part.empty())
            return;
        auto eq = part.find('=');
        if (eq == std::string::npos)
            out.push_back({Trim(part), ""}); // flag with no value
        else
            out.push_back({Trim(part.substr(0, eq)), Trim(part.substr(eq + 1))});
    };
    for (char c : p)
    {
        if (c == '"')
            inStr = !inStr;
        if (c == ',' && !inStr)
            flush();
        else
            cur.push_back(c);
    }
    flush();
    return out;
}

struct PropertyInfo
{
    std::string Name;
    std::string TypeSpelling;
    std::string Payload;
    bool Private = false;
    // Accessor property (Unreal UPROPERTY(Getter=,Setter=)): when both are set,
    // the property is emitted as .Property<&T::Getter, &T::Setter>(...) rather
    // than a field pointer. Lets a computed/invariant-maintaining value (e.g.
    // Transform rotation euler<->quat) be reflected without touching the field.
    std::string Getter;
    std::string Setter;
    Location Loc;

    [[nodiscard]] bool IsAccessor() const
    {
        return !Getter.empty() && !Setter.empty();
    }
};

struct TypeInfo
{
    std::string QualName;
    std::string Kind;    // "struct" | "class" | "enum"
    std::string Payload; // SCLASS(...) argument list -> type-level attributes
    std::string BaseQual; // qualified base name, empty if none
    bool BaseReflected = false;
    bool HasReflectHook = false; // SP_REFLECT() present (SpReflectMembers member)
    std::vector<PropertyInfo> Properties;
    std::vector<std::string> Enumerators;
    std::string EnumQual;
    int Errors = 0;
    Location Loc;
};

CXChildVisitResult VisitRecordMember(CXCursor c, CXCursor, CXClientData data)
{
    auto* type = static_cast<TypeInfo*>(data);
    CXCursorKind kind = clang_getCursorKind(c);

    if (kind == CXCursor_CXXBaseSpecifier)
    {
        if (type->BaseQual.empty())
        {
            // Use the base's DECLARATION for a fully-qualified name — the base
            // type spelling is source-relative ("Base"), but the generated
            // .Base<...>() is emitted at namespace scope and needs "NS::Base".
            CXCursor baseDecl =
                clang_getTypeDeclaration(clang_getCursorType(c));
            type->BaseQual = QualifiedName(baseDecl);
            type->BaseReflected =
                SpAnnotation(baseDecl).rfind("sp:class:", 0) == 0;
        }
        return CXChildVisit_Continue;
    }

    // SP_REFLECT() injects a static member SpReflectMembers — its presence marks
    // the type as intrusively reflectable (private access + dynamic resolution).
    if (kind == CXCursor_CXXMethod
        && Str(clang_getCursorSpelling(c)) == "SpReflectMembers")
    {
        type->HasReflectHook = true;
        return CXChildVisit_Continue;
    }

    if (kind == CXCursor_FieldDecl)
    {
        std::string ann = SpAnnotation(c);
        if (ann.rfind("sp:property:", 0) != 0)
            return CXChildVisit_Continue;

        PropertyInfo prop;
        prop.Name = Str(clang_getCursorSpelling(c));
        prop.TypeSpelling = Str(clang_getTypeSpelling(clang_getCursorType(c)));
        prop.Payload = Payload(ann);
        prop.Loc = LocationOf(c);
        prop.Private =
            clang_getCXXAccessSpecifier(c) == CX_CXXPrivate;

        // Extract getter=/setter= specifiers (consumed structurally, not emitted
        // as attributes). Their presence makes this an accessor property.
        for (auto& [key, value] : SplitAttrs(prop.Payload))
        {
            if (key == "getter")
                prop.Getter = Trim(value);
            else if (key == "setter")
                prop.Setter = Trim(value);
        }
        if ((prop.Getter.empty()) != (prop.Setter.empty()))
        {
            std::fprintf(stderr,
                "%s:%u:%u: error: SPROPERTY on '%s' has only one of getter/setter"
                " — accessor properties need both\n",
                prop.Loc.File.c_str(), prop.Loc.Line, prop.Loc.Col,
                prop.Name.c_str());
            ++type->Errors;
            return CXChildVisit_Continue;
        }

        // Accessor properties use public getter/setter, so a private backing
        // field needs no SP_REFLECT hook (no field pointer is formed).
        if (prop.IsAccessor())
            prop.Private = false;

        if (clang_Cursor_isBitField(c))
        {
            std::fprintf(
                stderr,
                "%s:%u:%u: error: SPROPERTY on bitfield '%s' is unsupported\n",
                prop.Loc.File.c_str(), prop.Loc.Line, prop.Loc.Col,
                prop.Name.c_str());
            ++type->Errors;
            return CXChildVisit_Continue;
        }

        type->Properties.push_back(prop);
    }

    return CXChildVisit_Continue;
}

CXChildVisitResult VisitTopLevel(CXCursor c, CXCursor, CXClientData data)
{
    auto* out = static_cast<std::vector<TypeInfo>*>(data);

    if (!IsFromMainFile(c))
        return CXChildVisit_Recurse; // descend into main-file namespaces

    CXCursorKind kind = clang_getCursorKind(c);
    if (kind == CXCursor_Namespace)
        return CXChildVisit_Recurse;

    const bool isRecord =
        kind == CXCursor_StructDecl || kind == CXCursor_ClassDecl;
    const bool isEnum = kind == CXCursor_EnumDecl;
    if (!isRecord && !isEnum)
        return CXChildVisit_Continue;
    if (!clang_isCursorDefinition(c))
        return CXChildVisit_Continue;

    std::string ann = SpAnnotation(c);
    const bool wantRecord = ann.rfind("sp:class:", 0) == 0;
    const bool wantEnum = ann.rfind("sp:enum:", 0) == 0;
    if (!wantRecord && !wantEnum)
        return CXChildVisit_Continue;

    TypeInfo t;
    t.QualName = QualifiedName(c);
    t.Kind = isEnum ? "enum"
                    : (kind == CXCursor_ClassDecl ? "class" : "struct");
    t.Payload = Payload(ann); // SCLASS(...) args -> type-level attributes
    t.Loc = LocationOf(c);

    if (isEnum)
    {
        t.EnumQual = t.QualName;
        clang_visitChildren(
            c,
            [](CXCursor e, CXCursor, CXClientData d) -> CXChildVisitResult
            {
                if (clang_getCursorKind(e) == CXCursor_EnumConstantDecl)
                    static_cast<TypeInfo*>(d)->Enumerators.push_back(
                        Str(clang_getCursorSpelling(e)));
                return CXChildVisit_Continue;
            },
            &t);
    }
    else
    {
        clang_visitChildren(c, VisitRecordMember, &t);
    }

    out->push_back(std::move(t));
    return CXChildVisit_Continue;
}

// --- emission ---

std::string AnyExpr(const std::string& value)
{
    std::string v = Trim(value);
    if (!v.empty() && v.front() == '"') // string literal -> std::string
        return "::Seraph::Any(std::string(" + v + "))";
    return "::Seraph::Any(" + v + ")";
}

void EmitProperty(std::ofstream& os, const TypeInfo& t, const PropertyInfo& p)
{
    if (p.IsAccessor())
        os << "    .Property<&" << t.QualName << "::" << p.Getter << ", &"
           << t.QualName << "::" << p.Setter << ">(\"" << p.Name << "\")\n";
    else
        os << "    .Property<&" << t.QualName << "::" << p.Name << ">(\""
           << p.Name << "\")\n";

    for (auto& [key, value] : SplitAttrs(p.Payload))
    {
        if (value.empty())
            continue; // bare flag with no value — skipped for now
        if (key == "getter" || key == "setter")
            continue; // structural specifiers, not attributes
        os << "        .Attr(::Seraph::AttributeKey(\"" << key << "\"), "
           << AnyExpr(value) << ")\n";
    }
}

// Returns false on a loud failure (already reported).
bool EmitType(std::ofstream& os, const TypeInfo& t)
{
    if (t.Kind == "enum")
    {
        os << "SP_REFLECT_ENUM(" << t.EnumQual << ")\n";
        for (const std::string& e : t.Enumerators)
            os << "    .Value(\"" << e << "\", " << t.EnumQual << "::" << e
               << ")\n";
        os << "SP_REFLECT_ENUM_END();\n\n";
        return true;
    }

    bool hasPrivate = false;
    for (const PropertyInfo& p : t.Properties)
        hasPrivate |= p.Private;

    if (hasPrivate && !t.HasReflectHook)
    {
        std::fprintf(stderr,
                     "%s:%u:%u: error: '%s' has a private SPROPERTY but no "
                     "SP_REFLECT(%s) in the class body\n",
                     t.Loc.File.c_str(), t.Loc.Line, t.Loc.Col,
                     t.QualName.c_str(), t.QualName.c_str());
        return false;
    }

    const bool intrusive = t.HasReflectHook;
    if (intrusive)
        os << "SP_REFLECT_IMPL(" << t.QualName << ")\n";
    else
        os << "SP_REFLECT_TYPE(" << t.QualName << ")\n";

    // Class-level directives from SCLASS(...). `dynamic` is a STRUCTURAL flag
    // (like getter/setter on a property): it emits .Dynamic() to enable
    // polymorphic type resolution through a virtual GetType() — for a reflected
    // root such as ScriptableEntity. Everything else is a key = value type
    // attribute, emitted before any property/base so TypeBuilder::Attr attaches it
    // to the TYPE (it targets the last-added property once one exists). e.g.
    // SCLASS(dynamic) or SCLASS(script.name = "Foo"). Bare value-less keys other
    // than `dynamic` are skipped.
    bool dynamic = false;
    for (auto& [key, value] : SplitAttrs(t.Payload))
    {
        if (key == "dynamic")
        {
            dynamic = true;
            continue;
        }
        if (value.empty())
            continue;
        os << "    .Attr(::Seraph::AttributeKey(\"" << key << "\"), "
           << AnyExpr(value) << ")\n";
    }
    if (dynamic)
        os << "    .Dynamic()\n";

    if (!t.BaseQual.empty() && t.BaseReflected)
        os << "    .Base<" << t.BaseQual << ">()\n";

    for (const PropertyInfo& p : t.Properties)
        EmitProperty(os, t, p);

    if (intrusive)
        os << "SP_REFLECT_IMPL_END(" << t.QualName << ")\n\n";
    else
        os << "SP_REFLECT_END();\n\n";
    return true;
}

bool Emit(const std::vector<TypeInfo>& types, const std::string& outPath,
          const std::string& includePath)
{
    std::ofstream os(outPath);
    if (!os)
    {
        std::fprintf(stderr, "error: cannot open '%s' for writing\n",
                     outPath.c_str());
        return false;
    }

    os << "// GENERATED by SeraphHeaderTool — do not edit.\n";
    os << "#include \"" << includePath << "\"\n";
    os << "#include \"Seraph/Reflection/Reflect.h\"\n";
    os << "#include \"Seraph/Reflection/Reflection.h\"\n\n";
    os << "#include <string>\n\n";

    // Drift guard: every discovered annotated type MUST produce a registration.
    // If any EmitType fails (unsupported construct, private without SP_REFLECT),
    // the whole generation fails -> the build fails. This is what guarantees
    // "annotation count == registration count" (reflection-plan.md example E).
    int emitted = 0;
    for (const TypeInfo& t : types)
    {
        if (!EmitType(os, t))
            return false;
        ++emitted;
    }

    std::printf("SeraphHeaderTool: %zu annotated type(s) -> %d registration(s) "
                "-> %s\n",
                types.size(), emitted, outPath.c_str());
    return true;
}

// Write a Make-style depfile (gcc -MMD format) listing the main header plus every
// transitively-included file as prerequisites of the generated output, so the
// build regenerates when any of them changes. See reflection-plan.md example E.
void WriteDepfile(CXTranslationUnit tu, const std::string& depPath,
                  const std::string& outPath, const std::string& mainHeader)
{
    std::vector<std::string> files;
    files.push_back(mainHeader);
    clang_getInclusions(
        tu,
        [](CXFile included, CXSourceLocation*, unsigned, CXClientData cd)
        {
            static_cast<std::vector<std::string>*>(cd)->push_back(
                Str(clang_getFileName(included)));
        },
        &files);

    std::ofstream os(depPath);
    if (!os)
    {
        std::fprintf(stderr, "warning: cannot write depfile '%s'\n",
                     depPath.c_str());
        return;
    }
    auto escape = [](const std::string& p)
    {
        std::string e;
        for (char c : p)
        {
            if (c == ' ')
                e += '\\';
            e += c;
        }
        return e;
    };
    os << escape(outPath) << ":";
    for (const std::string& f : files)
        os << " \\\n  " << escape(f);
    os << "\n";
}

void Dump(const std::vector<TypeInfo>& types)
{
    std::printf("== discovered %zu reflected type(s) ==\n", types.size());
    for (const TypeInfo& t : types)
    {
        std::printf("\n%s %s", t.Kind.c_str(), t.QualName.c_str());
        if (!t.BaseQual.empty())
            std::printf(" : %s%s", t.BaseQual.c_str(),
                        t.BaseReflected ? " (reflected)" : "");
        if (t.HasReflectHook)
            std::printf("  [intrusive]");
        std::printf("\n");
        for (const std::string& e : t.Enumerators)
            std::printf("    - %s\n", e.c_str());
        for (const PropertyInfo& p : t.Properties)
            std::printf("    . %s %s%s%s\n", p.TypeSpelling.c_str(),
                        p.Name.c_str(), p.Private ? " (private)" : "",
                        p.Payload.empty() ? ""
                                          : (" {" + p.Payload + "}").c_str());
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr,
                     "usage: %s <header.h> [-o <out.gen.cpp>] "
                     "[--include <inc>] [-- <clang args>]\n",
                     argv[0]);
        return 2;
    }

    const char* header = argv[1];
    std::string outPath;
    std::string depPath;
    std::string includePath = header;

    std::vector<const char*> clangArgs = {
        "-x", "c++", "-std=c++20", "-DSP_SHT_PARSE", "-ferror-limit=0",
    };
    for (int i = 2; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc)
            outPath = argv[++i];
        else if (a == "--include" && i + 1 < argc)
            includePath = argv[++i];
        else if (a == "--depfile" && i + 1 < argc)
            depPath = argv[++i];
        else if (a == "--")
            continue;
        else
            clangArgs.push_back(argv[i]);
    }

    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit tu = nullptr;
    CXErrorCode err = clang_parseTranslationUnit2(
        index, header, clangArgs.data(), static_cast<int>(clangArgs.size()),
        nullptr, 0, CXTranslationUnit_SkipFunctionBodies, &tu);
    if (err != CXError_Success || !tu)
    {
        std::fprintf(stderr, "%s: error: failed to parse (clang error %d)\n",
                     header, static_cast<int>(err));
        clang_disposeIndex(index);
        return 1;
    }

    int fatal = 0;
    unsigned n = clang_getNumDiagnostics(tu);
    for (unsigned i = 0; i < n; ++i)
    {
        CXDiagnostic d = clang_getDiagnostic(tu, i);
        if (clang_getDiagnosticSeverity(d) >= CXDiagnostic_Error)
        {
            std::fprintf(stderr, "%s\n",
                         Str(clang_formatDiagnostic(
                                 d, clang_defaultDiagnosticDisplayOptions()))
                             .c_str());
            ++fatal;
        }
        clang_disposeDiagnostic(d);
    }
    if (fatal > 0)
    {
        std::fprintf(stderr, "%s: error: %d parse error(s); aborting\n", header,
                     fatal);
        clang_disposeTranslationUnit(tu);
        clang_disposeIndex(index);
        return 1;
    }

    std::vector<TypeInfo> types;
    clang_visitChildren(clang_getTranslationUnitCursor(tu), VisitTopLevel,
                        &types);

    int parseErrors = 0;
    for (const TypeInfo& t : types)
        parseErrors += t.Errors;

    int rc = 0;
    if (outPath.empty())
    {
        Dump(types);
    }
    else if (!Emit(types, outPath, includePath))
    {
        rc = 1;
    }
    else if (!depPath.empty())
    {
        // Emit succeeded — record transitive includes for incremental rebuilds.
        WriteDepfile(tu, depPath, outPath, header);
    }

    if (parseErrors > 0)
    {
        std::fprintf(stderr,
                     "SeraphHeaderTool: %d unsupported-construct error(s)\n",
                     parseErrors);
        rc = 1;
    }

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);
    return rc;
}
