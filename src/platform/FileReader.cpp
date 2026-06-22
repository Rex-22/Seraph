//
// Created by Ruben on 2026/06/22.
//

#include "FileReader.h"

#include "core/Core.h"

#include <config.h>

bool Platform::FileReader::open(
    const bx::FilePath& _filePath, bx::Error* _err)
{
    Core::String filePath(ASSET_PATH);
    filePath.append(_filePath);
    return super::open(bx::FilePath(filePath), _err);
}
