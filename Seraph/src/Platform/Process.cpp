#include "Platform/Process.h"

#include <vector>

#if defined(__APPLE__) || defined(__linux__)

#include <array>
#include <cstdio>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace Seraph
{

ProcessResult RunProcess(const std::string& exePath, const std::vector<std::string>& args)
{
    ProcessResult result;

    int pipeFds[2];
    if (pipe(pipeFds) != 0)
        return result;

    // Route the child's stdout+stderr to the pipe's write end.
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipeFds[0]);
    posix_spawn_file_actions_addclose(&actions, pipeFds[1]);

    // argv: exePath, args..., nullptr (execv needs mutable char*).
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(exePath.c_str()));
    for (const std::string& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    pid_t pid = 0;
    const int rc = posix_spawn(
        &pid, exePath.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipeFds[1]); // parent only reads

    if (rc != 0) {
        close(pipeFds[0]);
        return result;
    }
    result.Launched = true;

    std::array<char, 4096> buffer;
    ssize_t n = 0;
    while ((n = read(pipeFds[0], buffer.data(), buffer.size())) > 0)
        result.Output.append(buffer.data(), static_cast<std::size_t>(n));
    close(pipeFds[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) == pid && WIFEXITED(status))
        result.ExitCode = WEXITSTATUS(status);
    return result;
}

} // namespace Seraph

#else // Fallback (e.g. Windows): compose a quoted command and use std::system.

#include <cstdlib>

namespace Seraph
{

ProcessResult RunProcess(const std::string& exePath, const std::vector<std::string>& args)
{
    ProcessResult result;
    std::string cmd = "\"" + exePath + "\"";
    for (const std::string& a : args)
        cmd += " \"" + a + "\"";

    const int rc = std::system(cmd.c_str());
    result.Launched = rc != -1;
    result.ExitCode = rc;
    return result;
}

} // namespace Seraph

#endif
