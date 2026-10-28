#include "EditorWindow.h"
#include "Icons.h"

#include <k2d/FileSystem.h>
#include <k2d/OpenGL.h>

#include <igui_zen/ZenApp.hpp>
#include <platform.h>

#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
    ig::zen::App::Config config;
    config.title = "Kinetix2D Editor";
    config.width = 1600;
    config.height = 900;
    config.msaa = 0;
    config.showStats = false;

    ig::zen::App app;
    if (!app.create(config))
        return 1;
    if (!app.usesOpenGL() || !k2d::LoadOpenGL())
    {
        log_error("an OpenGL context is required");
        return 1;
    }
    k2d::FileSystem::Instance().Init();
    k2d::editor::Icons::load();

    k2d::editor::EditorWindow window;
    window.build(app.widgets(), app.window());
    // k2d_editor [scene.k2dscene | project.k2dproj] [--mcp[=port]]; with neither,
    // the last project opened.
    bool opened = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strncmp(argv[i], "--mcp", 5) == 0)
        {
            const int port = argv[i][5] == '=' ? std::atoi(argv[i] + 6) : 7777;
            window.setMcp(static_cast<std::uint16_t>(port > 0 && port < 65536 ? port : 7777));
        }
        else if (path_has_extension(argv[i], ".k2dproj"))
            opened = window.openProject(argv[i]) || opened;
        else if (window.openScene(argv[i]))
            opened = true;
        else
            log_error("Could not open scene: %s", argv[i]);
    }
    if (!opened)
        window.openLastProject();

    app.setEventHook([&window](const Event& event) { window.onPlatformEvent(event); });
    app.setFrameCallback([&app, &window](float deltaTime)
    {
        int width = 0, height = 0, pixelWidth = 0, pixelHeight = 0;
        window_get_size(app.window(), &width, &height);
        window_get_framebuffer_size(app.window(), &pixelWidth, &pixelHeight);
        window.frame(deltaTime, width > 0 ? static_cast<float>(pixelWidth) / width : 1.0f);
    });
    app.run();
    window.shutdown();
    k2d::editor::Icons::unload();
    k2d::FileSystem::Instance().Shutdown();
    return 0;
}
