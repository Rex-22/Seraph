//
// Created by Ruben on 2026/06/22.
//

#pragma once

#include "bimg/bimg.h"
#include "bx/file.h"

namespace Platform
{
class FileReader : public bx::FileReader
{
    typedef bx::FileReader super;
public:
    bool open(const bx::FilePath& _filePath, bx::Error* _err) override;
};
}