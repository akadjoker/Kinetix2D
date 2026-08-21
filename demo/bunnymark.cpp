#include <k2d/k2d.h>

#include <glad/glad.h>
#include <ct/string.hpp>
#include <cstdio>

static const int SCANCODE_ESCAPE = 41;
static const int SCANCODE_SPACE = 44;
static const int SCANCODE_R = 21;
static const int SCANCODE_P = 19;
static const int SCANCODE_1 = 30;
static const int SCANCODE_2 = 31;
static const int SCANCODE_F4 = 61;
static const int SCANCODE_F5 = 62;

static unsigned gSeed = 12345;
static float Rnd()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return (float)(gSeed >> 8) / 16777216.0f;
}

static float gMaxX = 1280.0f;
static float gMaxY = 720.0f;
static const float GRAVITY = 400.0f;
static bool gAdditive = false;

class BunnyBehavior : public k2d::ScriptComponent
{
public:
    BunnyBehavior()
        : mVx(Rnd() * 500.0f), mVy(Rnd() * 250.0f - 125.0f)
    {
    }

protected:
    void onUpdate(float dt) override
    {
        glm::vec2 p = owner()->position();
        p.x += mVx * dt;
        p.y += mVy * dt;
        mVy += GRAVITY * dt;

        if (p.x > gMaxX)
        {
            p.x = gMaxX;
            mVx = -mVx;
        }
        else if (p.x < 0.0f)
        {
            p.x = 0.0f;
            mVx = -mVx;
        }
        if (p.y > gMaxY)
        {
            p.y = gMaxY;
            mVy = -mVy * 0.85f;
            if (Rnd() > 0.5f)
                mVy -= Rnd() * 200.0f;
        }
        else if (p.y < 0.0f)
        {
            p.y = 0.0f;
            mVy = 0.0f;
        }
        owner()->setPosition(p);
    }

private:
    float mVx;
    float mVy;
};

static void Spawn(k2d::Scene &scene, k2d::Texture *tex, int count)
{
    for (int i = 0; i < count; ++i)
    {
        k2d::GameObject *bunny = scene.createObject("b");
        bunny->setPosition(glm::vec2(10.0f, 10.0f));
        k2d::SpriteComponent *sprite = bunny->addComponent<k2d::SpriteComponent>(tex);
        sprite->setPivot(glm::vec2(0.0f, 0.0f));
        bunny->addComponent<BunnyBehavior>();
    }
}

static void Clear(k2d::Scene &scene)
{
    k2d::GameObject &root = scene.root();
    for (std::size_t i = 0; i < root.childCount(); ++i)
        scene.destroy(root.child(i));
}

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Bunnymark (componentes)", 1280, 720, false))
        return 1;

    k2d::BatchRenderer batch;
    k2d::BatchRenderer::Config config;
    config.maxVertices = 65532;
    config.maxDrawCalls = 26;
    if (!batch.Init(config))
        return 1;

    batch.Resize(device.Width(), device.Height());

    k2d::CanvasRenderer canvas;
    k2d::CanvasRenderer::Config canvasConfig;
    canvasConfig.maxVertices = 65532;
    canvasConfig.maxDrawCalls = 64;
    if (!canvas.Init(canvasConfig))
        return 1;

    canvas.SetOrtho((float)device.Width(), (float)device.Height());

    k2d::Assets assets;
    k2d::Texture *bunnyTex = assets.LoadTexture("bunny", "assets/wabbit_alpha.png");
    if (!bunnyTex)
        bunnyTex = assets.LoadTexture("bunny", "../../assets/wabbit_alpha.png");
    if (!bunnyTex)
        return 1;

    k2d::Scene scene;
    Spawn(scene, bunnyTex, 1000);

    float fpsSmooth = 0.0f;
    float printTimer = 0.0f;
    bool showProfiler = true;

    bool running = true;
    while (running)
    {
        running = device.PollEvents();

        k2d::Profiler::Get().beginFrame();

        k2d::Input &input = device.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;
        if (input.KeyPressed(SCANCODE_R))
            Clear(scene);
        if (input.KeyPressed(SCANCODE_1))
            Spawn(scene, bunnyTex, 100);
        if (input.KeyPressed(SCANCODE_2))
            Spawn(scene, bunnyTex, 1000);
        if (input.KeyPressed(SCANCODE_F4))
            showProfiler = !showProfiler;
        if (input.KeyPressed(SCANCODE_F5))
        {
            gAdditive = !gAdditive;
            k2d::GameObject &root = scene.root();
            for (std::size_t i = 0; i < root.childCount(); ++i)
            {
                k2d::SpriteComponent *s = root.child(i)->getComponent<k2d::SpriteComponent>();
                if (s)
                    s->setBlendMode(gAdditive ? k2d::BLEND_ADD : k2d::BLEND_MIX);
            }
        }
        if (input.KeyDown(SCANCODE_SPACE) || (input.MouseDown(0) && !device.ImGuiWantsMouse()))
            Spawn(scene, bunnyTex, 2500);
        if (input.KeyPressed(SCANCODE_P))
            canvas.PrintStats();

        if (device.WasResized())
        {
            batch.Resize(device.Width(), device.Height());
            canvas.SetOrtho((float)device.Width(), (float)device.Height());
            glViewport(0, 0, device.Width(), device.Height());
        }

        float dt = device.DeltaTime();
        gMaxX = (float)device.Width() - (float)bunnyTex->Width();
        gMaxY = (float)device.Height() - (float)bunnyTex->Height();

        if (dt > 0.0f)
            fpsSmooth = fpsSmooth * 0.95f + (1.0f / dt) * 0.05f;
        printTimer += dt;
        if (printTimer >= 2.0f)
        {
            printTimer = 0.0f;
            std::printf("bunnies=%zu fps=%d\n", scene.objectCount(), (int)fpsSmooth);
            std::fflush(stdout);
        }

        {
            k2d::ProfileScope updateScope("scene.update");
            scene.update(dt);
        }

        glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        batch.BeginFrame();
        batch.ResetStats();
        batch.SetColor((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255);

        {
            k2d::ProfileScope renderScope("scene.render");
            scene.render(canvas);
        }

        ct::String info;
        info += "bunnies ";
        info.append_number((unsigned long long)scene.objectCount());
        info += "  FPS ";
        info.append_number((int)fpsSmooth);
        batch.SetColor((unsigned char)0, (unsigned char)0, (unsigned char)0, (unsigned char)180);
        batch.DrawRect(10.0f, 10.0f, 380.0f, 64.0f, true);
        batch.SetColor((unsigned char)255, (unsigned char)255, (unsigned char)80, (unsigned char)255);
        batch.DrawText(20.0f, 16.0f, 20.0f, info.c_str());
        batch.SetColor((unsigned char)200, (unsigned char)210, (unsigned char)220, (unsigned char)255);
        batch.DrawText(20.0f, 44.0f, 14.0f, "1:+100  2:+1000  SPACE:+2500  R:clear  F5:additive blend  P:stats");

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
