//
// See CommandLine.h.
//

#include "CommandLine.h"

#include <algorithm>

namespace Seraph
{

std::vector<std::string> CommandLine::s_Args;

void CommandLine::Init(int argc, char** argv)
{
    s_Args.assign(argv, argv + argc);
}

bool CommandLine::Has(std::string_view flag)
{
    return std::find(s_Args.begin(), s_Args.end(), flag) != s_Args.end();
}

std::string CommandLine::Get(std::string_view flag)
{
    for (std::size_t i = 0; i + 1 < s_Args.size(); ++i)
        if (s_Args[i] == flag)
            return s_Args[i + 1];
    return {};
}

const std::vector<std::string>& CommandLine::Args()
{
    return s_Args;
}

} // namespace Seraph
