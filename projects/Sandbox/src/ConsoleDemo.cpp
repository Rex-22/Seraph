//
// A few genuinely-useful console commands (quit / exit / echo) plus one demo CVar
// that proves change hooks (req 5.1): con.demoHook logs via its OnChanged handler
// whenever it is toggled from the console. Serves as the worked example the
// console docs point at; safe to trim once real gameplay commands exist.
//

#include "Seraph/Console/AutoCVar.h"
#include "Seraph/Console/ConsoleCommand.h"
#include "Seraph/Core/Application.h"
#include "Seraph/Core/Log.h"

#include <string>

using namespace Seraph;

namespace
{

// A bound CVar with a change hook: toggling it live-updates (here, logs). A real
// render CVar would push into a renderer setting the same way.
AutoCVar<bool> g_DemoHook{"con.demoHook", false, CVarFlag_None,
                          "Demo variable — logs to the console when it changes"};

struct DemoHookInstaller
{
    DemoHookInstaller()
    {
        g_DemoHook.OnChanged([](const bool& v)
                             { SP_CONSOLE_LOG_INFO("con.demoHook changed -> {}",
                                                   v ? "true" : "false"); });
    }
} g_DemoHookInstaller;

} // namespace

SP_CONSOLE_COMMAND("quit", "Exit the application",
    [](const ConsoleCommandArgs&) { Application::Instance().Close(); });

SP_CONSOLE_COMMAND("exit", "Exit the application",
    [](const ConsoleCommandArgs&) { Application::Instance().Close(); });

SP_CONSOLE_COMMAND("echo", "Print the arguments back to the console",
    [](const ConsoleCommandArgs& a)
    {
        std::string s;
        for (std::size_t i = 0; i < a.Count(); ++i)
        {
            if (i)
                s += ' ';
            s += a[i];
        }
        SP_CONSOLE_LOG_INFO("{}", s);
    });