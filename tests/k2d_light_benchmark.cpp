// Frame time of the 2D lights and shadows on the GPU: fixed scenes drawn into
// a target of their own, one pixel read back every frame (WebGL's glFinish does
// not wait for the GPU); every 30th is shown, outside the
// timing. Needs a display; run from bin/. --dump writes each case's last frame
// to light_case_N.png.
#include <k2d/Camera2D.h>
#include <k2d/CanvasRenderer.h>
#include <k2d/Device.h>
#include <k2d/DirectionalLight2D.h>
#include <k2d/GameObject.h>
#include <k2d/Image.h>
#include <k2d/Light2D.h>
#include <k2d/LightOccluder2D.h>
#include <k2d/OpenGL.h>
#include <k2d/Profiler.h>
#include <k2d/RectShape.h>
#include <k2d/RenderTarget.h>
#include <k2d/Scene.h>

#include <ct/sort.hpp>
#include <ct/vector.hpp>

#include <cstdio>
#include <cstring>

namespace
{

constexpr float kWidth = 1280.0f;
constexpr float kHeight = 720.0f;

struct Case
{
    const char* name;
    int rects;
    int lights;
    bool shadows;
    int occluders;
    k2d::ShadowFilter filter;
    float spread = 1.0f;
    int directional = 0;
};

void build(k2d::Scene& scene, const Case& c)
{
    for (int i = 0; i < c.rects; ++i)
    {
        k2d::GameObject* object = scene.createObject("rect");
        object->setPosition(
            Math::Vec2(20.0f + (i * 37) % 1240, 20.0f + ((i * 37) / 1240) * 23 % 680) * c.spread);
        k2d::RectShape* shape = object->addComponent<k2d::RectShape>();
        shape->setSize(Math::Vec2(24.0f, 24.0f));
        shape->setColor(200, 200, 210);
    }
    for (int i = 0; i < c.occluders; ++i)
    {
        k2d::GameObject* object = scene.createObject("occluder");
        object->setPosition(Math::Vec2(60.0f + (i % 20) * 60.0f, 60.0f + (i / 20) * 60.0f));
        const Math::Vec2 box[4] = {Math::Vec2(-10.0f, -10.0f), Math::Vec2(10.0f, -10.0f), Math::Vec2(10.0f, 10.0f),
                                   Math::Vec2(-10.0f, 10.0f)};
        object->addComponent<k2d::LightOccluder2D>()->setPolygon(box, 4);
    }
    for (int i = 0; i < c.lights; ++i)
    {
        k2d::GameObject* object = scene.createObject("light");
        object->setPosition(Math::Vec2(160.0f + (i % 4) * 320.0f, 180.0f + (i / 4) * 360.0f) * c.spread);
        k2d::Light2D* light = object->addComponent<k2d::Light2D>();
        light->setRadius(400.0f);
        light->setEnergy(1.0f);
        light->setCastShadow(c.shadows);
        light->setShadowFilter(c.filter);
    }
    for (int i = 0; i < c.directional; ++i)
    {
        k2d::GameObject* object = scene.createObject("sun");
        object->setRotationDegrees(30.0f + 60.0f * (float)i);
        k2d::DirectionalLight2D* light = object->addComponent<k2d::DirectionalLight2D>();
        light->setEnergy(0.3f);
        light->setCastShadow(c.shadows);
        light->setShadowFilter(c.filter);
    }
}

uint64_t counter(const char* name)
{
    const k2d::Profiler& profiler = k2d::Profiler::Get();
    for (uint32_t i = 0; i < profiler.counterCount(); ++i)
        if (std::strcmp(profiler.counters()[i].name, name) == 0)
            return profiler.counters()[i].value;
    return 0;
}

void run(k2d::Device& device, k2d::CanvasRenderer& canvas, k2d::RenderTarget& target, const Case& c, int dump)
{
    k2d::Scene scene;
    build(scene, c);
    k2d::Camera2D camera;
    camera.position = Math::Vec2(kWidth * 0.5f, kHeight * 0.5f);
    ct::Vector<double> times;
    std::size_t drawCalls = 0;
    std::size_t culled = 0;
    uint64_t lights = 0;
    // Into a framebuffer of its own and never presented while measured, so the
    // display's refresh does not pace the frames.
    for (int frame = 0; frame < 360 && device.PollEvents(); ++frame)
    {
        const double start = k2d::Device::TimeSeconds();
        k2d::Profiler::Get().beginFrame();
        canvas.ResetStats();
        target.bind();
        glViewport(0, 0, target.width(), target.height());
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetProjection(camera.Projection(kWidth, kHeight));
        scene.setRenderCamera(&camera, kWidth, kHeight);
        scene.render(canvas);
        unsigned char pixel[4];
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        lights = counter("lights");
        k2d::Profiler::Get().endFrame();
        const double ms = (k2d::Device::TimeSeconds() - start) * 1000.0;
        drawCalls = canvas.GetStats().drawCalls;
        culled = scene.renderCulledCount() + canvas.GetStats().commandsCulled + canvas.GetStats().lightsCulled;
        if (dump >= 0 && frame == 359)
        {
            ct::Vector<unsigned char> pixels(static_cast<std::size_t>(target.width() * target.height() * 4));
            glReadPixels(0, 0, target.width(), target.height(), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            char path[64];
            std::snprintf(path, sizeof(path), "light_case_%d.png", dump);
            k2d::WriteImagePng(path, target.width(), target.height(),
                               pixels.data() + (target.height() - 1) * target.width() * 4, -target.width() * 4);
        }
        if (frame % 30 == 29)
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, target.width(), target.height(), 0, 0, target.width(), target.height(),
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
            device.Swap();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (frame >= 60)
            times.push_back(ms);
    }
    if (times.empty())
        return;
    double total = 0.0;
    for (double t : times)
        total += t;
    ct::sort(times.begin(), times.end());
    std::printf("%-36s avg %7.3f ms  p95 %7.3f ms  batch draws %4zu  lights %2u  culled %5zu\n", c.name,
                total / static_cast<double>(times.size()), times[times.size() * 95 / 100], drawCalls,
                (unsigned)lights, culled);
    const k2d::Profiler& profiler = k2d::Profiler::Get();
    std::printf("   ");
    for (uint32_t i = 0; i < profiler.sampleCount(); ++i)
        if (profiler.samples()[i].average > 0.0f)
            std::printf(" %s %.3f", profiler.samples()[i].name, profiler.samples()[i].average);
    std::printf("\n");
}

} // namespace

int main(int argc, char** argv)
{
    const bool dump = argc > 1 && std::strcmp(argv[1], "--dump") == 0;
    k2d::Device device;
    if (!device.Init("k2d light benchmark", static_cast<int>(kWidth), static_cast<int>(kHeight), false))
        return 1;
    k2d::CanvasRenderer canvas;
    k2d::RenderTarget target;
    if (!canvas.Init() || !target.resize(static_cast<int>(kWidth), static_cast<int>(kHeight)))
        return 1;
    const Case cases[] = {
        {"2000 rects, no lights", 2000, 0, false, 0, k2d::SHADOW_FILTER_PCF5},
        {"2000 rects, 8 lights", 2000, 8, false, 0, k2d::SHADOW_FILTER_PCF5},
        {"1 shadow light, 50 occluders", 2000, 1, true, 50, k2d::SHADOW_FILTER_PCF5},
        {"8 shadow lights, 50 occluders", 2000, 8, true, 50, k2d::SHADOW_FILTER_PCF5},
        {"8 shadow lights, 200 occluders", 2000, 8, true, 200, k2d::SHADOW_FILTER_PCF5},
        {"8 shadow lights, 200 occl., PCF13", 2000, 8, true, 200, k2d::SHADOW_FILTER_PCF13},
        {"8000 rects, 16 lights over 3x3 view", 8000, 16, false, 0, k2d::SHADOW_FILTER_PCF5, 3.0f},
        {"2000 rects, 16 lights", 2000, 16, false, 0, k2d::SHADOW_FILTER_PCF5},
        {"16 shadow lights, 200 occluders", 2000, 16, true, 200, k2d::SHADOW_FILTER_PCF5},
        {"16 shadow lights, 200 occl., PCF13", 2000, 16, true, 200, k2d::SHADOW_FILTER_PCF13},
        {"8 shadow lights + 2 suns, 200 occl.", 2000, 8, true, 200, k2d::SHADOW_FILTER_PCF5, 1.0f, 2},
    };
    k2d::Profiler::Get().SetEnabled(true);
    std::printf("GL: %s | %s\n", reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
                reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    GLint fragmentVectors = 0, blockSize = 0, fragmentBlocks = 0, textureSize = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &fragmentVectors);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &blockSize);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &fragmentBlocks);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureSize);
    std::printf("limits: fragment uniform vectors %d, uniform block %d bytes, fragment blocks %d, texture %d\n",
                fragmentVectors, blockSize, fragmentBlocks, textureSize);
    int index = 0;
    for (const Case& c : cases)
        run(device, canvas, target, c, dump ? index++ : -1);
    target.destroy();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}
