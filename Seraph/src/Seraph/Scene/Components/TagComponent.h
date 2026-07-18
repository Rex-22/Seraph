//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Reflection/Annotations.h"

#include <string>

namespace Seraph
{

struct SCLASS() TagComponent
{
    SPROPERTY()
    std::string Tag;
};

}