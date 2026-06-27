//
// Created by ruben on 2026/06/27.
//

#include "Entity.h"

namespace Seraph {

Entity::Entity(const entt::entity handle, Scene* scene): m_Handle(handle), m_Scene(scene) {}

} // Seraph