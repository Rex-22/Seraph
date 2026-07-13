//
// Shared YAML (de)serialization for material parameters and render state, used
// by both MaterialSerializer (.smaterial) and MaterialInstanceSerializer
// (.smatinst).
//

#pragma once

#include "Seraph/Graphics/Material/MaterialParameter.h"
#include "Seraph/Graphics/Material/MaterialRenderState.h"

#include <yaml-cpp/yaml.h>

namespace Seraph::MaterialYaml
{

void EmitParameter(YAML::Emitter& emitter, const MaterialParameter& param);
MaterialParameter ParseParameter(const YAML::Node& node);

void EmitRenderState(YAML::Emitter& emitter, const MaterialRenderState& state);
MaterialRenderState ParseRenderState(const YAML::Node& node);

} // namespace Seraph::MaterialYaml
