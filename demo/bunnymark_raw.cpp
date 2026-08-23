#include <k2d/k2d.h>

#include <glad/glad.h>
#include <ct/string.hpp>
#include <ct/vector.hpp>
#include <cstdio>
#include <cstdlib>

static const int SCANCODE_ESCAPE = 41;
static const int SCANCODE_SPACE = 44;
static const int SCANCODE_R = 21;
static const int SCANCODE_P = 19;

struct Bunny
{
    float x, y, vx, vy;
};

static unsigned gSeed = 12345;
static float Rnd()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return (float)(gSeed >> 8) / 16777216.0f;
}

static void Spawn(ct::Vector<Bunny> &bunnies, int count)
{
    for (int i = 0; i < count; ++i)
    {
        Bunny b;
        b.x = 10.0f;
        b.y = 10.0f;
        b.vx = Rnd() * 500.0f;
        b.vy = Rnd() * 250.0f - 125.0f;
        bunnies.push_back(b);
    }
}

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Bunnymark", 1280, 720, false))
        return 1;

    k2d::BatchRenderer batch;
    k2d::BatchRenderer::Config config;
    config.maxVertices = 65532;
    config.maxDrawCalls = 4096;
    if (!batch.Init(config))
        return 1;

    batch.Resize(device.Width(), device.Height());

    k2d::Assets assets;
    k2d::Texture *bunnyTex = assets.LoadTexture("bunny", "assets/wabbit_alpha.png");
    if (!bunnyTex)
        bunnyTex = assets.LoadTexture("bunny", "../../assets/wabbit_alpha.png");
    if (!bunnyTex)
        return 1;

    const float bw = (float)bunnyTex->Width();
    const float bh = (float)bunnyTex->Height();
    const unsigned texId = bunnyTex->Id();
    const int texW = bunnyTex->Width();
    const int texH = bunnyTex->Height();

    ct::Vector<Bunny> bunnies;
    bunnies.reserve(400000);
    Spawn(bunnies, 100000);

    float fpsSmooth = 0.0f;
    float gravity = 400.0f;
    float printTimer = 0.0f;

    bool running = true;
    while (running)
    {
        running = device.PollEvents();

        k2d::Input &input = device.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;
        if (input.KeyPressed(SCANCODE_R))
            bunnies.clear();
        if (input.KeyDown(SCANCODE_SPACE) || input.MouseDown(0))
            Spawn(bunnies, 2500);
        if (input.KeyPressed(SCANCODE_P))
            batch.PrintStats();

        if (device.WasResized())
        {
            batch.Resize(device.Width(), device.Height());
            glViewport(0, 0, device.Width(), device.Height());
        }

        float dt = device.DeltaTime();
        if (dt > 0.0f)
            fpsSmooth = fpsSmooth * 0.95f + (1.0f / dt) * 0.05f;
        printTimer += dt;
        if (printTimer >= 2.0f)
        {
            printTimer = 0.0f;
            std::printf("bunnies=%zu fps=%d\n", bunnies.size(), (int)fpsSmooth);
            std::fflush(stdout);
        }

        const float maxX = (float)device.Width() - bw;
        const float maxY = (float)device.Height() - bh;

        Bunny *data = bunnies.data();
        const std::size_t count = bunnies.size();
        for (std::size_t i = 0; i < count; ++i)
        {
            Bunny &b = data[i];
            b.x += b.vx * dt;
            b.y += b.vy * dt;
            b.vy += gravity * dt;

            if (b.x > maxX)
            {
                b.x = maxX;
                b.vx = -b.vx;
            }
            else if (b.x < 0.0f)
            {
                b.x = 0.0f;
                b.vx = -b.vx;
            }
            if (b.y > maxY)
            {
                b.y = maxY;
                b.vy = -b.vy * 0.85f;
                if (Rnd() > 0.5f)
                    b.vy -= Rnd() * 200.0f;
            }
            else if (b.y < 0.0f)
            {
                b.y = 0.0f;
                b.vy = 0.0f;
            }
        }

        glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        batch.BeginFrame();
        batch.ResetStats();
        batch.SetColor((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255);

        batch.BeginSpriteRun(texId);
        for (std::size_t i = 0; i < count; ++i)
            batch.SubmitSprite(data[i].x, data[i].y, bw, bh);
        batch.EndSpriteRun();

        ct::String info;
        info += "bunnies ";
        info.append_number((unsigned long long)count);
        info += "  FPS ";
        info.append_number((int)fpsSmooth);
        batch.SetColor((unsigned char)0, (unsigned char)0, (unsigned char)0, (unsigned char)180);
        batch.DrawRect(10.0f, 10.0f, 380.0f, 40.0f, true);
        batch.SetColor((unsigned char)255, (unsigned char)255, (unsigned char)80, (unsigned char)255);
        batch.DrawText(20.0f, 20.0f, 20.0f, info.c_str());

        batch.EndFrame();
        device.Swap();
    }

    return 0;
}