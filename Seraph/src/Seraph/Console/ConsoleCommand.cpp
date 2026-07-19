//
// ConsoleCommand registry implementation. Commands are heap-owned (unique_ptr) so
// the const ConsoleCommand* handed back stays stable. Function-local-static
// storage keeps SP_CONSOLE_COMMAND registration init-order safe.
//

#include "Seraph/Console/ConsoleCommand.h"

#include "Seraph/Console/AnyTextCodec.h"
#include "Seraph/Core/Log.h"

#include <cctype>
#include <memory>
#include <unordered_map>

namespace Seraph
{

namespace
{

struct Registry
{
    std::vector<std::unique_ptr<ConsoleCommand>> Owned;
    std::unordered_map<std::string, ConsoleCommand*> ByName; // key = lowercased name
    std::vector<const ConsoleCommand*> AllList;
};

Registry& Store()
{
    static Registry s;
    return s;
}

std::string Lower(std::string_view s)
{
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

} // namespace

const ConsoleCommand* ConsoleCommandRegistry::Register(ConsoleCommand cmd)
{
    Registry& r = Store();
    std::string key = Lower(cmd.Name);
    if (auto it = r.ByName.find(key); it != r.ByName.end())
    {
        SP_CORE_WARN_TAG("Console", "command '{}' registered more than once",
                         cmd.Name);
        return it->second;
    }

    auto owned = std::make_unique<ConsoleCommand>(std::move(cmd));
    ConsoleCommand* p = owned.get();
    r.Owned.push_back(std::move(owned));
    r.ByName.emplace(std::move(key), p);
    r.AllList.push_back(p);
    return p;
}

const ConsoleCommand* ConsoleCommandRegistry::Find(std::string_view name)
{
    Registry& r = Store();
    auto it = r.ByName.find(Lower(name));
    return it == r.ByName.end() ? nullptr : it->second;
}

const std::vector<const ConsoleCommand*>& ConsoleCommandRegistry::All()
{
    return Store().AllList;
}

void ConsoleCommandRegistry::Unregister(std::string_view name)
{
    Registry& r = Store();
    const std::string key = Lower(name);
    auto it = r.ByName.find(key);
    if (it == r.ByName.end())
        return;
    ConsoleCommand* p = it->second;
    r.ByName.erase(it);
    std::erase(r.AllList, p);
    std::erase_if(r.Owned, [p](const std::unique_ptr<ConsoleCommand>& c)
                  { return c.get() == p; });
}

void ConsoleCommandRegistry::Clear()
{
    Registry& r = Store();
    r.ByName.clear();
    r.AllList.clear();
    r.Owned.clear();
}

const ConsoleCommand* ConsoleCommandRegistry::RegisterMethod(
    const MethodInfo& method, std::function<void*()> target,
    std::string nameOverride)
{
    // Refuse to register a command we could never parse arguments for — surface it
    // now (at registration) rather than as a confusing failure at call time.
    for (const Type* pt : method.ParamTypes)
    {
        if (!AnyTextCodec::IsSupported(pt))
        {
            SP_CORE_WARN_TAG("Console",
                "reflected method '{}' has an unsupported parameter type '{}'; "
                "not registered as a command",
                method.Name, AnyTextCodec::TypeLabel(pt));
            return nullptr;
        }
    }

    ConsoleCommand cmd;
    if (!nameOverride.empty())
        cmd.Name = std::move(nameOverride);
    else if (const std::string* n = method.Attrs.Get<std::string>(CommandAttr::Name))
        cmd.Name = *n;
    else
        cmd.Name = std::string(method.Name);

    if (const std::string* h = method.Attrs.Get<std::string>(CommandAttr::Help))
        cmd.Description = *h;

    if (const bool* c = method.Attrs.Get<bool>(CommandAttr::Cheat); c && *c)
        cmd.Flags |= ConsoleCommandFlag_Cheat;

    cmd.ParamTypes = method.ParamTypes; // for argument-value autocomplete

    // Usage hint: "<type> <type> ...".
    for (const Type* pt : method.ParamTypes)
    {
        if (!cmd.Usage.empty())
            cmd.Usage += ' ';
        cmd.Usage += '<';
        cmd.Usage += AnyTextCodec::TypeLabel(pt);
        cmd.Usage += '>';
    }

    // The MethodInfo (and its owning Type) is stable for the type's registered
    // lifetime, so capturing the pointer is safe for engine types. (Game-module
    // types re-register on reload; a console command over one would be re-added by
    // the same registration pass.)
    const MethodInfo* m = &method;
    cmd.Invoke = [m, target = std::move(target)](const ConsoleCommandArgs& args)
    {
        if (args.Count() != m->ParamTypes.size())
        {
            SP_CONSOLE_LOG_ERROR("expected {} argument(s), got {}",
                                 m->ParamTypes.size(), args.Count());
            return;
        }
        std::vector<Any> values;
        values.reserve(args.Count());
        for (std::size_t i = 0; i < args.Count(); ++i)
        {
            std::optional<Any> parsed =
                AnyTextCodec::Parse(m->ParamTypes[i], args[i]);
            if (!parsed)
            {
                SP_CONSOLE_LOG_ERROR("argument {} ('{}') is not a valid {}", i + 1,
                                     args[i],
                                     AnyTextCodec::TypeLabel(m->ParamTypes[i]));
                return;
            }
            values.push_back(std::move(*parsed));
        }
        void* obj = target ? target() : nullptr;
        if (!obj)
        {
            SP_CONSOLE_LOG_ERROR("command has no target instance to run on");
            return;
        }
        Any ret = m->Invoke(obj, values.data(), values.size());
        if (!ret.IsEmpty())
            SP_CONSOLE_LOG_INFO("-> {}",
                                AnyTextCodec::Format(ret, m->ReturnType));
    };

    return Register(std::move(cmd));
}

} // namespace Seraph
