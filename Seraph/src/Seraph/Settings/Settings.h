//
// Settings — the central settings registry (static facade, like Reflection /
// FileSystem). The "brain": register settings, get/set by stable string key, and
// (later tickets) resolve the scope override chain, persist via ISettingsStore,
// and fire change notifications. Built on the reflection layer. See
// Todo/plans/settings-plan.md.
//
// Settings 1 scope: descriptor + facade + bind-vs-own + typed get/set + the
// settings attribute vocabulary. Override chain (2), stores (3), change-notify
// (4), lifecycle (5), and UI (6/7) come later.
//

#pragma once

#include "Seraph/Core/Assert.h"
#include "Seraph/Reflection/Reflection.h"
#include "Seraph/Settings/SettingDescriptor.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Seraph
{

// Fluent configurator returned by Settings::Register. Mutates a
// registry-owned descriptor in place (pointer stays stable). Chain then discard.
class SettingBuilder
{
public:
    explicit SettingBuilder(SettingDescriptor* d) : m_D(d) {}

    // BOUND to a live field. The owning struct need not be reflected; the value
    // type does (primitives/glm/AssetHandle are always registered).
    template<class T>
    SettingBuilder& Bind(T* field)
    {
        m_D->IsBound = true;
        m_D->ValueType = Reflection::TryGet<T>();
        m_D->BoundGet = [field] { return Any(*field); };
        m_D->BoundSet = [field](const Any& v)
        {
            if (const T* t = v.template Cast<T>())
                *field = *t;
        };
        return *this;
    }

    // BOUND to computed getter/setter (e.g. camera FOV round-trip).
    template<class T>
    SettingBuilder& Bind(std::function<T()> get, std::function<void(T)> set)
    {
        m_D->IsBound = true;
        m_D->ValueType = Reflection::TryGet<T>();
        m_D->BoundGet = [get] { return Any(get()); };
        m_D->BoundSet = [set](const Any& v)
        {
            if (const T* t = v.template Cast<T>())
                set(*t);
        };
        return *this;
    }

    // OWNED value cell (scripts / no C++ home). Also records the default.
    template<class T>
    SettingBuilder& Default(T value)
    {
        m_D->ValueType = Reflection::TryGet<T>();
        m_D->DefaultValue = Any(value);
        if (!m_D->IsBound)
            m_D->OwnedValue = Any(value);
        return *this;
    }

    SettingBuilder& Scope(SettingScope s)
    {
        m_D->Scope = s;
        return *this;
    }

    SettingBuilder& Section(std::string_view s)
    {
        m_D->Attrs.Set(Setting::Attr::Section, Any(std::string(s)));
        return *this;
    }
    SettingBuilder& Display(std::string_view s)
    {
        m_D->Attrs.Set(Setting::Attr::DisplayName, Any(std::string(s)));
        return *this;
    }
    SettingBuilder& Tooltip(std::string_view s)
    {
        m_D->Attrs.Set(Setting::Attr::Tooltip, Any(std::string(s)));
        return *this;
    }
    template<class T>
    SettingBuilder& Min(T v)
    {
        m_D->Attrs.Set(Setting::Attr::Min, Any(v));
        return *this;
    }
    template<class T>
    SettingBuilder& Max(T v)
    {
        m_D->Attrs.Set(Setting::Attr::Max, Any(v));
        return *this;
    }
    template<class T>
    SettingBuilder& Step(T v)
    {
        m_D->Attrs.Set(Setting::Attr::Step, Any(v));
        return *this;
    }
    SettingBuilder& Flags(u32 flags)
    {
        m_D->Attrs.Set(Setting::Attr::Flags, Any(flags));
        return *this;
    }

    [[nodiscard]] SettingDescriptor* Descriptor() const { return m_D; }

private:
    SettingDescriptor* m_D;
};

class Settings
{
public:
    Settings() = delete;

    // Register (or return the existing builder for) a setting by key. Keys are
    // namespaced by convention: engine.* / editor.* / game.*.
    static SettingBuilder Register(std::string key);

    static const SettingDescriptor* Find(std::string_view key);
    static const std::vector<SettingDescriptor*>& All();

    // Type-erased get/set.
    static Any GetAny(std::string_view key);
    static void SetAny(std::string_view key, const Any& value);

    // Typed convenience.
    template<class T>
    static T Get(std::string_view key)
    {
        Any a = GetAny(key);
        const T* v = a.Cast<T>();
        SP_CORE_ASSERT(v, "Settings::Get<T>('{}'): missing or wrong type", key);
        return v ? *v : T{};
    }

    template<class T>
    static bool TryGet(std::string_view key, T& out)
    {
        const SettingDescriptor* d = Find(key);
        if (!d)
            return false;
        Any a = d->Read();
        const T* v = a.Cast<T>();
        if (!v)
            return false;
        out = *v;
        return true;
    }

    template<class T>
    static void Set(std::string_view key, const T& value)
    {
        SetAny(key, Any(value));
    }

    // Drop all registrations (tests; later, module-scoped purge in Settings 5).
    static void Clear();

private:
    struct Registry;
    static Registry& Store();
};

} // namespace Seraph
