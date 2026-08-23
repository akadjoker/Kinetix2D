#pragma once

#include "../common.h"

class Testbed;

typedef class Demo *(*DemoCreateFcn)(Testbed &tb);

class Demo
{
public:
    Demo(Testbed &tb, const Math::Vec2 &gravity);
    virtual ~Demo();

    virtual void Step(float) {}
    virtual void UpdateUI() {}
    virtual void Draw() {}

    kx::World &World() { return *mWorld; }
    Testbed &T() { return mTB; }

private:
    Testbed &mTB;
    kx::World *mWorld;
};

struct DemoEntry
{
    const char *category;
    const char *name;
    DemoCreateFcn create;
};

int TestbedRegisterDemo(const char *category, const char *name, DemoCreateFcn create);
const DemoEntry *TestbedDemos();
int TestbedDemoCount();

inline Math::Mat4 CameraProjection(const Camera &cam, float screenW, float screenH)
{
    float hw = screenW * 0.5f / cam.zoom;
    float hh = screenH * 0.5f / cam.zoom;
    return Math::Mat4::Ortho(cam.center.x - hw, cam.center.x + hw, cam.center.y - hh, cam.center.y + hh, -1.0f, 1.0f);
}

inline Math::Vec2 CameraScreenToWorld(const Camera &cam, float sx, float sy, float screenW, float screenH)
{
    return Math::Vec2(cam.center.x + (sx - screenW * 0.5f) / cam.zoom,
                     cam.center.y - (sy - screenH * 0.5f) / cam.zoom);
}

class Testbed
{
public:
    Testbed();
    ~Testbed();

    bool Init();
    void Run();

    k2d::Device &Device() { return mDevice; }
    k2d::Input &Input() { return mDevice.GetInput(); }
    k2d::BatchRenderer &Batch() { return mBatch; }
    k2d::Assets &Assets() { return mAssets; }
    kx::DebugDraw &DebugDraw() { return mDebugDraw; }
    Camera &Cam() { return mCamera; }

    Math::Vec2 MouseWorld();
    unsigned DrawFlags() const { return mDrawFlags; }

    void SwitchDemo(int index);
    void RestartDemo();
    Demo *Current() { return mDemo; }
    void SetStatus(const char *fmt, ...);

private:
    void HandleInput(float dt);
    void UpdateMouseGrab();
    void DrawScene();
    void DrawUI();

    k2d::Device mDevice;
    k2d::BatchRenderer mBatch;
    k2d::Assets mAssets;
    StressDebugDraw mDebugDraw;
    Camera mCamera;
    Demo *mDemo;
    kx::MouseJoint *mGrab;
    unsigned mDrawFlags;
    int mDemoIndex;
    bool mPaused;
    bool mSingleStep;
    bool mShowUI;
    bool mShowProfiler;
    float mAccumulator;
    char mStatus[256];
};

int TestbedMain();