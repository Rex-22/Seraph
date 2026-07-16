//
// v1 settings backend: YAML files via FileSystem, one flat map (key -> value) per
// scope, with an optional per-platform overlay sibling. Corrupt-file tolerant
// (a bad parse logs + is skipped, defaults survive). See settings-plan.md.
//
//   Engine  -> Root::Engine  EngineSettings.yaml  (+ EngineSettings.<Platform>.yaml)
//   Project -> Root::Project ProjectSettings.yaml (+ .<Platform>.yaml)
//   User    -> Root::User    UserSettings.yaml    (+ .<Platform>.yaml)
//

#pragma once

#include "Seraph/Settings/ISettingsStore.h"

namespace Seraph
{

class YamlSettingsStore : public ISettingsStore
{
public:
    bool Load(SettingScope scope, bool platformOverlay) override;
    bool Save(SettingScope scope) override;
};

} // namespace Seraph
