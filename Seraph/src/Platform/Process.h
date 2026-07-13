//
// Run an external command-line tool and capture its combined stdout/stderr and
// exit code. Used by the editor's shader-cook step to invoke the shaderc tool.
// Blocking; call off the render-critical path.
//

#pragma once

#include <string>
#include <vector>

namespace Seraph
{

struct ProcessResult
{
    bool Launched = false; // false if the process could not be started
    int ExitCode = -1;     // valid when Launched is true
    std::string Output;    // combined stdout + stderr
};

// Run `exePath` with `args` (argv[1..]). Does not go through a shell, so paths
// with spaces need no quoting.
ProcessResult RunProcess(const std::string& exePath, const std::vector<std::string>& args);

} // namespace Seraph
