#include <k2d/k2d.h>
#include <kx/kx.h>

#include <glad/glad.h>

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstdlib>

static const int SCANCODE_ESCAPE = 41;
static const int SCANCODE_B = 5;
static const int SCANCODE_C = 6;
static const int SCANCODE_E = 8;
static const int SCANCODE_K = 14;
static const int SCANCODE_R = 21;
static const int SCANCODE_V = 25;
static const int SCANCODE_F1 = 58;
static const int SCANCODE_F2 = 59;
static const int SCANCODE_F3 = 60;

class BatchDebugDraw : public kx::DebugDraw
{
public:
    explicit BatchDebugDraw(k2d::BatchRenderer &batch) : mBatch(batch)
    {
    }

    void DrawCircleShape(const kx::Transform &xf, float radius, kx::Color color) override
    {
        glm::vec2 c = xf.Transform(0.0f, 0.0f);
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawCircle(c.x, c.y, radius, 24);
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
        mBatch.DrawCircle(p.x, p.y, size, 10);
    }

    void DrawAABB(const glm::vec2 &lower, const glm::vec2 &upper, kx::Color color) override
    {
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawRect(lower.x, lower.y, upper.x - lower.x, upper.y - lower.y, false);
    }

private:
    k2d::BatchRenderer &mBatch;
};

struct KinematicPlatform
{
    kx::Body *body;
    float halfWidth;
};

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Physics", 1280, 720, true))
        return 1;

    k2d::BatchRenderer batch;
    k2d::BatchRenderer::Config config;
    if (!batch.Init(config))
        return 1;

    batch.Resize(device.Width(), device.Height());

    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(640.0f, 700.0f), 600.0f, 20.0f);

    BatchDebugDraw debugDraw(batch);
    unsigned drawFlags = kx::DebugDrawShapes | kx::DebugDrawContacts;

    ct::Vector<KinematicPlatform> kinematics;
    kx::MouseJoint *grab = nullptr;

    bool running = true;
    while (running)
    {
        running = device.PollEvents();

        k2d::Input &input = device.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;

        float mouseX = input.MouseX();
        float mouseY = input.MouseY();
        glm::vec2 mouse(mouseX, mouseY);

        if (input.MousePressed(0) && !grab)
        {
            kx::Body *picked = world.BodyAtPoint(mouse);
            if (picked)
            {
                grab = new kx::MouseJoint(picked, mouse, 5000.0f * picked->Mass());
                world.AddJoint(grab);
            }
        }
        if (grab)
        {
            if (input.MouseDown(0))
            {
                grab->SetTarget(mouse);
            }
            else
            {
                world.DestroyJoint(grab);
                grab = nullptr;
            }
        }

        if (input.KeyPressed(SCANCODE_B))
        {
            float hw = 10.0f + (float)(std::rand() % 21);
            float hh = 10.0f + (float)(std::rand() % 21);
            world.CreateBox(glm::vec2(mouseX, mouseY), hw, hh, 1.0f);
        }

        if (input.KeyPressed(SCANCODE_C))
        {
            float radius = 8.0f + (float)(std::rand() % 18);
            world.CreateCircle(glm::vec2(mouseX, mouseY), radius, 1.0f);
        }

        if (input.KeyPressed(SCANCODE_K))
        {
            kx::Body *platform = world.CreateKinematicBox(glm::vec2(mouseX, mouseY), 60.0f, 7.0f);
            platform->SetVelocity(glm::vec2(150.0f, 0.0f));
            KinematicPlatform info;
            info.body = platform;
            info.halfWidth = 60.0f;
            kinematics.push_back(info);
        }

        if (input.KeyPressed(SCANCODE_V))
        {
            kx::Body *cart = world.CreateBody(kx::BodyType::Dynamic, glm::vec2(mouseX, mouseY));
            cart->AddBox(15.0f, 5.0f, 1.0f);
            cart->AddCircle(glm::vec2(-20.0f, 12.0f), 8.0f, 1.0f);
            cart->AddCircle(glm::vec2(20.0f, 12.0f), 8.0f, 1.0f);
        }

        if (input.KeyPressed(SCANCODE_E))
        {
            world.CreateEdge(glm::vec2(mouseX - 100.0f, mouseY), glm::vec2(mouseX + 100.0f, mouseY));
        }

        if (input.KeyPressed(SCANCODE_R))
        {
            ct::Vector<kx::Body *> toDestroy;
            const ct::Vector<kx::Body *> &bodies = world.Bodies();
            for (size_t i = 0; i < bodies.size(); ++i)
                if (bodies[i]->Type() == kx::BodyType::Dynamic)
                    toDestroy.push_back(bodies[i]);

            for (size_t i = 0; i < toDestroy.size(); ++i)
                world.Destroy(toDestroy[i]);

            kinematics.clear();
        }

        if (input.KeyPressed(SCANCODE_F1))
            drawFlags ^= kx::DebugDrawShapes;
        if (input.KeyPressed(SCANCODE_F2))
            drawFlags ^= kx::DebugDrawAABBs;
        if (input.KeyPressed(SCANCODE_F3))
            drawFlags ^= kx::DebugDrawContacts;

        if (device.WasResized())
        {
            batch.Resize(device.Width(), device.Height());
            glViewport(0, 0, device.Width(), device.Height());
        }

        float dt = device.DeltaTime();

        for (size_t i = 0; i < kinematics.size(); ++i)
        {
            kx::Body *b = kinematics[i].body;
            float hw = kinematics[i].halfWidth;
            glm::vec2 pos = b->Position();
            glm::vec2 vel = b->Velocity();
            if (pos.x - hw <= 0.0f && vel.x < 0.0f)
                vel.x = -vel.x;
            if (pos.x + hw >= (float)device.Width() && vel.x > 0.0f)
                vel.x = -vel.x;
            b->SetVelocity(vel);
        }

        world.Step(dt);

        glClearColor(0.05f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        batch.BeginFrame();

        if (drawFlags != 0)
            kx::Draw(world, debugDraw, drawFlags);

        batch.SetColor((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255);
        batch.DrawText(20.0f, 20.0f, 20.0f, "Kinetix2D Physics");

        ct::String hud;
        hud += "bodies ";
        hud.append_number((int)world.BodyCount());
        hud += " contacts ";
        hud.append_number((int)world.ContactCount());
        hud += " fps ";
        hud.append_number((int)(dt > 0.0f ? 1.0f / dt : 0.0f));
        batch.DrawText(20.0f, 50.0f, 16.0f, hud.c_str());

        ct::String flagsText;
        flagsText += "F1 shapes:";
        flagsText += (drawFlags & kx::DebugDrawShapes) ? "on" : "off";
        flagsText += " F2 aabbs:";
        flagsText += (drawFlags & kx::DebugDrawAABBs) ? "on" : "off";
        flagsText += " F3 contacts:";
        flagsText += (drawFlags & kx::DebugDrawContacts) ? "on" : "off";
        batch.DrawText(20.0f, 75.0f, 16.0f, flagsText.c_str());

        batch.DrawText(20.0f, 100.0f, 16.0f, "B box  C circle  K platform  V cart  E edge  R clear  ESC quit");

        batch.EndFrame();

        device.Swap();
    }

    batch.Shutdown();
    device.Shutdown();

    return 0;
}
