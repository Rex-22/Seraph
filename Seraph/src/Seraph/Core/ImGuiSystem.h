//
// Created by Ruben on 2026/06/23.
//

#pragma once

namespace Seraph
{
class ImGuiSystem // TODO(Ruben):Add support for some sort of subsystem system /*: public SubSystem*/
{
public:
    ImGuiSystem();
    ~ImGuiSystem();

    void Begin();
    void End();
};

}