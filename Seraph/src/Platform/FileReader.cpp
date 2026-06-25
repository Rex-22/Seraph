//
// Created by Ruben on 2026/06/22.
//

#include "FileReader.h"

#include "Seraph/Core/Core.h"

#include <config.h>

bool Seraph::FileReader::open(
    const bx::FilePath& _filePath, bx::Error* _err)
{
    String filePath(ASSET_PATH);
    filePath.append(_filePath);
    return super::open(bx::FilePath(filePath), _err);
}
