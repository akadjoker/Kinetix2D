#include <k2d/Assets.h>
#include <k2d/CameraComponent.h>
#include <k2d/CanvasRenderer.h>
#include <k2d/Device.h>
#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/GameObject.h>
#include <k2d/Physics2DSerializer.h>
#include <k2d/PhysicsWorld2D.h>
#include <k2d/Profiler.h>
#include <k2d/RenderQueue.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>
#include <k2d/ZenScriptComponent.h>
#include <k2d/ZenRuntime.h>

#include <glad/glad.h>
#include <SDL.h>

#include <ct/json.hpp>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace
{
struct PhysicsConfig
{
    Math::Vec2 gravity = Math::Vec2(0.0f, 980.0f);
    float fixedTimeStep = 1.0f / 60.0f;
    int velocityIterations = 8;
    bool treeBroadphase = true;
};

Math::Vec2 readVec2(const ct::Json &value, const Math::Vec2 &fallback)
{
    if (!value.is_array() || value.size() < 2)
        return fallback;
    return Math::Vec2(static_cast<float>(value[0].as_double(fallback.x)),
                      static_cast<float>(value[1].as_double(fallback.y)));
}

void preloadTextures(const ct::Json &node, k2d::Assets &assets)
{
    if (node.is_object())
    {
        const ct::Json::Object &members = node.members();
        for (size_t i = 0; i < members.size(); ++i)
        {
            const ct::String &key = members[i].key;
            const ct::Json &value = members[i].value;
            if ((key == "texture" || key == "normalMap") && value.is_string())
            {
                const char *path = value.as_cstr("");
                if (path[0] && !assets.GetTexture(path))
                    assets.LoadTexture(path, path, true, false);
            }
            else
                preloadTextures(value, assets);
        }
    }
    else if (node.is_array())
    {
        for (size_t i = 0; i < node.size(); ++i)
            preloadTextures(node[i], assets);
    }
}

bool loadJson(const char *path, ct::Json &out)
{
    k2d::FileBuffer buffer;
    if (!k2d::FileSystem::Instance().LoadFile(path, buffer, true))
        return false;
    ct::Json::Error error;
    out = ct::Json::parse(buffer.Text(), &error);
    return !error;
}

void applyRoot(k2d::Scene &scene, const ct::Json &rootJson, k2d::Assets &assets)
{
    k2d::GameObject &root = scene.root();
    root.setName(rootJson["name"].as_cstr("Scene"));
    root.setTag(rootJson["tag"].as_cstr(""));
    root.setActive(rootJson["active"].as_bool(true));
    root.setVisible(rootJson["visible"].as_bool(true));
    root.setLocked(rootJson["locked"].as_bool(false));
    root.setZIndex(static_cast<int>(rootJson["zIndex"].as_int(0)));
    root.setPosition(readVec2(rootJson["position"], Math::Vec2(0.0f)));
    root.setRotationDegrees(static_cast<float>(rootJson["rotation"].as_double(0.0)));
    root.setScale(readVec2(rootJson["scale"], Math::Vec2(1.0f)));

    const ct::Json &children = rootJson["children"];
    if (children.is_array())
        for (size_t i = 0; i < children.size(); ++i)
            k2d::Serializer::ReadObject(scene, children[i], &root, &assets);
}

PhysicsConfig loadPhysics(const char *projectPath)
{
    PhysicsConfig config;
    ct::Json project;
    if (!projectPath || !projectPath[0] || !loadJson(projectPath, project))
        return config;
    const ct::Json &physics = project["physics"];
    if (!physics.is_object())
        return config;
    config.gravity = readVec2(physics["gravity"], config.gravity);
    config.fixedTimeStep = static_cast<float>(physics["fixedTimeStep"].as_double(config.fixedTimeStep));
    config.velocityIterations = static_cast<int>(physics["velocityIterations"].as_int(config.velocityIterations));
    config.treeBroadphase = physics["treeBroadphase"].as_bool(config.treeBroadphase);
    return config;
}

void scriptOutput(const char *text, bool error, void *)
{
    static std::string line;
    static bool lineIsError = false;
    if (!text)
        return;

    for (const char *c = text; *c; ++c)
    {
        if (*c == '\n')
        {
            std::fputs(lineIsError ? "[script] ERROR: " : "[script] ", stderr);
            std::fputs(line.c_str(), stderr);
            std::fputc('\n', stderr);
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

void drawProfilerOverlay(k2d::CanvasRenderer &canvas, float width, float height)
{
    k2d::RenderQueue overlay;
    k2d::RenderItem &item = overlay.AddItem(0);
    float y = 12.0f;
    char line[128];
    const k2d::Profiler &profiler = k2d::Profiler::Get();

    const auto addLine = [&](const char *text)
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
    const k2d::ProfileSample *samples = profiler.samples();
    const uint32_t sampleCount = profiler.sampleCount();
    const k2d::ProfileSample *frame = nullptr;
    for (uint32_t i = 0; i < sampleCount; ++i)
        if (std::strcmp(samples[i].name, "Frame") == 0)
        {
            frame = &samples[i];
            break;
        }

    const float frameMs = frame ? frame->display : profiler.frameMilliseconds();
    const float frameAverage = frame ? frame->average : frameMs;
    const float frameMaximum = frame ? frame->maximum : frameMs;
    std::snprintf(line, sizeof(line),
                  "Profiler (F5): %.2f ms / %.1f FPS | avg %.2f | max %.2f (120 frames)",
                  frameMs, frameMs > 0.0f ? 1000.0f / frameMs : 0.0f,
                  frameAverage, frameMaximum);
    addLine(line);

    const auto addSample = [&](const k2d::ProfileSample &sample)
    {
        std::snprintf(line, sizeof(line), "%s: %.2f ms | avg %.2f | max %.2f (%u calls)",
                      sample.name, sample.display, sample.average, sample.maximum,
                      sample.displayCalls);
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
        if (std::strcmp(samples[i].name, "Frame") == 0 ||
            std::strncmp(samples[i].name, "vm.", 3) == 0)
            continue;
        addSample(samples[i]);
        ++shown;
    }

    canvas.SetOrtho(width, height);
    overlay.Flush(canvas);
}
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "Usage: k2d_runner <scene.k2dscene> [project.k2dproj]\n");
        return 1;
    }

    const std::filesystem::path scenePath(argv[1]);
    const std::filesystem::path projectRoot = argc >= 3
        ? std::filesystem::path(argv[2]).parent_path()
        : scenePath.parent_path().parent_path();

    k2d::Device device;
    if (!device.Init("Kinetix2D Game", 1280, 720, true, false))
        return 1;

    int result = 0;
    {
        k2d::FileSystem::Instance().AddSearchPath(projectRoot.string().c_str());
        k2d::FileSystem::Instance().AddSearchPath((projectRoot / "assets").string().c_str());
        k2d::FileSystem::Instance().AddSearchPath(scenePath.parent_path().string().c_str());

        ct::Json sceneJson;
        if (!loadJson(argv[1], sceneJson))
        {
            std::fprintf(stderr, "Could not load scene: %s\n", argv[1]);
            device.Shutdown();
            return 1;
        }

        k2d::RegisterZenScriptSerializer();
        k2d::RegisterPhysics2DSerializers();
        k2d::Assets assets;
        k2d::Scene scene;
        k2d::CanvasRenderer canvas;
        k2d::PhysicsWorld2D physics;

        k2d::SetZenScriptInput(&device.GetInput());
        k2d::SetZenScriptAssets(&assets);
        k2d::SetZenScriptOutput(&scriptOutput, nullptr);
        k2d::SetZenScriptsEnabled(true);
        preloadTextures(sceneJson, assets);
        const ct::Json &rootJson = sceneJson["root"];
        if (!rootJson.is_object())
        {
            std::fprintf(stderr, "Invalid scene (missing root object): %s\n", argv[1]);
            result = 1;
        }
        else
            applyRoot(scene, rootJson, assets);

        PhysicsConfig physicsConfig = loadPhysics(argc >= 3 ? argv[2] : nullptr);
        const ct::Json &scenePhysics = sceneJson["physics"];
        if (scenePhysics.is_object())
            physicsConfig.gravity = readVec2(scenePhysics["gravity"], physicsConfig.gravity);
        physics.setGravity(physicsConfig.gravity);
        physics.setFixedTimeStep(physicsConfig.fixedTimeStep);
        physics.world().SetVelocityIterations(physicsConfig.velocityIterations);
        physics.world().SetTreeBroadphase(physicsConfig.treeBroadphase);
        k2d::RouteZenScriptCollisions(physics);
        physics.build(scene.root());

        if (result == 0)
        {
            if (!canvas.Init())
            {
                std::fprintf(stderr, "Could not initialize the game renderer\n");
                result = 1;
            }
            else
            {
                bool profilerVisible = false;
                while (device.PollEvents())
                {
                    k2d::Profiler::Get().beginFrame();
                    if (device.GetInput().KeyPressed(SDL_SCANCODE_F5))
                    {
                        profilerVisible = !profilerVisible;
                        k2d::SetZenScriptProfilerVisible(profilerVisible);
                        k2d::ZenRuntime::instance().setVmProfiling(profilerVisible);
                    }
                    const float deltaTime = device.DeltaTime();
                    k2d::SetZenScriptFrameStats(deltaTime, device.FPS());
                    scene.update(deltaTime);
                    physics.step(deltaTime);
                    k2d::DispatchZenScriptEvents(scene.root());

                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    const float width = static_cast<float>(device.Width());
                    const float height = static_cast<float>(device.Height());
                    k2d::SetZenScriptGameViewport(0.0f, 0.0f, width, height);
                    if (k2d::CameraComponent *camera = scene.activeCamera())
                    {
                        camera->setViewport(width, height);
                        canvas.SetProjection(camera->projection());
                        k2d::SetZenScriptGameCamera(&camera->camera());
                    }
                    else
                    {
                        k2d::Camera2D defaultCamera;
                        canvas.SetProjection(defaultCamera.Projection(width, height));
                        k2d::SetZenScriptGameCamera(&defaultCamera);
                    }
                    scene.render(canvas);
                    k2d::ZenRuntime::instance().submitProfilerSamples();
                    if (profilerVisible)
                        drawProfilerOverlay(canvas, width, height);
                    device.Swap();
                    k2d::Profiler::Get().endFrame();
                }
                canvas.Shutdown();
            }
        }

        k2d::SetZenScriptsEnabled(false);
        k2d::SetZenScriptGameCamera(nullptr);
        k2d::SetZenScriptGameViewport(0.0f, 0.0f, 0.0f, 0.0f);
        k2d::SetZenScriptProfilerVisible(false);
        k2d::ZenRuntime::instance().setVmProfiling(false);
        k2d::SetZenScriptOutput(nullptr, nullptr);
        k2d::SetZenScriptAssets(nullptr);
        k2d::SetZenScriptInput(nullptr);
    }
    device.Shutdown();
    return result;
}
