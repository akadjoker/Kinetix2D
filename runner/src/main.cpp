#include <k2d/Assets.h>
#include <k2d/AudioEngine.h>
#include <k2d/CameraComponent.h>
#include <k2d/CollisionLayers.h>
#include <k2d/CanvasRenderer.h>
#include <k2d/Device.h>
#include <k2d/OpenGL.h>
#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/GameObject.h>
#include <k2d/GameViewport.h>
#include <k2d/InputActionMap.h>
#include <k2d/KPak.h>
#include <k2d/Physics2DSerializer.h>
#include <k2d/MouseCursor.h>
#include <k2d/Profiler.h>
#include <k2d/RenderQueue.h>
#include <k2d/RenderTarget.h>
#include <k2d/Scene.h>
#include <k2d/ScreenFade.h>
#include <k2d/SceneManager.h>
#include <k2d/Serializer.h>
#include <k2d/ZenDebugger.h>
#include <k2d/ZenScriptComponent.h>
#include <k2d/ZenRuntime.h>

#include <k2d/UiControls.h>
#include <k2d/UiTheme.h>
#include <k2d/UserData.h>
#include <k2d/VirtualPad.h>

#include <platform.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include <ct/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ct/string.hpp>

namespace
{
struct PhysicsConfig
{
    Math::Vec2 gravity = Math::Vec2(0.0f, 980.0f);
    float fixedTimeStep = 1.0f / 60.0f;
    int velocityIterations = 8;
    bool treeBroadphase = true;
};

// On Android the window is the whole screen in physical pixels, not the size the
// project asked for, so a camera that follows the window (resolution 0) gets the
// project's screen as its resolution, scaled to fill the display.
k2d::GameViewport gameViewportFor(const k2d::CameraComponent* camera, float outputWidth, float outputHeight,
                                  float displayWidth, float displayHeight)
{
    float width = camera ? camera->viewportWidth() : 0.0f;
    float height = camera ? camera->viewportHeight() : 0.0f;
#if defined(__ANDROID__)
    if (width <= 0.0f || height <= 0.0f)
    {
        width = displayWidth;
        height = displayHeight;
    }
#else
    (void)displayWidth;
    (void)displayHeight;
#endif
    if (!camera)
        return k2d::CalculateGameViewport(outputWidth, outputHeight, width > 0.0f ? width : outputWidth,
                                          height > 0.0f ? height : outputHeight);
    return k2d::CalculateGameViewport(outputWidth, outputHeight, width, height, camera->viewportScaleMode(),
                                      camera->integerScale());
}

void setGlViewport(const k2d::GameViewport& viewport)
{
    glViewport(static_cast<int>(std::lround(viewport.x)), static_cast<int>(std::lround(viewport.y)),
               static_cast<int>(std::lround(viewport.width)), static_cast<int>(std::lround(viewport.height)));
}

k2d::GameViewport inputViewportFor(const k2d::GameViewport& renderViewport, float windowWidth, float windowHeight,
                                   float drawableWidth, float drawableHeight)
{
    k2d::GameViewport result = renderViewport;
    const float scaleX = drawableWidth > 0.0f ? windowWidth / drawableWidth : 1.0f;
    const float scaleY = drawableHeight > 0.0f ? windowHeight / drawableHeight : 1.0f;
    result.x *= scaleX;
    result.y *= scaleY;
    result.width *= scaleX;
    result.height *= scaleY;
    return result;
}

Math::Vec2 readVec2(const ct::Json& value, const Math::Vec2& fallback)
{
    if (!value.is_array() || value.size() < 2)
        return fallback;
    return Math::Vec2(static_cast<float>(value[0].as_double(fallback.x)),
                      static_cast<float>(value[1].as_double(fallback.y)));
}

ct::String joinPath(const ct::String& base, const char* relative)
{
    char buffer[4096];
    return path_join(buffer, sizeof buffer, base.c_str(), relative) ? ct::String(buffer) : ct::String(relative);
}

ct::String directoryOf(const char* path)
{
    char buffer[4096];
    path_directory(path, buffer, sizeof buffer);
    return ct::String(buffer);
}

ct::String projectFile(const ct::String& projectRoot, const char* name)
{
    return path_is_absolute(name) ? ct::String(name) : joinPath(projectRoot, name);
}

void preloadTextures(const ct::Json& node, k2d::Assets& assets, const ct::String& projectRoot)
{
    if (node.is_object())
    {
        const ct::Json::Object& members = node.members();
        for (size_t i = 0; i < members.size(); ++i)
        {
            const ct::String& key = members[i].key;
            const ct::Json& value = members[i].value;
            if ((key == "texture" || key == "normalMap") && value.is_string())
            {
                const char* name = value.as_cstr("");
                if (name[0] && !assets.GetTexture(name))
                {
                    assets.LoadTexture(name, projectFile(projectRoot, name).c_str(), true, false);
                }
            }
            else
                preloadTextures(value, assets, projectRoot);
        }
    }
    else if (node.is_array())
    {
        for (size_t i = 0; i < node.size(); ++i)
            preloadTextures(node[i], assets, projectRoot);
    }
}

bool loadJson(const char* path, ct::Json& out)
{
    k2d::FileBuffer buffer;
    if (!k2d::FileSystem::Instance().LoadFile(path, buffer, true))
        return false;
    ct::Json::Error error;
    out = ct::Json::parse(buffer.Text(), &error);
    return !error;
}

bool loadBytecodeManifest(const ct::String& projectRoot)
{
    const ct::String manifestPath = joinPath(projectRoot, ".k2d/web/scripts.json");
    if (!file_exists(manifestPath.c_str()))
        return true;

    ct::Json manifest;
    if (!loadJson(manifestPath.c_str(), manifest) ||
        std::strcmp(manifest["format"].as_cstr(""), "k2d-zen-bytecode-bundle") != 0 ||
        manifest["version"].as_int(0) != 1)
    {
        log_error("Invalid Zen bytecode manifest: %s\n", manifestPath.c_str());
        return false;
    }

    ct::String error;
    const ct::String bundlePath = joinPath(projectRoot, ".k2d/web/scripts.zbc");
    if (!k2d::ZenRuntime::instance().loadBytecodeBundle(bundlePath.c_str(), &error))
    {
        log_error("Could not load Zen bytecode bundle: %s\n", error.c_str());
        return false;
    }

    const ct::Json& scripts = manifest["scripts"];
    if (!scripts.is_array())
        return false;
    for (size_t i = 0; i < scripts.size(); ++i)
    {
        const ct::Json& script = scripts[i];
        const char* path = script["path"].as_cstr("");
        const char* className = script["class"].as_cstr("");
        if (!k2d::ZenRuntime::instance().registerBytecodeScript(path, className, &error))
        {
            log_error("Could not register Zen bytecode script '%s': %s\n", path, error.c_str());
            return false;
        }
    }
    return true;
}

// With no scene to play, the runner plays the pack beside it (the web page puts
// it at /data.kpak): unpacked where the game reads its files, from the project's
// startup scene. Audio opens files by path, so a mounted pack would not do.
bool unpackGame(ct::String& scenePath, ct::String& projectPath, ct::String& title)
{
#if defined(__EMSCRIPTEN__)
    const ct::String pack("/data.kpak");
#elif defined(__ANDROID__)
    // Inside the APK: copied out to the app's cache so KPak reads it as a file.
    char cache[4096];
    const ct::String pack = fs_get_temp_path(cache, sizeof cache) ? joinPath(cache, "data.kpak") : ct::String();
    std::size_t packSize = 0;
    std::uint8_t* packed = asset_read("data.kpak", &packSize);
    const bool copied = packed && !pack.empty() && file_write(pack.c_str(), packed, packSize);
    fs_free(packed);
    if (!copied)
    {
        log_error("The APK has no data.kpak\n");
        return false;
    }
#else
    char base[4096];
    const ct::String pack = fs_get_base_path(base, sizeof base) ? joinPath(base, "data.kpak") : ct::String("data.kpak");
#endif
    if (!file_exists(pack.c_str()))
        return false;
    k2d::KPak kpak;
    if (!kpak.Open(pack.c_str()))
    {
        log_error("Could not open %s\n", pack.c_str());
        return false;
    }
    ct::Vector<unsigned char> data;
    if (kpak.Read("project.k2dproj", data))
    {
        data.push_back(0);
        title = ct::Json::parse(reinterpret_cast<const char*>(data.data()))["name"].as_cstr("");
    }
#if defined(__EMSCRIPTEN__)
    const ct::String directory("/project");
#else
    // One folder per game, so two games never unpack over each other.
    char temp[4096];
    const ct::String folder = "kinetix2d-" + (title.empty() ? ct::String("game") : title);
    const ct::String directory =
        fs_get_temp_path(temp, sizeof temp) ? joinPath(temp, folder.c_str()) : folder;
#endif
    for (std::size_t i = 0; i < kpak.EntryCount(); ++i)
    {
        const char* name = kpak.EntryName(i);
        const ct::String target = joinPath(directory, name);
        if (!kpak.Read(name, data) || !fs_create_directory(directoryOf(target.c_str()).c_str()) ||
            !file_write(target.c_str(), data.data(), data.size()))
        {
            log_error("Could not unpack %s\n", name);
            return false;
        }
    }
    projectPath = joinPath(directory, "project.k2dproj");
    ct::Json project;
    const char* startup = loadJson(projectPath.c_str(), project) ? project["startupScene"].as_cstr("") : "";
    if (!startup[0])
    {
        log_error("The game has no startup scene\n");
        return false;
    }
    scenePath = joinPath(directory, startup);
    return true;
}

void applyRoot(k2d::Scene& scene, const ct::Json& rootJson, k2d::Assets& assets)
{
    k2d::GameObject& root = scene.root();
    root.setName(rootJson["name"].as_cstr("Scene"));
    root.setTag(rootJson["tag"].as_cstr(""));
    root.setActive(rootJson["active"].as_bool(true));
    root.setVisible(rootJson["visible"].as_bool(true));
    root.setLocked(rootJson["locked"].as_bool(false));
    root.setZIndex(static_cast<int>(rootJson["zIndex"].as_int(0)));
    root.setPosition(readVec2(rootJson["position"], Math::Vec2(0.0f)));
    root.setRotationDegrees(static_cast<float>(rootJson["rotation"].as_double(0.0)));
    root.setScale(readVec2(rootJson["scale"], Math::Vec2(1.0f)));

    const ct::Json& children = rootJson["children"];
    if (children.is_array())
        for (size_t i = 0; i < children.size(); ++i)
            k2d::Serializer::ReadObject(scene, children[i], &root, &assets);
}

PhysicsConfig loadPhysics(const char* projectPath)
{
    PhysicsConfig config;
    ct::Json project;
    if (!projectPath || !projectPath[0] || !loadJson(projectPath, project))
        return config;
    const ct::Json& physics = project["physics"];
    if (!physics.is_object())
        return config;
    config.gravity = readVec2(physics["gravity"], config.gravity);
    config.fixedTimeStep = static_cast<float>(physics["fixedTimeStep"].as_double(config.fixedTimeStep));
    config.velocityIterations = static_cast<int>(physics["velocityIterations"].as_int(config.velocityIterations));
    config.treeBroadphase = physics["treeBroadphase"].as_bool(config.treeBroadphase);
    const ct::Json& layers = physics["layers"];
    for (int layer = 0; layer < k2d::kCollisionLayers && layers.is_array() && layer < (int)layers.size(); ++layer)
        k2d::SetCollisionLayerName(layer, layers[layer].as_cstr(""));
    return config;
}

// The window the project asks for; 1280 x 720 without one.
void loadDisplay(const char* projectPath, int& width, int& height, bool& showFps)
{
    ct::Json project;
    if (!projectPath || !projectPath[0] || !loadJson(projectPath, project))
        return;
    const ct::Json& display = project["display"];
    if (!display.is_object())
        return;
    width = static_cast<int>(display["width"].as_int(width));
    height = static_cast<int>(display["height"].as_int(height));
    showFps = display["showFps"].as_bool(showFps);
}

// The project's "show FPS": frames per second and the frame time, top right.
void drawFpsCounter(k2d::CanvasRenderer& canvas, float width, float height, float fps)
{
    char text[48];
    std::snprintf(text, sizeof text, "%.0f FPS  %.1f ms", fps, fps > 0.0f ? 1000.0f / fps : 0.0f);
    k2d::RenderQueue overlay;
    k2d::RenderItem& item = overlay.AddItem(0);
    for (int pass = 0; pass < 2; ++pass)
    {
        k2d::RenderCommand command;
        command.type = k2d::RenderCommand::kText;
        command.x = width - 240.0f + (pass ? 0.0f : 1.5f);
        command.y = 10.0f + (pass ? 0.0f : 1.5f);
        command.width = 16.0f;
        command.color = pass ? k2d::Color::White() : k2d::Color(0.0f, 0.0f, 0.0f, 0.8f);
        command.text = text;
        item.commands.push_back(command);
    }
    canvas.SetOrtho(width, height);
    overlay.Flush(canvas);
}

void configureVirtualPad(const ct::Json& sceneJson, k2d::VirtualPad& pad)
{
    const ct::Json& config = sceneJson["virtualPad"];
    if (!config.is_object())
        return;

    pad.SetEnabled(config["enabled"].as_bool(pad.Enabled()));
    pad.SetScale(static_cast<float>(config["scale"].as_double(1.0)));
    pad.SetOpacity(static_cast<float>(config["opacity"].as_double(0.58)));
}

void configureDefaultInputActions()
{
    k2d::GetInputActions().BindDefaults();
}

void scriptOutput(const char* text, bool error, void*)
{
    static ct::String line;
    static bool lineIsError = false;
    if (!text)
        return;

    for (const char* c = text; *c; ++c)
    {
        if (*c == '\n')
        {
            log_message(lineIsError ? LOGLEVEL_ERROR : LOGLEVEL_INFO, "[script] %s", line.c_str());
            line.clear();
            lineIsError = false;
        }
        else
        {
            line += *c;
            lineIsError = lineIsError || error;
        }
    }
}

void drawProfilerOverlay(k2d::CanvasRenderer& canvas, float width, float height)
{
    k2d::RenderQueue overlay;
    k2d::RenderItem& item = overlay.AddItem(0);
    float y = 12.0f;
    char line[128];
    const k2d::Profiler& profiler = k2d::Profiler::Get();

    const auto addLine = [&](const char* text)
    {
        k2d::RenderCommand command;
        command.type = k2d::RenderCommand::kText;
        command.x = 12.0f;
        command.y = y;
        command.width = 14.0f;
        command.color = k2d::Color::White();
        command.text = text;
        item.commands.push_back(command);
        y += 18.0f;
    };
    const k2d::ProfileSample* samples = profiler.samples();
    const uint32_t sampleCount = profiler.sampleCount();
    const k2d::ProfileSample* frame = nullptr;
    for (uint32_t i = 0; i < sampleCount; ++i)
        if (std::strcmp(samples[i].name, "Frame") == 0)
        {
            frame = &samples[i];
            break;
        }

    const float frameMs = frame ? frame->display : profiler.frameMilliseconds();
    const float frameAverage = frame ? frame->average : frameMs;
    const float frameMaximum = frame ? frame->maximum : frameMs;
    std::snprintf(line, sizeof(line), "Profiler (F5): %.2f ms / %.1f FPS | avg %.2f | max %.2f (120 frames)", frameMs,
                  frameMs > 0.0f ? 1000.0f / frameMs : 0.0f, frameAverage, frameMaximum);
    addLine(line);

    int used = 0;
    line[0] = '\0';
    for (uint32_t i = 0; i < profiler.counterCount() && used < static_cast<int>(sizeof(line)); ++i)
        used += std::snprintf(line + used, sizeof(line) - static_cast<size_t>(used), "%s%s %llu", i ? " | " : "",
                              profiler.counters()[i].name,
                              static_cast<unsigned long long>(profiler.counters()[i].display));
    if (used > 0)
        addLine(line);

    const auto addSample = [&](const k2d::ProfileSample& sample)
    {
        std::snprintf(line, sizeof(line), "%s: %.2f ms | avg %.2f | max %.2f (%u calls)", sample.name, sample.display,
                      sample.average, sample.maximum, sample.displayCalls);
        addLine(line);
    };

    // VM entries are always shown first: they are the reason the F5 overlay
    // exists and can otherwise be pushed past the regular sample limit.
    for (uint32_t i = 0; i < sampleCount; ++i)
        if (std::strncmp(samples[i].name, "vm.", 3) == 0)
            addSample(samples[i]);

    uint32_t shown = 0;
    for (uint32_t i = 0; i < sampleCount && shown < 8; ++i)
    {
        if (std::strcmp(samples[i].name, "Frame") == 0 || std::strncmp(samples[i].name, "vm.", 3) == 0)
            continue;
        addSample(samples[i]);
        ++shown;
    }

    canvas.SetOrtho(width, height);
    overlay.Flush(canvas);
}
} // namespace

static int run(int argc, char** argv)
{
    // k2d_runner scene.k2dscene [project.k2dproj] [--debug=port]
    ct::Vector<ct::String> positional;
    int debugPort = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strncmp(argv[i], "--debug=", 8) == 0)
            debugPort = std::atoi(argv[i] + 8);
        else
            positional.push_back(argv[i]);
    }
    ct::String scenePath = positional.size() >= 1 ? positional[0] : ct::String();
    ct::String projectPath = positional.size() >= 2 ? positional[1] : ct::String();
    ct::String title;
    if (scenePath.empty() && !unpackGame(scenePath, projectPath, title))
    {
        std::fprintf(stderr, "Usage: k2d_runner <scene.k2dscene> [project.k2dproj], or data.kpak beside it\n");
        return 1;
    }
    const ct::String projectRoot = !projectPath.empty() ? directoryOf(projectPath.c_str())
                                                        : directoryOf(directoryOf(scenePath.c_str()).c_str());

    // The game lives in statics: on the web the browser calls the frame after
    // main has given its stack back.
    static k2d::Device device;
    static int displayWidth = 1280;
    static int displayHeight = 720;
    static bool showFps = false;
    loadDisplay(projectPath.c_str(), displayWidth, displayHeight, showFps);
    if (!device.Init(title.empty() ? "Kinetix2D Game" : title.c_str(), displayWidth, displayHeight, true))
        return 1;
    static k2d::UserData userData;
    const ct::String projectName(path_filename(projectRoot.c_str()));
    if (userData.open("Kinetix2D", projectName.empty() ? "Runner" : projectName.c_str()))
    {
        userData.load();
        device.SetDisplayIndex(userData.getInt("windowDisplayIndex", 0));
    }

    int result = 0;
    {
        k2d::FileSystem::Instance().AddSearchPath(joinPath(projectRoot, "assets").c_str(), true);
        k2d::FileSystem::Instance().AddSearchPath(projectRoot.c_str(), true);
        k2d::FileSystem::Instance().AddSearchPath(directoryOf(scenePath.c_str()).c_str());

        ct::Json sceneJson;
        if (!loadJson(scenePath.c_str(), sceneJson))
        {
            log_error("Could not load scene: %s\n", scenePath.c_str());
            device.Shutdown();
            return 1;
        }

        k2d::RegisterZenScriptSerializer();
        k2d::RegisterPhysics2DSerializers();
        static k2d::Assets assets;
        static k2d::Scene scene;
        static k2d::CanvasRenderer canvas;
        static k2d::RenderTarget gameTarget;
        // Held here, not per frame: Scene::setRenderCamera keeps the pointer
        // for the whole render pass, so it must outlive the frame body.
        static k2d::Camera2D defaultCamera;

        k2d::SetZenScriptInput(&device.GetInput());
        k2d::SetUiInput(&device.GetInput());
        k2d::SetZenScriptAssets(&assets);
        k2d::SetZenScriptUserData(&userData);
        k2d::SetZenScriptOutput(&scriptOutput, nullptr);
        k2d::SetZenScriptsEnabled(true);
        configureDefaultInputActions();
        if (debugPort > 0 && debugPort < 65536)
            k2d::GetZenDebugger().Connect(static_cast<std::uint16_t>(debugPort));
        if (!loadBytecodeManifest(projectRoot))
            result = 1;
        preloadTextures(sceneJson, assets, projectRoot);
        const ct::Json& rootJson = sceneJson["root"];
        if (!rootJson.is_object())
        {
            log_error("Invalid scene (missing root object): %s\n", scenePath.c_str());
            result = 1;
        }
        else if (result == 0 && !k2d::GetSceneManager().Load(scene, assets, scenePath.c_str()))
        {
            log_error("Could not load scene: %s\n", scenePath.c_str());
            result = 1;
        }

        PhysicsConfig physicsConfig = loadPhysics(projectPath.empty() ? nullptr : projectPath.c_str());
        const ct::Json& scenePhysics = sceneJson["physics"];
        if (scenePhysics.is_object())
            physicsConfig.gravity = readVec2(scenePhysics["gravity"], physicsConfig.gravity);
        scene.setGravity(physicsConfig.gravity);
        scene.setFixedTimeStep(physicsConfig.fixedTimeStep);
        k2d::RouteZenScriptCollisions(scene);
        k2d::RouteZenScriptAnimationEvents(scene);
        k2d::RouteZenScriptActionEvents(scene);
        k2d::RouteZenScriptParticleHits(scene);
        scene.setSimulationEnabled(true);
        scene.setVelocityIterations(physicsConfig.velocityIterations);
        scene.setTreeBroadphase(physicsConfig.treeBroadphase);

        const ct::Json& sceneCursor = sceneJson["cursor"];
        if (sceneCursor.is_object() && sceneCursor["enabled"].as_bool(false))
        {
            const char* cursorImage = sceneCursor["image"].as_cstr("");
            k2d::Texture* cursorTexture =
                cursorImage[0] ? assets.LoadTexture(cursorImage, projectFile(projectRoot, cursorImage).c_str()) : nullptr;
            if (!cursorTexture)
                log_error("Could not load cursor image: %s\n", cursorImage);

            k2d::MouseCursor& cursor = k2d::GetMouseCursor();
            cursor.setTexture(cursorTexture);
            cursor.setOffset(readVec2(sceneCursor["offset"], Math::Vec2(0.0f, 0.0f)));
            cursor.setEnabled(true);
            // The scene asked for its own cursor, so the OS pointer goes away
            // even when the image failed to load -- showing both is worse than
            // showing neither, and the error above says why it is missing.
            device.ShowCursor(false);
        }

        if (result == 0)
        {
            if (!canvas.Init())
            {
                log_error("Could not initialize the game renderer\n");
                result = 1;
            }
            else
            {
                k2d::SetUiThemeTexture(k2d::UiTheme::DefaultTexture(assets));
                if (!k2d::GetAudio().Init())
                    log_warn("Audio unavailable; continuing without sound\n");
                else
                    k2d::GetAudio().LoadSettings(userData);
                static k2d::VirtualPad virtualPad;
                virtualPad.SetTexture(k2d::VirtualPad::DefaultTexture(assets));
                virtualPad.SetKeyBindings((int)k2d::Key::Left, (int)k2d::Key::Right, (int)k2d::Key::Up, (int)k2d::Key::Down,
                                          (int)k2d::Key::Space, (int)k2d::Key::LeftCtrl);
                configureVirtualPad(sceneJson, virtualPad);
                k2d::SetZenScriptVirtualPad(&virtualPad);
                static bool profilerVisible = false;
                static const auto frame = []() -> bool
                {
                    if (!device.PollEvents())
                        return false;
                    k2d::Profiler::Get().beginFrame();
                    k2d::GetZenDebugger().Poll();
                    // F5, or three fingers on a screen with no keyboard.
                    static int lastTouches = 0;
                    const int touches = device.GetInput().TouchCount();
                    const bool threeFingers = touches >= 3 && lastTouches < 3;
                    lastTouches = touches;
                    if (device.GetInput().KeyPressed((int)k2d::Key::F5) || threeFingers)
                    {
                        profilerVisible = !profilerVisible;
                        k2d::SetZenScriptProfilerVisible(profilerVisible);
                        k2d::ZenRuntime::instance().setVmProfiling(profilerVisible);
                    }
                    if (device.GetInput().KeyPressed((int)k2d::Key::F6))
                    {
                        const std::size_t reloaded = k2d::ReloadChangedZenScripts();
                        if (reloaded)
                            log_info("Reloaded %zu saved script(s)", reloaded);
                        else
                            log_info("No saved script changes to reload");
                    }
                    const float deltaTime = device.DeltaTime();
                    k2d::GetScreenFade().Update(deltaTime);
                    k2d::GetSceneTransition().Update(deltaTime);
                    if (k2d::GetSceneManager().HasRequest())
                    {
                        if (k2d::GetSceneManager().ApplyRequest(scene, assets))
                            scene.setSimulationEnabled(true);
                        else
                            log_error("Could not load requested scene\n");
                    }
                    k2d::GetAudio().Update();
                    const float windowWidth = static_cast<float>(device.Width());
                    const float windowHeight = static_cast<float>(device.Height());
                    const float drawableWidth = static_cast<float>(device.DrawableWidth());
                    const float drawableHeight = static_cast<float>(device.DrawableHeight());
                    k2d::CameraComponent* camera = scene.activeCamera();
                    k2d::GameViewport renderViewport =
                        gameViewportFor(camera, drawableWidth, drawableHeight, static_cast<float>(displayWidth),
                                        static_cast<float>(displayHeight));
                    k2d::GameViewport inputViewport =
                        inputViewportFor(renderViewport, windowWidth, windowHeight, drawableWidth, drawableHeight);
                    if (camera)
                    {
                        camera->setRenderViewport(inputViewport.virtualWidth, inputViewport.virtualHeight);
                        k2d::SetZenScriptGameCamera(&camera->camera());
                    }
                    else
                        k2d::SetZenScriptGameCamera(&defaultCamera);
                    k2d::SetZenScriptGameViewport(inputViewport.x, inputViewport.y, inputViewport.width,
                                                  inputViewport.height, inputViewport.virtualWidth,
                                                  inputViewport.virtualHeight);
                    k2d::SetUiViewport(inputViewport.x, inputViewport.y, inputViewport.width, inputViewport.height,
                                       inputViewport.virtualWidth, inputViewport.virtualHeight);
                    virtualPad.Update(device.GetInput(), inputViewport.x, inputViewport.y, inputViewport.width,
                                      inputViewport.height, inputViewport.virtualWidth, inputViewport.virtualHeight,
                                      deltaTime);
                    k2d::SetZenScriptFrameStats(deltaTime, device.FPS());
                    scene.update(deltaTime);
                    k2d::DispatchZenScriptEvents(scene.root());

                    camera = scene.activeCamera();
                    renderViewport = gameViewportFor(camera, drawableWidth, drawableHeight,
                                                     static_cast<float>(displayWidth),
                                                     static_cast<float>(displayHeight));
                    inputViewport =
                        inputViewportFor(renderViewport, windowWidth, windowHeight, drawableWidth, drawableHeight);
                    if (camera)
                        camera->setRenderViewport(renderViewport.virtualWidth, renderViewport.virtualHeight);
                    k2d::SetZenScriptGameViewport(inputViewport.x, inputViewport.y, inputViewport.width,
                                                  inputViewport.height, inputViewport.virtualWidth,
                                                  inputViewport.virtualHeight);
                    k2d::SetUiViewport(inputViewport.x, inputViewport.y, inputViewport.width, inputViewport.height,
                                       renderViewport.virtualWidth, renderViewport.virtualHeight);
                    glViewport(0, 0, device.DrawableWidth(), device.DrawableHeight());
                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    const float virtualWidth = renderViewport.virtualWidth;
                    const float virtualHeight = renderViewport.virtualHeight;
                    // On Android the screen has several times the game's pixels and the
                    // lights pay per pixel: the frame is drawn at the game's resolution and
                    // scaled up to the screen.
                    const int targetWidth = static_cast<int>(std::lround(virtualWidth));
                    const int targetHeight = static_cast<int>(std::lround(virtualHeight));
#if defined(__ANDROID__)
                    const bool scaled = renderViewport.width > virtualWidth + 0.5f &&
                                        gameTarget.resize(targetWidth, targetHeight);
#else
                    const bool scaled = false;
#endif
                    if (scaled)
                    {
                        gameTarget.bind();
                        glViewport(0, 0, targetWidth, targetHeight);
                        glClear(GL_COLOR_BUFFER_BIT);
                    }
                    else
                        setGlViewport(renderViewport);
                    if (camera)
                    {
                        k2d::GetAudio().SetListenerPosition(camera->camera().position);
                        canvas.SetProjection(camera->projection());
                        k2d::SetZenScriptGameCamera(&camera->camera());
                        scene.setRenderCamera(&camera->camera(), virtualWidth, virtualHeight);
                    }
                    else
                    {
                        canvas.SetProjection(defaultCamera.Projection(virtualWidth, virtualHeight));
                        k2d::SetZenScriptGameCamera(&defaultCamera);
                        scene.setRenderCamera(&defaultCamera, virtualWidth, virtualHeight);
                    }
                    scene.render(canvas);
                    if (k2d::GetSceneManager().WantsCapture())
                        k2d::GetSceneTransition().Capture();
                    k2d::GetSceneTransition().Draw(canvas, virtualWidth, virtualHeight);
                    k2d::ZenRuntime::instance().submitProfilerSamples();
                    if (profilerVisible)
                        drawProfilerOverlay(canvas, virtualWidth, virtualHeight);
                    else if (showFps)
                        drawFpsCounter(canvas, virtualWidth, virtualHeight, device.FPS());
                    virtualPad.Draw(canvas, virtualWidth, virtualHeight);
                    k2d::GetScreenFade().Draw(canvas, virtualWidth, virtualHeight);
                    k2d::MouseCursor& cursor = k2d::GetMouseCursor();
                    if (cursor.enabled())
                    {
                        cursor.setPosition(Math::Vec2(inputViewport.toVirtualX(device.GetInput().MouseX()),
                                                     inputViewport.toVirtualY(device.GetInput().MouseY())));
                        cursor.draw(canvas, virtualWidth, virtualHeight);
                    }
                    if (scaled)
                    {
                        const int x = static_cast<int>(std::lround(renderViewport.x));
                        const int y = static_cast<int>(std::lround(renderViewport.y));
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, gameTarget.framebuffer());
                        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                        glBlitFramebuffer(0, 0, targetWidth, targetHeight, x, y,
                                          x + static_cast<int>(std::lround(renderViewport.width)),
                                          y + static_cast<int>(std::lround(renderViewport.height)),
                                          GL_COLOR_BUFFER_BIT,
                                          camera && camera->integerScale() ? GL_NEAREST : GL_LINEAR);
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    }
                    device.Swap();
                    k2d::Profiler::Get().endFrame();
                    return true;
                };
#if defined(__EMSCRIPTEN__)
                // The browser calls a frame at each repaint; this never returns.
                emscripten_set_main_loop_arg([](void*) { frame(); }, nullptr, 0, 1);
#else
                while (frame())
                {
                }
#endif
                scene.clear();
                assets.Clear();
                k2d::GetSceneTransition().Release();
                gameTarget.destroy();
                canvas.Shutdown();
                k2d::GetAudio().SaveSettings(userData);
                k2d::GetAudio().Shutdown();
            }
        }

        k2d::SetZenScriptsEnabled(false);
        k2d::SetZenScriptGameCamera(nullptr);
        k2d::SetZenScriptGameViewport(0.0f, 0.0f, 0.0f, 0.0f);
        k2d::SetUiViewport(0.0f, 0.0f, 0.0f, 0.0f);
        k2d::GetScreenFade().SetClear();
        k2d::SetZenScriptProfilerVisible(false);
        k2d::ZenRuntime::instance().setVmProfiling(false);
        k2d::SetZenScriptOutput(nullptr, nullptr);
        k2d::SetZenScriptUserData(nullptr);
        k2d::SetZenScriptAssets(nullptr);
        k2d::SetZenScriptInput(nullptr);
        k2d::SetZenScriptVirtualPad(nullptr);
        k2d::SetUiInput(nullptr);
    }
    userData.setInt("windowDisplayIndex", device.DisplayIndex());
    userData.save();
    device.Shutdown();
    return result;
}

#if defined(__ANDROID__)
// The platform's android_main calls main() with no arguments: the game is the
// data.kpak in the APK.
int main()
{
    char name[] = "k2d_runner";
    char* argv[] = {name, nullptr};
    return run(1, argv);
}
#else
int main(int argc, char** argv)
{
    return run(argc, argv);
}
#endif
