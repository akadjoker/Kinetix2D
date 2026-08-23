#include <k2d/k2d.h>

#include <imgui.h>
#include <kx/kx.h>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <ct/string.hpp>
#include <ct/vector.hpp>

static const int SCANCODE_ESCAPE = 41;
static const int SCANCODE_SPACE = 44;
static const int SCANCODE_R = 21;
static const int SCANCODE_W = 26;
static const int SCANCODE_A = 4;
static const int SCANCODE_S = 22;
static const int SCANCODE_D = 7;
static const int SCANCODE_1 = 30;
static const int SCANCODE_2 = 31;
static const int SCANCODE_F1 = 58;
static const int SCANCODE_F2 = 59;
static const int SCANCODE_F3 = 60;
static const int SCANCODE_F4 = 61;

static const float kArenaW = 8000.0f;
static const float kArenaH = 4000.0f;

static unsigned gSeed = 777;
static float Rnd()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return (float)(gSeed >> 8) / 16777216.0f;
}

struct Camera
{
    glm::vec2 center;
    float zoom;

    glm::mat4 Projection(float screenW, float screenH) const
    {
        float hw = screenW * 0.5f / zoom;
        float hh = screenH * 0.5f / zoom;
        return glm::ortho(center.x - hw, center.x + hw, center.y + hh, center.y - hh, -1.0f, 1.0f);
    }

    glm::vec2 ScreenToWorld(float sx, float sy, float screenW, float screenH) const
    {
        return glm::vec2(center.x + (sx - screenW * 0.5f) / zoom,
                         center.y + (sy - screenH * 0.5f) / zoom);
    }
};

class StressDebugDraw : public kx::DebugDraw
{
public:
    explicit StressDebugDraw(k2d::BatchRenderer &batch) : mBatch(batch)
    {
    }

    void DrawCircleShape(const kx::Transform &xf, float radius, kx::Color color) override
    {
        glm::vec2 c = xf.Transform(0.0f, 0.0f);
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawCircle(c.x, c.y, radius, 12);
    }

    void DrawPolygonShape(const kx::Transform &xf, const glm::vec2 *verts, int count, kx::Color color) override
    {
        if (count <= 0 || count > 16)
            return;
        float points[(16 + 1) * 2];
        for (int i = 0; i < count; ++i)
        {
            glm::vec2 p = xf.Transform(verts[i]);
            points[i * 2 + 0] = p.x;
            points[i * 2 + 1] = p.y;
        }
        glm::vec2 first = xf.Transform(verts[0]);
        points[count * 2 + 0] = first.x;
        points[count * 2 + 1] = first.y;
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawPolyline(points, count + 1);
    }

    void DrawSegment(const glm::vec2 &a, const glm::vec2 &b, kx::Color color) override
    {
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawLine(a.x, a.y, b.x, b.y);
    }

    void DrawPoint(const glm::vec2 &p, float size, kx::Color color) override
    {
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawCircle(p.x, p.y, size, 6);
    }

    void DrawAABB(const glm::vec2 &lower, const glm::vec2 &upper, kx::Color color) override
    {
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawRect(lower.x, lower.y, upper.x - lower.x, upper.y - lower.y, false);
    }

private:
    k2d::BatchRenderer &mBatch;
};

static void SpawnRain(kx::World &world, const glm::vec2 &around, int count)
{
    for (int i = 0; i < count; ++i)
    {
        glm::vec2 pos = around + glm::vec2((Rnd() - 0.5f) * 1200.0f, (Rnd() - 0.5f) * 400.0f);
        if (pos.x < 60.0f)
            pos.x = 60.0f;
        if (pos.x > kArenaW - 60.0f)
            pos.x = kArenaW - 60.0f;
        if (pos.y < 60.0f)
            pos.y = 60.0f;
        if (pos.y > kArenaH - 60.0f)
            pos.y = kArenaH - 60.0f;

        if (Rnd() > 0.5f)
            world.CreateBox(pos, 8.0f + Rnd() * 12.0f, 8.0f + Rnd() * 12.0f, 1.0f);
        else
            world.CreateCircle(pos, 7.0f + Rnd() * 10.0f, 1.0f);
    }
}

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Stress", 1280, 720, false))
        return 1;

    k2d::BatchRenderer batch;
    k2d::BatchRenderer::Config config;
    config.maxVertices = 65532;
    config.maxDrawCalls = 4096;
    if (!batch.Init(config))
        return 1;

    batch.Resize(device.Width(), device.Height());

    kx::World world(glm::vec2(0.0f, 500.0f));
    world.SetTimeSource(&k2d::Device::TimeSeconds);

    world.CreateStaticBox(glm::vec2(kArenaW * 0.5f, kArenaH - 20.0f), kArenaW * 0.5f, 20.0f);
    world.CreateStaticBox(glm::vec2(kArenaW * 0.5f, 20.0f), kArenaW * 0.5f, 20.0f);
    world.CreateStaticBox(glm::vec2(20.0f, kArenaH * 0.5f), 20.0f, kArenaH * 0.5f);
    world.CreateStaticBox(glm::vec2(kArenaW - 20.0f, kArenaH * 0.5f), 20.0f, kArenaH * 0.5f);

    for (int i = 0; i < 6; ++i)
        world.CreateStaticBox(glm::vec2(800.0f + i * 1200.0f, kArenaH - 600.0f - (i % 3) * 400.0f),
                              300.0f, 15.0f);

    StressDebugDraw debugDraw(batch);
    unsigned drawFlags = kx::DebugDrawShapes | kx::DebugDrawContacts;

    Camera camera;
    camera.center = glm::vec2(kArenaW * 0.5f, kArenaH - 800.0f);
    camera.zoom = 0.5f;

    kx::MouseJoint *grab = nullptr;
    bool debugEnabled = true;
    bool showProfiler = true;
    k2d::Profiler::Get().SetEnabled(true);

    float fpsSmooth = 60.0f;

    bool running = true;
    while (running)
    {
        k2d::Profiler::Get().beginFrame();
        running = device.PollEvents();

        k2d::Input &input = device.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;

        float dt = device.DeltaTime();
        if (dt > 0.0f)
            fpsSmooth = fpsSmooth * 0.95f + (1.0f / dt) * 0.05f;

        float panSpeed = 900.0f / camera.zoom * dt;
        if (input.KeyDown(SCANCODE_W))
            camera.center.y -= panSpeed;
        if (input.KeyDown(SCANCODE_S))
            camera.center.y += panSpeed;
        if (input.KeyDown(SCANCODE_A))
            camera.center.x -= panSpeed;
        if (input.KeyDown(SCANCODE_D))
            camera.center.x += panSpeed;
        if (input.WheelY() != 0.0f)
        {
            camera.zoom *= input.WheelY() > 0.0f ? 1.1f : 0.9f;
            if (camera.zoom < 0.05f)
                camera.zoom = 0.05f;
            if (camera.zoom > 4.0f)
                camera.zoom = 4.0f;
        }

        glm::vec2 mouseWorld = camera.ScreenToWorld(input.MouseX(), input.MouseY(),
                                                    (float)device.Width(), (float)device.Height());

        if (input.KeyDown(SCANCODE_SPACE))
            SpawnRain(world, mouseWorld, 25);
        if (input.KeyPressed(SCANCODE_1))
            SpawnRain(world, glm::vec2(kArenaW * 0.5f, 600.0f), 500);
        if (input.KeyPressed(SCANCODE_2))
            SpawnRain(world, mouseWorld, 500);

        if (input.KeyPressed(SCANCODE_R))
        {
            if (grab)
            {
                world.DestroyJoint(grab);
                grab = nullptr;
            }
            ct::Vector<kx::Body *> toDestroy;
            const ct::Vector<kx::Body *> &bodies = world.Bodies();
            for (size_t i = 0; i < bodies.size(); ++i)
                if (bodies[i]->Type() == kx::BodyType::Dynamic)
                    toDestroy.push_back(bodies[i]);
            for (size_t i = 0; i < toDestroy.size(); ++i)
                world.Destroy(toDestroy[i]);
        }

        if (input.MousePressed(0) && !grab && !device.ImGuiWantsMouse())
        {
            kx::Body *picked = world.BodyAtPoint(mouseWorld);
            if (picked)
            {
                grab = new kx::MouseJoint(picked, mouseWorld, 5000.0f * picked->Mass());
                world.AddJoint(grab);
            }
        }
        if (grab)
        {
            if (input.MouseDown(0))
                grab->SetTarget(mouseWorld);
            else
            {
                world.DestroyJoint(grab);
                grab = nullptr;
            }
        }

        if (input.KeyPressed(SCANCODE_F1))
            drawFlags ^= kx::DebugDrawShapes;
        if (input.KeyPressed(SCANCODE_F2))
            debugEnabled = !debugEnabled;
        if (input.KeyPressed(23))
            world.SetTreeBroadphase(!world.TreeBroadphase());
        if (input.KeyPressed(SCANCODE_F3))
            drawFlags ^= kx::DebugDrawContacts;
        if (input.KeyPressed(SCANCODE_F4))
            showProfiler = !showProfiler;

        if (device.WasResized())
            batch.Resize(device.Width(), device.Height());

        static float accumulator = 0.0f;
        accumulator += dt;
        if (accumulator > 4.0f / 60.0f)
            accumulator = 4.0f / 60.0f;

        {
            k2d::ProfileScope profileStep("physics.step");
            while (accumulator >= 1.0f / 60.0f)
            {
                world.Step(1.0f / 60.0f);
                accumulator -= 1.0f / 60.0f;
            }
        }

        const kx::StepProfile &sp = world.Profile();
        k2d::Profiler::Get().addSample("kx.broadphase", sp.broadphase);
        k2d::Profiler::Get().addSample("kx.narrowphase", sp.narrowphase);
        k2d::Profiler::Get().addSample("kx.solve.vel", sp.solveVelocity);
        k2d::Profiler::Get().addSample("kx.solve.pos", sp.solvePosition);
        k2d::Profiler::Get().addSample("kx.integrate", sp.integrate);

        glClearColor(0.04f, 0.05f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        batch.BeginFrame();
        batch.ResetStats();

        if (debugEnabled)
        {
            batch.SetProjection(camera.Projection((float)device.Width(), (float)device.Height()));
            {
                k2d::ProfileScope profileDraw("debug.draw");
                kx::Draw(world, debugDraw, drawFlags);
            }
            batch.DrawRenderBatch();
        }

        batch.SetProjection(glm::ortho(0.0f, (float)device.Width(), (float)device.Height(), 0.0f, -1.0f, 1.0f));

        ct::String hud;
        hud += "bodies ";
        hud.append_number((int)world.BodyCount());
        hud += "  contacts ";
        hud.append_number((int)world.ContactCount());
        hud += world.TreeBroadphase() ? "  [tree]" : "  [BRUTE]";
        hud += "  fps ";
        hud.append_number((int)fpsSmooth);
        batch.SetColor((unsigned char)0, (unsigned char)0, (unsigned char)0, (unsigned char)180);
        batch.DrawRect(10.0f, 10.0f, 460.0f, 34.0f, true);
        batch.SetColor((unsigned char)120, (unsigned char)255, (unsigned char)120, (unsigned char)255);
        batch.DrawText(20.0f, 18.0f, 18.0f, hud.c_str());
        batch.SetColor((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255);
        batch.DrawText(20.0f, 46.0f, 14.0f, "WASD camera  scroll zoom  SPACE chuva  1/2 +500  R clear  T tree  F2 debug  F1/F3/F4  ESC");

        batch.EndFrame();

        device.BeginUI();
        if (showProfiler)
            k2d::ShowProfilerWindow(&showProfiler);
        device.EndUI();

        device.Swap();
        k2d::Profiler::Get().endFrame();
    }

    return 0;
}