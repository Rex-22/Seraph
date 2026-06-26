//
// Created by Ruben on 2026/06/23.
//

#include "Layer.h"

#include <utility>

namespace Seraph
{
Layer::Layer(std::string name): m_DebugName(std::move(name))
{
}
}