#include "core/Application.h"


int main()
{
    auto app = Core::Application();
    app.Run();

    app.Cleanup();
    return 0;
}
