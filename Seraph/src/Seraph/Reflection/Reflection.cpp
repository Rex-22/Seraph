//
// Reflection registry implementation. Owns Type storage in a
// std::vector<unique_ptr<Type>> — the heap-allocated pointees keep stable
// addresses across vector growth (only the owning pointers move), so const Type*
// handed out by Register stay valid. Indexed by TypeId and by name, with an
// insertion-ordered list for enumeration. Built-in primitives register on first access.
//

#include "Seraph/Reflection/Reflection.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Core/UUID.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <unordered_map>

namespace Seraph
{

struct Reflection::Storage
{
    // Each Type is heap-owned via unique_ptr, so removing some entries (a module
    // purge) never moves the surviving Type objects — every const Type* handed
    // out stays valid. The vector holding the unique_ptrs may reallocate, but
    // that only moves the pointers, not the pointees.
    struct OwnedType
    {
        std::unique_ptr<Type> Ptr;
        ModuleTag Module;
    };

    std::vector<OwnedType> Owned;
    std::unordered_map<TypeId, const Type*> ById;
    std::unordered_map<std::string_view, const Type*> ByName;
    std::vector<const Type*> AllList;
    ModuleTag Current = k_EngineModule;

    Storage()
    {
        // Built-in primitives. Registered here (not via the public Register) so
        // first-touch of the registry always resolves the fundamentals.
        InsertPrimitive<bool>();
        InsertPrimitive<s32>();
        InsertPrimitive<u32>();
        InsertPrimitive<s64>();
        InsertPrimitive<u64>();
        InsertPrimitive<f32>();
        InsertPrimitive<f64>();
        InsertPrimitive<std::string>();
        InsertPrimitive<glm::vec2>();
        InsertPrimitive<glm::vec3>();
        InsertPrimitive<glm::vec4>();
        InsertPrimitive<UUID>(); // AssetHandle = UUID; reflectable asset refs
    }

    const Type* Insert(Type&& type)
    {
        if (auto it = ById.find(type.Id); it != ById.end())
        {
            const Type* existing = it->second;
            // Same id + same name => benign re-registration (e.g. a header's
            // registration TU pulled into two link units). Different name =>
            // FNV-1a collision: two distinct types cannot share an identity.
            // VERIFY (not ASSERT): a collision must trap even in release —
            // SP_CORE_ASSERT compiles out there, silently aliasing the second
            // type onto the first (wrong serialization key + wrong reflection).
            SP_CORE_VERIFY(existing->Name == type.Name,
                           "Reflection: TypeId collision between '{}' and '{}' "
                           "(both hash to {})",
                           existing->Name, type.Name, type.Id);
            SP_CORE_WARN_TAG("Reflection", "Type '{}' registered more than once",
                             type.Name);
            return existing;
        }

        auto owned = std::make_unique<Type>(std::move(type));
        Type* p = owned.get();
        Owned.push_back({std::move(owned), Current});
        ById.emplace(p->Id, p);
        ByName.emplace(p->Name, p);
        AllList.push_back(p);

        // Back-patch any earlier-registered property whose type is THIS type but
        // was unresolved at its own registration time. Static-init order across
        // TUs is undefined, so a component can register before the enum/struct it
        // references (e.g. RigidBodyComponent before its SHT-generated BodyType).
        for (OwnedType& o : Owned)
            for (Property& prop : o.Ptr->Properties)
                if (prop.PropType == nullptr && prop.PropTypeId != 0
                    && prop.PropTypeId == p->Id)
                    prop.PropType = p;

        return p;
    }

    void RebuildIndices()
    {
        ById.clear();
        ByName.clear();
        AllList.clear();
        for (const OwnedType& o : Owned)
        {
            const Type* p = o.Ptr.get();
            ById.emplace(p->Id, p);
            ByName.emplace(p->Name, p);
            AllList.push_back(p);
        }
    }

    template<class T>
    void InsertPrimitive()
    {
        Type t;
        t.Id = TypeIdOf<T>();
        t.Name = TypeName<T>();
        t.Kind = TypeKind::Primitive;
        t.Size = static_cast<u32>(sizeof(T));
        t.Align = static_cast<u32>(alignof(T));
        Insert(std::move(t));
    }
};

Reflection::Storage& Reflection::Store()
{
    // Function-local static: constructed on first use, immune to static-init
    // order across translation units (ScriptRegistry::Storage() pattern).
    static Storage s;
    return s;
}

const Type* Reflection::Resolve(TypeId id) noexcept
{
    Storage& s = Store();
    auto it = s.ById.find(id);
    return it == s.ById.end() ? nullptr : it->second;
}

const Type* Reflection::Resolve(std::string_view name) noexcept
{
    Storage& s = Store();
    auto it = s.ByName.find(name);
    return it == s.ByName.end() ? nullptr : it->second;
}

const std::vector<const Type*>& Reflection::All()
{
    return Store().AllList;
}

const Type* Reflection::Register(Type&& type)
{
    return Store().Insert(std::move(type));
}

ModuleTag Reflection::CurrentModule() noexcept
{
    return Store().Current;
}

void Reflection::SetCurrentModule(ModuleTag module) noexcept
{
    Store().Current = module;
}

void Reflection::ClearModule(ModuleTag module) noexcept
{
    Storage& s = Store();
    const std::size_t before = s.Owned.size();
    std::erase_if(s.Owned,
                  [module](const Storage::OwnedType& o)
                  { return o.Module == module; });
    if (s.Owned.size() != before)
    {
        s.RebuildIndices();
        SP_CORE_INFO_TAG("Reflection",
                         "Cleared module {} ({} types dropped, {} remain)",
                         module, before - s.Owned.size(), s.Owned.size());
    }
}

// Resolver hook used by Any::GetType() — declared in Any.h so that header stays
// free of any dependency on the registry (breaks the Any <-> Type <-> Reflection
// include cycle).
namespace Detail
{
const Type* ResolveTypeById(TypeId id) noexcept
{
    return Reflection::Resolve(id);
}
} // namespace Detail

} // namespace Seraph
