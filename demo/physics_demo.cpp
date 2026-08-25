#include <k2d/k2d.h>

#include <imgui.h>
#include <kx/kx.h>

#include <stb_image.h>

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstdlib>

static const int SCANCODE_ESCAPE = 41;
static const int SCANCODE_B = 5;
static const int SCANCODE_C = 6;
static const int SCANCODE_E = 8;
static const int SCANCODE_G = 10;
static const int SCANCODE_I = 12;
static const int SCANCODE_K = 14;
static const int SCANCODE_R = 21;
static const int SCANCODE_V = 25;
static const int SCANCODE_F1 = 58;
static const int SCANCODE_F2 = 59;
static const int SCANCODE_F3 = 60;
static const int SCANCODE_F4 = 61;
static const int SCANCODE_F9 = 66;
static const int SCANCODE_F10 = 67;
static const int SCANCODE_RIGHT = 79;
static const int SCANCODE_LEFT = 80;

class BatchDebugDraw : public kx::DebugDraw
{
  public:
    explicit BatchDebugDraw(k2d::BatchRenderer& batch) : mBatch(batch)
    {
    }

    void DrawCircleShape(const kx::Transform& xf, float radius, kx::Color color) override
    {
        Math::Vec2 c = xf.Transform(0.0f, 0.0f);
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawCircle(c.x, c.y, radius, 24);
    }

    void DrawPolygonShape(const kx::Transform& xf, const Math::Vec2* verts, int count, kx::Color color) override
    {
        if (count <= 0 || count > 16)
            return;

        float points[(16 + 1) * 2];
        for (int i = 0; i < count; ++i)
        {
            Math::Vec2 p = xf.Transform(verts[i]);
            points[i * 2 + 0] = p.x;
            points[i * 2 + 1] = p.y;
        }
        Math::Vec2 first = xf.Transform(verts[0]);
        points[count * 2 + 0] = first.x;
        points[count * 2 + 1] = first.y;

        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawPolyline(points, count + 1);
    }

    void DrawSegment(const Math::Vec2& a, const Math::Vec2& b, kx::Color color) override
    {
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawLine(a.x, a.y, b.x, b.y);
    }

    void DrawPoint(const Math::Vec2& p, float size, kx::Color color) override
    {
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawCircle(p.x, p.y, size, 10);
    }

    void DrawAABB(const Math::Vec2& lower, const Math::Vec2& upper, kx::Color color) override
    {
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawRect(lower.x, lower.y, upper.x - lower.x, upper.y - lower.y, false);
    }

  private:
    k2d::BatchRenderer& mBatch;
};

struct KinematicPlatform
{
    kx::Body* body;
    float halfWidth;
};

struct Car
{
    kx::Body* chassis;
    kx::Body* wheelRear;
    kx::Body* wheelFront;
    kx::WheelJoint* jointRear;
    kx::WheelJoint* jointFront;
};

static void SpawnBridge(kx::World& world, const Math::Vec2& dropCenter)
{
    const int kPlanks = 14;
    const float kSpan = 700.0f;
    const float kPlankHalfHeight = 6.0f;
    const float kPlankWidth = kSpan / (float)kPlanks;
    const float kPlankHalfWidth = kPlankWidth * 0.5f - 1.0f;

    float leftX = dropCenter.x - kSpan * 0.5f;
    float y = dropCenter.y;

    kx::Body* leftPillar = world.CreateStaticBox(Math::Vec2(leftX, y + 60.0f), 20.0f, 70.0f);
    kx::Body* rightPillar = world.CreateStaticBox(Math::Vec2(leftX + kSpan, y + 60.0f), 20.0f, 70.0f);

    kx::Body* prev = leftPillar;
    Math::Vec2 anchor(leftX, y);

    for (int i = 0; i < kPlanks; ++i)
    {
        float cx = leftX + kPlankWidth * ((float)i + 0.5f);
        kx::Body* plank = world.CreateBox(Math::Vec2(cx, y), kPlankHalfWidth, kPlankHalfHeight, 1.0f);

        kx::RevoluteJoint* joint = new kx::RevoluteJoint(prev, plank, anchor);
        world.AddJoint(joint);

        prev = plank;
        anchor = Math::Vec2(cx + kPlankHalfWidth + 1.0f, y);
    }

    kx::RevoluteJoint* last = new kx::RevoluteJoint(prev, rightPillar, anchor);
    world.AddJoint(last);
}

static void SpawnGears(kx::World& world, const Math::Vec2& dropCenter)
{
    Math::Vec2 posA = dropCenter + Math::Vec2(-50.0f, 0.0f);
    Math::Vec2 posB = dropCenter + Math::Vec2(50.0f, 0.0f);

    kx::Body* anchorA = world.CreateBody(kx::BodyType::Static, posA);
    kx::Body* discA = world.CreateCircle(posA, 40.0f, 1.0f);
    kx::RevoluteJoint* jointA = new kx::RevoluteJoint(anchorA, discA, posA);
    jointA->SetMotor(true, 3.0f, 1.0e7f);
    world.AddJoint(jointA);

    kx::Body* anchorB = world.CreateBody(kx::BodyType::Static, posB);
    kx::Body* discB = world.CreateCircle(posB, 40.0f, 1.0f);
    kx::RevoluteJoint* jointB = new kx::RevoluteJoint(anchorB, discB, posB);
    world.AddJoint(jointB);

    kx::GearJoint* gear = new kx::GearJoint(jointA, jointB, 2.0f);
    world.AddJoint(gear);
}

static void SpawnBlob(kx::World& world, const Math::Vec2& dropCenter)
{
    const int kRing = 12;
    const float kRingRadius = 30.0f;
    const float kNodeRadius = 7.0f;
    const float kCenterRadius = 10.0f;
    const int16_t kBlobGroup = -1;
    const float kSpringFrequency = 4.0f;
    const float kSpringDamping = 0.5f;

    kx::Body* center = world.CreateCircle(dropCenter, kCenterRadius, 1.0f);
    center->SetFilter(1, 0xFFFF, kBlobGroup);

    kx::Body* nodes[kRing];
    for (int i = 0; i < kRing; ++i)
    {
        float angle = (float)i * (6.28318531f / (float)kRing);
        Math::Vec2 pos = dropCenter + Math::Vec2(kRingRadius * cosf(angle), kRingRadius * sinf(angle));
        nodes[i] = world.CreateCircle(pos, kNodeRadius, 1.0f);
        nodes[i]->SetFilter(1, 0xFFFF, kBlobGroup);
    }

    for (int i = 0; i < kRing; ++i)
    {
        int next = (i + 1) % kRing;

        kx::DistanceJoint* ringJoint =
            new kx::DistanceJoint(nodes[i], nodes[next], nodes[i]->Position(), nodes[next]->Position());
        ringJoint->SetSpring(kSpringFrequency, kSpringDamping);
        ringJoint->SetLengthRange(0.0f, 1000.0f);
        world.AddJoint(ringJoint);

        kx::DistanceJoint* spokeJoint =
            new kx::DistanceJoint(center, nodes[i], center->Position(), nodes[i]->Position());
        spokeJoint->SetSpring(kSpringFrequency, kSpringDamping);
        spokeJoint->SetLengthRange(0.0f, 1000.0f);
        world.AddJoint(spokeJoint);
    }
}

static Car SpawnCar(kx::World& world, const Math::Vec2& pos)
{
    const float kScale = 50.0f;

    Math::Vec2 chassisOutline[6] = {
        Math::Vec2(-1.5f * kScale, 0.5f * kScale),   Math::Vec2(1.5f * kScale, 0.5f * kScale),
        Math::Vec2(1.5f * kScale, 0.0f * kScale),    Math::Vec2(0.0f * kScale, -0.9f * kScale),
        Math::Vec2(-1.15f * kScale, -0.9f * kScale), Math::Vec2(-1.5f * kScale, -0.2f * kScale)};

    Car car;
    car.chassis = world.CreatePolygon(pos, chassisOutline, 6, 1.0f);

    const float wheelRadius = 0.4f * kScale;
    Math::Vec2 rearLocal(-1.0f * kScale, 0.5f * kScale);
    Math::Vec2 frontLocal(1.0f * kScale, 0.5f * kScale);

    car.wheelRear = world.CreateCircle(pos + rearLocal, wheelRadius, 1.0f);
    car.wheelFront = world.CreateCircle(pos + frontLocal, wheelRadius, 1.0f);

    car.jointRear =
        new kx::WheelJoint(car.chassis, car.wheelRear, pos + rearLocal, Math::Vec2(0.0f, -1.0f), 4.0f, 0.7f);
    car.jointFront =
        new kx::WheelJoint(car.chassis, car.wheelFront, pos + frontLocal, Math::Vec2(0.0f, -1.0f), 4.0f, 0.7f);
    world.AddJoint(car.jointRear);
    world.AddJoint(car.jointFront);

    return car;
}

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

    kx::World world(Math::Vec2(0.0f, 500.0f));
    world.CreateStaticBox(Math::Vec2(640.0f, 700.0f), 600.0f, 20.0f);

    world.CreateEdge(Math::Vec2(1240.0f, 700.0f), Math::Vec2(1500.0f, 560.0f));
    world.CreateEdge(Math::Vec2(1500.0f, 560.0f), Math::Vec2(1800.0f, 560.0f));

    unsigned char* bunnyPixels = nullptr;
    int bunnyWidth = 0;
    int bunnyHeight = 0;
    {
        k2d::FileBuffer file;
        if (file.Load("assets/wabbit_alpha.png", false))
        {
            int channels = 0;
            bunnyPixels = stbi_load_from_memory(file.Data(), (int)file.Size(), &bunnyWidth, &bunnyHeight, &channels, 4);
        }
    }

    BatchDebugDraw debugDraw(batch);
    unsigned drawFlags = kx::DebugDrawShapes | kx::DebugDrawContacts | kx::DebugDrawJoints;

    ct::Vector<KinematicPlatform> kinematics;
    ct::Vector<Car> cars;
    kx::MouseJoint* grab = nullptr;

    float carFrequency = 4.0f;
    float carDamping = 0.7f;
    float carMaxTorque = 5000000.0f;

    bool showProfiler = true;
    k2d::Profiler::Get().SetEnabled(true);
    world.SetTimeSource(&k2d::Device::TimeSeconds);

    bool running = true;
    while (running)
    {
        k2d::Profiler::Get().beginFrame();
        running = device.PollEvents();

        k2d::Input& input = device.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;

        float mouseX = input.MouseX();
        float mouseY = input.MouseY();
        Math::Vec2 mouse(mouseX, mouseY);

        if (input.MousePressed(0) && !grab && !device.ImGuiWantsMouse())
        {
            kx::Body* picked = world.BodyAtPoint(mouse);
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

        if (input.KeyDown(SCANCODE_B))
        {
            float hw = 10.0f + (float)(std::rand() % 21);
            float hh = 10.0f + (float)(std::rand() % 21);
            world.CreateBox(Math::Vec2(mouseX, mouseY), hw, hh, 1.0f);
        }

        if (input.KeyDown(SCANCODE_C))
        {
            float radius = 8.0f + (float)(std::rand() % 18);
            world.CreateCircle(Math::Vec2(mouseX, mouseY), radius, 1.0f);
        }

        if (input.KeyPressed(SCANCODE_K))
        {
            kx::Body* platform = world.CreateKinematicBox(Math::Vec2(mouseX, mouseY), 60.0f, 7.0f);
            platform->SetVelocity(Math::Vec2(150.0f, 0.0f));
            KinematicPlatform info;
            info.body = platform;
            info.halfWidth = 60.0f;
            kinematics.push_back(info);
        }

        if (input.KeyDown(SCANCODE_V))
        {
            kx::Body* cart = world.CreateBody(kx::BodyType::Dynamic, Math::Vec2(mouseX, mouseY));
            cart->AddBox(15.0f, 5.0f, Math::Vec2(0.0f, 0.0f), 1.0f);
            cart->AddCircle(Math::Vec2(-20.0f, 12.0f), 8.0f, 1.0f);
            cart->AddCircle(Math::Vec2(20.0f, 12.0f), 8.0f, 1.0f);
        }

        if (input.KeyPressed(SCANCODE_G))
            cars.push_back(SpawnCar(world, Math::Vec2(mouseX, mouseY)));

        if (input.KeyPressed(SCANCODE_I) && bunnyPixels)
            world.CreateFromImage(Math::Vec2(mouseX, mouseY), bunnyPixels, bunnyWidth, bunnyHeight, 4, 128, 1.0f, 2.0f);

        if (cars.size() > 0)
        {
            float motorSpeed = 0.0f;
            if (input.KeyDown(SCANCODE_RIGHT))
                motorSpeed = 15.0f;
            else if (input.KeyDown(SCANCODE_LEFT))
                motorSpeed = -15.0f;

            for (size_t i = 0; i < cars.size(); ++i)
            {
                cars[i].jointRear->SetMotor(true, motorSpeed, carMaxTorque);
                cars[i].jointFront->SetMotor(true, motorSpeed, carMaxTorque);
            }
        }

        if (input.KeyPressed(61))
            showProfiler = !showProfiler;

        if (input.KeyPressed(23))
            world.SetTreeBroadphase(!world.TreeBroadphase());

        if (input.KeyPressed(SCANCODE_E))
        {
            world.CreateEdge(Math::Vec2(mouseX - 100.0f, mouseY), Math::Vec2(mouseX + 100.0f, mouseY));
        }

        if (input.KeyPressed(SCANCODE_R))
        {
            if (grab)
            {
                world.DestroyJoint(grab);
                grab = nullptr;
            }

            ct::Vector<kx::Joint*> toDestroyJoints;
            const ct::Vector<kx::Joint*>& joints = world.Joints();
            for (size_t i = 0; i < joints.size(); ++i)
                toDestroyJoints.push_back(joints[i]);
            for (size_t i = 0; i < toDestroyJoints.size(); ++i)
                world.DestroyJoint(toDestroyJoints[i]);

            cars.clear();

            ct::Vector<kx::Body*> toDestroy;
            const ct::Vector<kx::Body*>& bodies = world.Bodies();
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
        if (input.KeyPressed(62))
            drawFlags ^= kx::DebugDrawJoints;

        if (input.KeyPressed(SCANCODE_F9))
            device.CaptureScreenshot();
        if (input.KeyPressed(SCANCODE_F10))
        {
            if (device.IsGifCapturing())
                device.StopGifCapture();
            else
                device.StartGifCapture(60);
        }

        if (device.WasResized())
        {
            batch.Resize(device.Width(), device.Height());
            glViewport(0, 0, device.Width(), device.Height());
        }

        float dt = device.DeltaTime();
        static float accumulator = 0.0f;
        accumulator += dt;
        if (accumulator > 4.0f / 60.0f)
            accumulator = 4.0f / 60.0f;

        for (size_t i = 0; i < kinematics.size(); ++i)
        {
            kx::Body* b = kinematics[i].body;
            float hw = kinematics[i].halfWidth;
            Math::Vec2 pos = b->Position();
            Math::Vec2 vel = b->Velocity();
            if (pos.x - hw <= 0.0f && vel.x < 0.0f)
                vel.x = -vel.x;
            if (pos.x + hw >= (float)device.Width() && vel.x > 0.0f)
                vel.x = -vel.x;
            b->SetVelocity(vel);
        }

        {
            k2d::ProfileScope profileStep("physics.step");
            while (accumulator >= 1.0f / 60.0f)
            {
                world.Step(1.0f / 60.0f);
                accumulator -= 1.0f / 60.0f;
            }
        }

        const kx::StepProfile& sp = world.Profile();
        k2d::Profiler::Get().addSample("kx.broadphase", sp.broadphase);
        k2d::Profiler::Get().addSample("kx.narrowphase", sp.narrowphase);
        k2d::Profiler::Get().addSample("kx.solve.vel", sp.solveVelocity);
        k2d::Profiler::Get().addSample("kx.solve.pos", sp.solvePosition);
        k2d::Profiler::Get().addSample("kx.integrate", sp.integrate);

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
        hud += world.TreeBroadphase() ? "  [tree]" : "  [brute]";
        batch.DrawText(20.0f, 50.0f, 16.0f, hud.c_str());

        ct::String flagsText;
        flagsText += "F1 shapes:";
        flagsText += (drawFlags & kx::DebugDrawShapes) ? "on" : "off";
        flagsText += " F2 aabbs:";
        flagsText += (drawFlags & kx::DebugDrawAABBs) ? "on" : "off";
        flagsText += " F3 contacts:";
        flagsText += (drawFlags & kx::DebugDrawContacts) ? "on" : "off";
        batch.DrawText(20.0f, 75.0f, 16.0f, flagsText.c_str());

        batch.DrawText(20.0f, 100.0f, 16.0f,
                       "B box  C circle  K platform  V cart  E edge  G car  I bunny  Left/Right drive  T tree  F5 "
                       "joints  R clear  ESC quit  (see Spawn/Car panels)");

        batch.EndFrame();

        device.BeginUI();
        ImGui::Begin("Kinetix2D");
        ImGui::Text("bodies %d  contacts %d", (int)world.BodyCount(), (int)world.ContactCount());
        ImGui::Text("%.1f fps  %.2f ms", 1.0f / (dt > 0.0f ? dt : 1.0f), dt * 1000.0f);
        bool tree = world.TreeBroadphase();
        if (ImGui::Checkbox("tree broadphase", &tree))
            world.SetTreeBroadphase(tree);
        ImGui::End();

        {
            Math::Vec2 dropPoint(640.0f, 100.0f);

            ImGui::Begin("Spawn");
            if (ImGui::Button("Box"))
                world.CreateBox(dropPoint, 20.0f, 20.0f, 1.0f);
            ImGui::SameLine();
            if (ImGui::Button("Circle"))
                world.CreateCircle(dropPoint, 15.0f, 1.0f);
            ImGui::SameLine();
            if (ImGui::Button("Cart"))
            {
                kx::Body* cart = world.CreateBody(kx::BodyType::Dynamic, dropPoint);
                cart->AddBox(15.0f, 5.0f, Math::Vec2(0.0f, 0.0f), 1.0f);
                cart->AddCircle(Math::Vec2(-20.0f, 12.0f), 8.0f, 1.0f);
                cart->AddCircle(Math::Vec2(20.0f, 12.0f), 8.0f, 1.0f);
            }

            if (ImGui::Button("Car"))
                cars.push_back(SpawnCar(world, dropPoint));
            ImGui::SameLine();
            if (ImGui::Button("Bunny") && bunnyPixels)
                world.CreateFromImage(dropPoint, bunnyPixels, bunnyWidth, bunnyHeight, 4, 128, 1.0f, 2.0f);
            ImGui::SameLine();
            if (ImGui::Button("Edge"))
                world.CreateEdge(dropPoint - Math::Vec2(100.0f, 0.0f), dropPoint + Math::Vec2(100.0f, 0.0f));

            if (ImGui::Button("Bridge"))
                SpawnBridge(world, Math::Vec2(640.0f, 500.0f));
            ImGui::SameLine();
            if (ImGui::Button("Gears"))
                SpawnGears(world, dropPoint);
            ImGui::SameLine();
            if (ImGui::Button("Blob"))
                SpawnBlob(world, dropPoint);
            ImGui::End();
        }

        if (cars.size() > 0)
        {
            ImGui::Begin("Car");
            bool springChanged = ImGui::SliderFloat("frequencyHz", &carFrequency, 0.5f, 10.0f);
            springChanged |= ImGui::SliderFloat("dampingRatio", &carDamping, 0.0f, 2.0f);
            if (springChanged)
            {
                for (size_t i = 0; i < cars.size(); ++i)
                {
                    cars[i].jointRear->SetSpring(carFrequency, carDamping);
                    cars[i].jointFront->SetSpring(carFrequency, carDamping);
                }
            }

            ImGui::SliderFloat("maxTorque", &carMaxTorque, 1.0e5f, 2.0e7f, "%.0f", ImGuiSliderFlags_Logarithmic);

            float gravityY = world.Gravity().y;
            if (ImGui::SliderFloat("gravity Y", &gravityY, 0.0f, 1500.0f))
                world.SetGravity(Math::Vec2(world.Gravity().x, gravityY));
            ImGui::End();
        }

        if (showProfiler)
            k2d::ShowProfilerWindow(&showProfiler);
        device.EndUI();

        device.Swap();

        if (device.IsGifCapturing())
            device.CaptureGifFrame();

        k2d::Profiler::Get().endFrame();
    }

    if (bunnyPixels)
        stbi_image_free(bunnyPixels);

    batch.Shutdown();
    device.Shutdown();

    return 0;
}
