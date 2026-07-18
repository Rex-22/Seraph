//
// Console — the developer console facade (static, like Settings / Reflection).
// Owns the runtime state the string evaluator needs: the cheats gate, the command
// history, and the panel's output-clear hook. Execute() is the single entry point
// for a typed line, whether from the console panel, an exec script, or code.
//
// A CVar is a Settings entry, so `set`/`get` route through the Settings registry
// (clamp, change hooks, dirty-tracking); commands come from ConsoleCommandRegistry.
//

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Seraph
{

class Console
{
public:
    Console() = delete;

    // Flush pending AutoCVar registrations and enable the console. Call after
    // Settings::Init + Settings::LoadEngineUser (so archived CVar values are
    // applied). Cheats default on in dev builds, off in a shipping (SP_DIST) build.
    static void Init();
    static void Shutdown();

    // Parse and run one line: echoes it, records it in history, then dispatches to
    // a command (typed-arg parse + cheat gate + invoke) or a CVar (no args prints
    // the value; args parse + Settings::SetAny, honouring ReadOnly + cheats). An
    // unrecognised name logs fuzzy "did you mean" suggestions. Safe to call before
    // Init (dispatch just finds nothing registered yet).
    static void Execute(std::string_view line);

    // --- Cheats gate ---
    static void SetCheatsEnabled(bool enabled);
    [[nodiscard]] static bool CheatsEnabled();

    // --- Command history (shared by the panel's Up/Down and the `history` cmd) ---
    [[nodiscard]] static const std::vector<std::string>& History();
    static void ClearHistory();

    // --- Output clear hook ---
    // The panel registers how to clear its output buffer; the `clear` built-in and
    // ClearOutput() call it. No-op if unset (e.g. headless).
    static void SetClearHandler(std::function<void()> fn);
    static void ClearOutput();
};

} // namespace Seraph
