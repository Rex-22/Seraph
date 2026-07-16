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

#elif defined(_WIN32) // Windows: CreateProcessW — no shell, matching POSIX.

#include <string>

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Seraph
{
namespace
{

std::wstring Widen(const std::string& utf8)
{
    if (utf8.empty())
        return {};
    const int len = MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), len);
    return wide;
}

// Quote one argument per the CommandLineToArgvW rules so the child sees exactly
// `arg` — the equivalent of passing a discrete argv entry to posix_spawn, with
// no shell interpretation.
void AppendQuotedArg(const std::wstring& arg, std::wstring& out, bool force)
{
    if (!force && !arg.empty()
        && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        out += arg;
        return;
    }

    out += L'"';
    for (auto it = arg.begin();; ++it) {
        unsigned backslashes = 0;
        while (it != arg.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }

        if (it == arg.end()) {
            out.append(backslashes * 2, L'\\'); // escape trailing backslashes
            break;
        }
        if (*it == L'"') {
            out.append(backslashes * 2 + 1, L'\\'); // escape backslashes + the quote
            out += *it;
        } else {
            out.append(backslashes, L'\\');
            out += *it;
        }
    }
    out += L'"';
}

} // namespace

ProcessResult RunProcess(const std::string& exePath, const std::vector<std::string>& args)
{
    ProcessResult result;

    const std::wstring exe = Widen(exePath);

    // argv[0] is the program itself, then each arg quoted independently.
    std::wstring cmdline;
    AppendQuotedArg(exe, cmdline, /*force=*/true);
    for (const std::string& a : args) {
        cmdline += L' ';
        AppendQuotedArg(Widen(a), cmdline, /*force=*/false);
    }
    std::vector<wchar_t> mutableCmd(cmdline.begin(), cmdline.end());
    mutableCmd.push_back(L'\0'); // CreateProcessW may write to this buffer

    // Pipe for the child's stdout+stderr; the write end is inheritable, the read
    // end is not (so the child never holds it open and we see EOF).
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &sa, 0))
        return result;
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = writeEnd;
    si.hStdError = writeEnd;

    PROCESS_INFORMATION pi = {};
    const BOOL ok = CreateProcessW(
        exe.c_str(), mutableCmd.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
        &si, &pi);

    // The parent never writes; close its copy so the read loop terminates once
    // the child exits.
    CloseHandle(writeEnd);

    if (!ok) {
        CloseHandle(readEnd);
        return result;
    }
    result.Launched = true;

    char buffer[4096];
    DWORD n = 0;
    while (ReadFile(readEnd, buffer, sizeof(buffer), &n, nullptr) && n > 0)
        result.Output.append(buffer, n);
    CloseHandle(readEnd);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    if (GetExitCodeProcess(pi.hProcess, &exitCode))
        result.ExitCode = static_cast<int>(exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return result;
}

} // namespace Seraph

#else // Last-resort fallback for platforms with no native backend.

#include <cstdlib>

namespace Seraph
{

// NOTE: unlike the POSIX/Windows backends this routes through a shell
// (std::system), so it does NOT capture output and args ARE shell-interpreted.
// Present only so the engine builds on exotic platforms; not a supported path.
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
