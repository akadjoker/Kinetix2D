#include "testbed.h"

#include <glad/glad.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

static const int SCANCODE_P = 19;
static const int SCANCODE_T = 23;
static const int SCANCODE_GRAVE = 53;
static const int SCANCODE_F5 = 62;
static const int SCANCODE_F9 = 66;
static const int SCANCODE_F10 = 67;
static const int SCANCODE_LEFTBRACKET = 47;
static const int SCANCODE_RIGHTBRACKET = 48;

namespace
{

    ct::Vector<DemoEntry> &DemoRegistry()
    {
        static ct::Vector<DemoEntry> sDemos;
        return sDemos;
    }

} 

int TestbedRegisterDemo(const char *category, const char *name, DemoCreateFcn create)
{
    DemoEntry entry;
    entry.category = category;
    entry.name = name;
    entry.create = create;
    DemoRegistry().push_back(entry);
    return (int)DemoRegistry().size() - 1;
}

const DemoEntry *TestbedDemos()
{
    return DemoRegistry().data();
}

int TestbedDemoCount()
{
    return (int)DemoRegistry().size();
}

Demo::Demo(Testbed &tb, const glm::vec2 &gravity)
    : mTB(tb), mWorld(new kx::World(gravity))
{
    mWorld->SetTimeSource(&k2d::Device::TimeSeconds);
}

Demo::~Demo()
{
    delete mWorld;
}

Testbed::Testbed()
    : mDebugDraw(mBatch), mDemo(nullptr), mGrab(nullptr),
      mDrawFlags(kx::DebugDrawShapes | kx::DebugDrawJoints), mDemoIndex(0),
      mPaused(false), mSingleStep(false), mShowUI(true), mShowProfiler(true),
      mAccumulator(0.0f)
{
    mStatus[0] = 0;
}

Testbed::~Testbed()
{
    if (mDemo)
    {
        delete mDemo;
        mDemo = nullptr;
    }
}

bool Testbed::Init()
{
    if (!mDevice.Init("Kinetix2D Testbed", 1280, 720, true))
        return false;

    k2d::BatchRenderer::Config config;
    config.maxVertices = 65532;
    config.maxDrawCalls = 4096;
    if (!mBatch.Init(config))
        return false;

    mBatch.Resize(mDevice.Width(), mDevice.Height());

    mCamera.center = glm::vec2(0.0f, 0.0f);
    mCamera.zoom = 1.5f;

    k2d::Profiler::Get().SetEnabled(true);

    if (TestbedDemoCount() == 0)
        return false;

    mDemoIndex = 0;
    mDemo = TestbedDemos()[0].create(*this);

    return true;
}

void Testbed::SwitchDemo(int index)
{
    if (TestbedDemoCount() == 0)
        return;

    if (index < 0)
        index = TestbedDemoCount() - 1;
    if (index >= TestbedDemoCount())
        index = 0;

    if (mDemo)
    {
        delete mDemo;
        mDemo = nullptr;
    }

    mGrab = nullptr;
    mDemoIndex = index;
    mAccumulator = 0.0f;
    mStatus[0] = 0;
    mDemo = TestbedDemos()[index].create(*this);
}

void Testbed::RestartDemo()
{
    SwitchDemo(mDemoIndex);
}

void Testbed::SetStatus(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(mStatus, sizeof(mStatus), fmt, args);
    va_end(args);
}

glm::vec2 Testbed::MouseWorld()
{
    k2d::Input &input = mDevice.GetInput();
    return CameraScreenToWorld(mCamera, input.MouseX(), input.MouseY(),
                               (float)mDevice.Width(), (float)mDevice.Height());
}

void Testbed::HandleInput(float dt)
{
    k2d::Input &input = mDevice.GetInput();

    if (input.KeyPressed(SCANCODE_LEFTBRACKET))
        SwitchDemo(mDemoIndex - 1);
    if (input.KeyPressed(SCANCODE_RIGHTBRACKET))
        SwitchDemo(mDemoIndex + 1);

    if (input.KeyPressed(SCANCODE_R))
        RestartDemo();
    if (input.KeyPressed(SCANCODE_P))
        mPaused = !mPaused;
    if (input.KeyPressed(SCANCODE_T))
        mSingleStep = true;
    if (input.KeyPressed(SCANCODE_GRAVE))
        mShowUI = !mShowUI;

    if (input.KeyPressed(SCANCODE_F1))
        mDrawFlags ^= kx::DebugDrawShapes;
    if (input.KeyPressed(SCANCODE_F2))
        mDrawFlags ^= kx::DebugDrawAABBs;
    if (input.KeyPressed(SCANCODE_F3))
        mDrawFlags ^= kx::DebugDrawContacts;
    if (input.KeyPressed(SCANCODE_F4))
        mDrawFlags ^= kx::DebugDrawJoints;
    if (input.KeyPressed(SCANCODE_F5))
        mShowProfiler = !mShowProfiler;

    if (input.KeyPressed(SCANCODE_F9))
        mDevice.CaptureScreenshot();
    if (input.KeyPressed(SCANCODE_F10))
    {
        if (mDevice.IsGifCapturing())
            mDevice.StopGifCapture();
        else
            mDevice.StartGifCapture(60);
    }

    float panSpeed = 500.0f / mCamera.zoom * dt;
    if (input.KeyDown(SCANCODE_W))
        mCamera.center.y += panSpeed;
    if (input.KeyDown(SCANCODE_S))
        mCamera.center.y -= panSpeed;
    if (input.KeyDown(SCANCODE_A))
        mCamera.center.x -= panSpeed;
    if (input.KeyDown(SCANCODE_D))
        mCamera.center.x += panSpeed;
    if (input.WheelY() != 0.0f)
    {
        mCamera.zoom *= input.WheelY() > 0.0f ? 1.1f : 0.9f;
        if (mCamera.zoom < 0.05f)
            mCamera.zoom = 0.05f;
        if (mCamera.zoom > 10.0f)
            mCamera.zoom = 10.0f;
    }
}

void Testbed::UpdateMouseGrab()
{
    if (!mDemo)
        return;

    k2d::Input &input = mDevice.GetInput();
    glm::vec2 mouse = MouseWorld();

    if (input.MousePressed(0) && !mGrab && !mDevice.ImGuiWantsMouse())
    {
        kx::Body *picked = mDemo->World().BodyAtPoint(mouse);
        if (picked)
        {
            mGrab = new kx::MouseJoint(picked, mouse, 5000.0f * picked->Mass());
            mDemo->World().AddJoint(mGrab);
        }
    }

    if (mGrab)
    {
        if (input.MouseDown(0))
        {
            mGrab->SetTarget(mouse);
        }
        else
        {
            mDemo->World().DestroyJoint(mGrab);
            mGrab = nullptr;
        }
    }
}

void Testbed::DrawScene()
{
    if (mDevice.WasResized())
    {
        mBatch.Resize(mDevice.Width(), mDevice.Height());
        glViewport(0, 0, mDevice.Width(), mDevice.Height());
    }

    glClearColor(0.05f, 0.07f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    mBatch.BeginFrame();

    mBatch.SetProjection(CameraProjection(mCamera, (float)mDevice.Width(), (float)mDevice.Height()));
    if (mDrawFlags != 0)
        kx::Draw(mDemo->World(), mDebugDraw, mDrawFlags);
    mDemo->Draw();
    mBatch.DrawRenderBatch();

    mBatch.EndFrame();
}

void Testbed::DrawUI()
{
    if (!mShowUI)
        return;

    ImGui::Begin("Testbed");
    {
        const DemoEntry &entry = TestbedDemos()[mDemoIndex];
        ImGui::Text("%s / %s", entry.category, entry.name);
        if (mStatus[0] != 0)
        {
            ImGui::Separator();
            ImGui::TextWrapped("%s", mStatus);
        }
        ImGui::Separator();

        ImGui::Text("bodies %d   contacts %d", (int)mDemo->World().BodyCount(), (int)mDemo->World().ContactCount());
        float dt = mDevice.DeltaTime();
        ImGui::Text("%.1f fps   %.2f ms", 1.0f / (dt > 0.0f ? dt : 1.0f), dt * 1000.0f);

        bool tree = mDemo->World().TreeBroadphase();
        if (ImGui::Checkbox("tree broadphase", &tree))
            mDemo->World().SetTreeBroadphase(tree);
        ImGui::Checkbox("pause", &mPaused);
        ImGui::SameLine();
        if (ImGui::Button("restart"))
            RestartDemo();
        if (ImGui::Button("prev demo"))
            SwitchDemo(mDemoIndex - 1);
        ImGui::SameLine();
        if (ImGui::Button("next demo"))
            SwitchDemo(mDemoIndex + 1);

        ImGui::Separator();
        ImGui::CheckboxFlags("shapes", &mDrawFlags, kx::DebugDrawShapes);
        ImGui::SameLine();
        ImGui::CheckboxFlags("aabbs", &mDrawFlags, kx::DebugDrawAABBs);
        ImGui::CheckboxFlags("contacts", &mDrawFlags, kx::DebugDrawContacts);
        ImGui::SameLine();
        ImGui::CheckboxFlags("joints", &mDrawFlags, kx::DebugDrawJoints);
        ImGui::Separator();
        ImGui::TextWrapped("[ ] demo   R restart   P pause   T step   ` UI   WASD+wheel camera   mouse drag grab   F1-F4 draw   F5 profiler   F9 shot   F10 gif");
        ImGui::Separator();

        const char *lastCategory = nullptr;
        for (int i = 0; i < TestbedDemoCount(); ++i)
        {
            const DemoEntry &de = TestbedDemos()[i];
            if (lastCategory == nullptr || strcmp(lastCategory, de.category) != 0)
            {
                ImGui::Separator();
                ImGui::Text("%s", de.category);
                lastCategory = de.category;
            }
            if (ImGui::Selectable(de.name, i == mDemoIndex))
                SwitchDemo(i);
        }
    }
    ImGui::End();

    if (mDemo)
        mDemo->UpdateUI();

    if (mShowProfiler)
        k2d::ShowProfilerWindow(&mShowProfiler);
}

void Testbed::Run()
{
    bool running = true;
    while (running)
    {
        k2d::Profiler::Get().beginFrame();
        running = mDevice.PollEvents();

        k2d::Input &input = mDevice.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;

        float dt = mDevice.DeltaTime();
        HandleInput(dt);
        UpdateMouseGrab();

        if (!mPaused || mSingleStep)
        {
            mDemo->Step(dt);

            mAccumulator += dt;
            if (mAccumulator > 4.0f / 60.0f)
                mAccumulator = 4.0f / 60.0f;

            {
                k2d::ProfileScope profileStep("physics.step");
                while (mAccumulator >= 1.0f / 60.0f)
                {
                    mDemo->World().Step(1.0f / 60.0f);
                    mAccumulator -= 1.0f / 60.0f;
                }
            }

            const kx::StepProfile &sp = mDemo->World().Profile();
            k2d::Profiler::Get().addSample("kx.broadphase", sp.broadphase);
            k2d::Profiler::Get().addSample("kx.narrowphase", sp.narrowphase);
            k2d::Profiler::Get().addSample("kx.solve.vel", sp.solveVelocity);
            k2d::Profiler::Get().addSample("kx.solve.pos", sp.solvePosition);
            k2d::Profiler::Get().addSample("kx.integrate", sp.integrate);

            mSingleStep = false;
        }

        DrawScene();

        mDevice.BeginUI();
        DrawUI();
        mDevice.EndUI();

        mDevice.Swap();

        if (mDevice.IsGifCapturing())
            mDevice.CaptureGifFrame();

        k2d::Profiler::Get().endFrame();
    }
}

int TestbedMain()
{
    Testbed testbed;
    if (!testbed.Init())
        return 1;

    testbed.Run();
    return 0;
}

int main()
{
    return TestbedMain();
}