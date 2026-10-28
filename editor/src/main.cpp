#include "EditorWindow.h"
#include "Icons.h"

#include <k2d/FileSystem.h>

#include <igui_zen/ZenApp.hpp>
#include <platform.h>

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
    if (!app.usesOpenGL())
    {
        log_error("an OpenGL context is required");
        return 1;
    }
    k2d::FileSystem::Instance().Init();
    k2d::editor::Icons::load();

    k2d::editor::EditorWindow window;
    window.build(app.widgets(), app.window());
    if (argc >= 2 && !window.openScene(argv[1]))
        log_error("Could not open scene: %s", argv[1]);

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
