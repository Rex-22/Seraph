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
#include "Seraph/Core/Ref.h"
#include "Seraph/Reflection/Reflection.h"
#include "Seraph/Settings/SettingDescriptor.h"

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Seraph
{

class ISettingsStore;

// Fluent configurator returned by Settings::Register. Mutates a
// registry-owned descriptor in place (pointer stays stable). Chain then discard.
class SettingBuilder
{
public:
    explicit SettingBuilder(SettingDescriptor* d) : m_D(d) {}

    // BOUND to a live field. The owning struct need not be reflected; the value
    // type does (primitives/glm/AssetHandle are always registered; an enum must be
    // reflected via SP_REFLECT_ENUM so its labels resolve).
    template<class T>
    SettingBuilder& Bind(T* field)
    {
        m_D->IsBound = true;
        m_D->ValueType = Reflection::TryGet<T>();
        if constexpr (std::is_enum_v<T>)
        {
            // Enum values cross the Any boundary as s64 (the reflection enum
            // convention), so the console/YAML paths can map value <-> label.
            m_D->BoundGet = [field] { return Any(static_cast<s64>(*field)); };
            m_D->BoundSet = [field](const Any& v)
            {
                if (const s64* i = v.template Cast<s64>())
                    *field = static_cast<T>(*i);
                else if (const T* t = v.template Cast<T>())
                    *field = *t;
            };
        }
        else
        {
            m_D->BoundGet = [field] { return Any(*field); };
            m_D->BoundSet = [field](const Any& v)
            {
                if (const T* t = v.template Cast<T>())
                    *field = *t;
            };
        }
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
        // Enums are carried as s64 to match the bound-getter convention above.
        Any boxed = [&]
        {
            if constexpr (std::is_enum_v<T>)
                return Any(static_cast<s64>(value));
            else
                return Any(value);
        }();
        m_D->DefaultValue = boxed;
        if (!m_D->IsBound)
            m_D->OwnedValue = std::move(boxed);
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
    // Hint a vec3/vec4 value to render as a color picker.
    SettingBuilder& AsColor()
    {
        m_D->Attrs.Set(Setting::Attr::Color, Any(true));
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

    // --- Lifecycle (Settings 5) ---
    // Install the default YAML backend. Call after FileSystem::Init. Does NOT
    // load yet — engine settings register first, then LoadEngineUser applies
    // their persisted values.
    static void Init();
    // Save dirty scopes and clear the registry. Call before FileSystem::Shutdown.
    static void Shutdown();
    // Override the backend (asset/server store later, or a test store).
    static void InstallStore(Ref<ISettingsStore> store);

    // Load Engine + User scopes (no project yet) + apply --set overrides.
    static void LoadEngineUser();
    // Load the Project scope (call after the project root + game module are set).
    static void LoadProject();
    // Save every dirty scope via the installed backend.
    static void SaveDirty();
    // Remove all settings whose key starts with `prefix` (e.g. "game." on script
    // module unload, so a reload re-registers cleanly).
    static void PurgeByPrefix(std::string_view prefix);

    // Register (or return the existing builder for) a setting by key. Keys are
    // namespaced by convention: engine.* / editor.* / game.*.
    static SettingBuilder Register(std::string key);

    static const SettingDescriptor* Find(std::string_view key);
    static const std::vector<SettingDescriptor*>& All();

    // Type-erased get/set. SetAny is a USER EDIT: it marks the setting's scope
    // dirty (unless the setting is CLI-overridden, which is never persisted).
    static Any GetAny(std::string_view key);
    static void SetAny(std::string_view key, const Any& value);

    // --- Override chain + dirty tracking (Settings 2) ---
    //
    // Load precedence (low -> high), applied by the store (Settings 3) + lifecycle
    // (Settings 5); each later layer overwrites the current value for keys it has:
    //   Engine -> Engine.<platform> -> Project -> Project.<platform>
    //          -> User -> User.<platform> -> CLI(--set)
    //
    // ApplyLoaded sets a value WITHOUT dirtying (it came from disk, not a user
    // edit). `loadedFrom` records the source tier so a scope reload can't clobber
    // a live console override. Type-checked like SetAny; ignores unknown keys.
    static void ApplyLoaded(std::string_view key, const Any& value,
                            SettingScope loadedFrom);

    // Parse and apply `--set key=value` overrides from the command line. These
    // are highest precedence and are NEVER persisted (the setting is flagged
    // CLI-overridden, excluding it from saves and from further dirtying).
    static void ApplyCommandLineOverrides();
    static bool IsCommandLineOverridden(std::string_view key);

    // Per-scope dirty tracking: SetAny marks a scope dirty; the store saves only
    // dirty scopes, then clears them.
    static bool IsScopeDirty(SettingScope scope);
    static void MarkScopeDirty(SettingScope scope);
    static void ClearScopeDirty(SettingScope scope);

    // --- Store orchestration (Settings 3) ---
    // Load every scope in precedence order (Engine -> platform -> Project ->
    // platform -> User -> platform), then apply --set overrides.
    static void LoadAll(ISettingsStore& store);
    // Save each dirty scope and clear its dirty flag.
    static void SaveDirty(ISettingsStore& store);

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

    // --- Change notification + validation (Settings 4) ---
    //
    // A subscriber fires after a value actually changes (old != new) via SetAny
    // or ApplyLoaded. RequiresRestart settings persist the new value but do NOT
    // fire live (they take effect next launch) and flip IsRestartPending().
    using SettingChangeFn = std::function<void(
        const SettingDescriptor& desc, const Any& oldValue, const Any& newValue)>;
    using SubscriptionToken = u64;

    // Per-key subscription; token can be passed to Unsubscribe.
    static SubscriptionToken Subscribe(std::string_view key, SettingChangeFn fn);
    // Global subscription: fires for every setting change.
    static SubscriptionToken SubscribeAll(SettingChangeFn fn);
    static void Unsubscribe(SubscriptionToken token);

    // True once a RequiresRestart setting has been edited this session.
    static bool IsRestartPending();

    // Reset a setting to its registered default (fires notification).
    static void ResetToDefault(std::string_view key);

    // Drop all registrations (tests; later, module-scoped purge in Settings 5).
    static void Clear();

private:
    struct Registry;
    static Registry& Store();

    // Core mutation: priority-check -> clamp -> type-check -> change-detect ->
    // write -> (dirty) -> notify. RequiresRestart runtime edits persist but don't
    // fire live. `source` records/enforces SetBy precedence.
    static void ApplyChange(const SettingDescriptor* d, const Any& value,
                            bool markDirty, bool isRuntimeEdit,
                            SettingSetBy source);
    static void FireNotify(const SettingDescriptor& d, const Any& oldValue,
                           const Any& newValue);
};

} // namespace Seraph
