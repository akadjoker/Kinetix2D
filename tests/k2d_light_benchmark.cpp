// Frame time of the 2D lights and shadows on the GPU: fixed scenes, vsync off,
// glFinish every frame. Needs a display; run from bin/.
#include <k2d/Camera2D.h>
#include <k2d/CanvasRenderer.h>
#include <k2d/Device.h>
#include <k2d/GameObject.h>
#include <k2d/Light2D.h>
#include <k2d/LightOccluder2D.h>
#include <k2d/OpenGL.h>
#include <k2d/RectShape.h>
#include <k2d/RenderTarget.h>
#include <k2d/Scene.h>

#include <ct/sort.hpp>
#include <ct/vector.hpp>

#include <cstdio>

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
};

void build(k2d::Scene& scene, const Case& c)
{
    for (int i = 0; i < c.rects; ++i)
    {
        k2d::GameObject* object = scene.createObject("rect");
        object->setPosition(Math::Vec2(20.0f + (i * 37) % 1240, 20.0f + ((i * 37) / 1240) * 23 % 680));
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
        object->setPosition(Math::Vec2(160.0f + (i % 4) * 320.0f, 180.0f + (i / 4) * 360.0f));
        k2d::Light2D* light = object->addComponent<k2d::Light2D>();
        light->setRadius(400.0f);
        light->setEnergy(1.0f);
        light->setCastShadow(c.shadows);
        light->setShadowFilter(c.filter);
    }
}

void run(k2d::Device& device, k2d::CanvasRenderer& canvas, k2d::RenderTarget& target, const Case& c)
{
    k2d::Scene scene;
    build(scene, c);
    k2d::Camera2D camera;
    camera.position = Math::Vec2(kWidth * 0.5f, kHeight * 0.5f);
    ct::Vector<double> times;
    std::size_t drawCalls = 0;
    // Into a framebuffer of its own and never presented while measured, so the
    // display's refresh does not pace the frames.
    for (int frame = 0; frame < 360 && device.PollEvents(); ++frame)
    {
        const double start = k2d::Device::TimeSeconds();
        canvas.ResetStats();
        target.bind();
        glViewport(0, 0, target.width(), target.height());
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetProjection(camera.Projection(kWidth, kHeight));
        scene.setRenderCamera(&camera, kWidth, kHeight);
        scene.render(canvas);
        glFinish();
        const double ms = (k2d::Device::TimeSeconds() - start) * 1000.0;
        drawCalls = canvas.GetStats().drawCalls;
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
    const int shadowDraws = c.shadows ? c.lights * 4 * c.occluders * 4 : 0;
    std::printf("%-34s avg %7.3f ms  p95 %7.3f ms  batch draws %4zu  shadow draws %6d\n", c.name,
                total / static_cast<double>(times.size()), times[times.size() * 95 / 100], drawCalls, shadowDraws);
}

} // namespace

int main()
{
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
        {"8 shadow lights, 50 occluders", 2000, 8, true, 50, k2d::SHADOW_FILTER_PCF5},
        {"8 shadow lights, 200 occluders", 2000, 8, true, 200, k2d::SHADOW_FILTER_PCF5},
        {"8 shadow lights, 200 occl., PCF13", 2000, 8, true, 200, k2d::SHADOW_FILTER_PCF13},
    };
    std::printf("GL: %s | %s\n", reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
                reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    for (const Case& c : cases)
        run(device, canvas, target, c);
    target.destroy();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}
