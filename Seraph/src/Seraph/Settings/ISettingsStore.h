//
// Storage backend for the settings registry. A store persists/loads one scope's
// file at a time; Settings::LoadAll / SaveDirty orchestrate the precedence order.
// Installed like AssetManagerBase (a Ref<> the registry holds). YamlSettingsStore
// is the v1 backend; an asset-backed or server backend can slot in later. See
// Todo/plans/settings-plan.md.
//

#pragma once

#include "Seraph/Core/Ref.h"
#include "Seraph/Settings/SettingDescriptor.h"

namespace Seraph
{

class ISettingsStore : public RefCounted
{
public:
    ~ISettingsStore() override = default;

    // Load one scope's values into the registry (via Settings::ApplyLoaded).
    // `platformOverlay` selects the per-platform sibling file. Missing file is a
    // silent success. Returns false only on a real read/parse error.
    virtual bool Load(SettingScope scope, bool platformOverlay) = 0;

    // Persist every setting whose Scope == `scope` to that scope's file.
    virtual bool Save(SettingScope scope) = 0;

    // False for read-only backends (e.g. a shipped engine-defaults store).
    [[nodiscard]] virtual bool SupportsWrite() const { return true; }
};

} // namespace Seraph
