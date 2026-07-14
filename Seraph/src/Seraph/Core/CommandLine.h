//
// Parsed process command line. Initialized once from main() (EntryPoint.h) and
// queryable from anywhere — e.g. the editor's `--project <sproj>` /
// `--package <sproj>` flags and the runtime's optional `--project` override.
//

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Seraph
{

class CommandLine
{
public:
    static void Init(int argc, char** argv);

    // True if `flag` (e.g. "--package") is present anywhere in the args.
    static bool Has(std::string_view flag);
    // The token after `flag` (e.g. "--project foo.sproj" -> "foo.sproj"), or "".
    static std::string Get(std::string_view flag);

    static const std::vector<std::string>& Args();

private:
    static std::vector<std::string> s_Args;
};

} // namespace Seraph
