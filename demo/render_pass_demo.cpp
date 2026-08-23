#include <k2d/k2d.h>

#include <glad/glad.h>

#include <cmath>

namespace
{
    unsigned int PackColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
    {
        return (unsigned int)r | ((unsigned int)g << 8) |
               ((unsigned int)b << 16) | ((unsigned int)a << 24);
    }

    void AddRect(k2d::RenderQueue &queue, k2d::Texture *texture,
                 const k2d::Matrix2D &transform, float width, float height,
                 unsigned int color, int zIndex)
    {
        k2d::RenderItem &item = queue.AddItem(zIndex);
        item.commands.push_back(k2d::RenderCommand::MakeTransform(transform));
        k2d::RenderCommand rect = k2d::RenderCommand::MakeRect(texture->Id(),
                                                                0.0f, 0.0f, width, height);
        rect.texWidth = texture->Width();
        rect.texHeight = texture->Height();
        rect.pivotX = 0.5f;
        rect.pivotY = 0.5f;
        rect.color = color;
        item.commands.push_back(rect);
    }
}

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Render Passes", 1280, 720, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    k2d::BatchRenderer batch;
    if (!canvas.Init() || !batch.Init())
        return 1;

    k2d::Assets assets;
    k2d::Pixmap whitePixmap(1, 1);
    whitePixmap.Clear(255, 255, 255, 255);
    k2d::Texture *white = whitePixmap.CreateTexture(assets, "pass_white");

    const int atlasWidth = 64;
    const int atlasHeight = 16;
    k2d::Pixmap atlasPixmap(atlasWidth, atlasHeight);
    for (int y = 0; y < atlasHeight; ++y)
    {
        for (int x = 0; x < atlasWidth; ++x)
        {
            int cell = x / 16;
            atlasPixmap.SetPixel(x, y,
                                 cell == 0 ? 255 : (cell == 1 ? 255 : 80),
                                 cell == 2 ? 210 : (cell == 1 ? 80 : 150),
                                 cell == 3 ? 255 : 40, 255);
        }
    }
    k2d::Texture *particleTexture = atlasPixmap.CreateTexture(assets, "pass_particles");

    k2d::ParticlePrefab litPrefab;
    litPrefab.direction = Math::Vec2(0.0f, -1.0f);
    litPrefab.speedMin = litPrefab.speedMax = 55.0f;
    litPrefab.lifeMin = litPrefab.lifeMax = 1.8f;
    litPrefab.sizeMin = litPrefab.sizeMax = 18.0f;
    litPrefab.colorStart = litPrefab.colorEnd = k2d::Color(1.0f, 0.55f, 0.15f, 1.0f);
    litPrefab.atlasBounds = Math::Vec4(0.0f, 0.0f, 16.0f, 16.0f);

    k2d::ParticleSystem litParticles(256);
    litParticles.SetTexture(particleTexture);
    litParticles.SetMode(k2d::ParticleMode::Loop);
    litParticles.SetPrefab(litPrefab);
    litParticles.SetEmitterPosition(Math::Vec2(410.0f, 500.0f));
    litParticles.SetEmissionRate(22.0f);
    litParticles.Start();

    k2d::ParticlePrefab unlitPrefab = litPrefab;
    unlitPrefab.colorStart = unlitPrefab.colorEnd = k2d::Color(0.25f, 0.8f, 1.0f, 1.0f);
    unlitPrefab.atlasBounds = Math::Vec4(32.0f, 0.0f, 16.0f, 16.0f);
    k2d::ParticleSystem unlitParticles(256);
    unlitParticles.SetTexture(particleTexture);
    unlitParticles.SetMode(k2d::ParticleMode::Loop);
    unlitParticles.SetPrefab(unlitPrefab);
    unlitParticles.SetEmitterPosition(Math::Vec2(870.0f, 500.0f));
    unlitParticles.SetEmissionRate(22.0f);
    unlitParticles.Start();

    ct::Vector<Math::Vec2> occluderPoints;
    occluderPoints.push_back(Math::Vec2(-35.0f, -120.0f));
    occluderPoints.push_back(Math::Vec2(35.0f, -120.0f));
    occluderPoints.push_back(Math::Vec2(35.0f, 120.0f));
    occluderPoints.push_back(Math::Vec2(-35.0f, 120.0f));

    k2d::PointLight point;
    point.position = Math::Vec2(640.0f, 330.0f);
    point.color = k2d::Color(1.0f, 0.72f, 0.35f, 2.2f);
    point.radius = 420.0f;
    point.useShadow = true;
    point.shadowFilter = k2d::SHADOW_FILTER_PCF5;
    point.shadowMatrix = k2d::Matrix2D::Identity();
    point.shadowColor = k2d::Color(0.0f, 0.0f, 0.0f, 0.85f);

    k2d::DirectionalLight directional;
    directional.direction = Math::Vec2(-0.6f, 0.8f).Normalized();
    directional.color = k2d::Color(0.25f, 0.3f, 0.42f, 0.55f);
    directional.useShadow = true;
    directional.shadowFilter = k2d::SHADOW_FILTER_PCF13;
    directional.shadowColor = k2d::Color(0.0f, 0.0f, 0.0f, 0.45f);

    bool running = true;
    while (running)
    {
        running = device.PollEvents();
        if (device.GetInput().KeyDown(41))
            running = false;

        float dt = device.DeltaTime();
        litParticles.Update(dt);
        unlitParticles.Update(dt);

        glClearColor(0.025f, 0.03f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetOrtho((float)device.Width(), (float)device.Height());
        batch.Resize(device.Width(), device.Height());
        batch.SetProjection(Math::Mat4::Ortho(0.0f, (float)device.Width(),
                                       (float)device.Height(), 0.0f, -1.0f, 1.0f));
        batch.BeginFrame();

        k2d::RenderQueue queue;
        AddRect(queue, white, k2d::Matrix2D::Translation(640.0f, 360.0f),
                1280.0f, 720.0f, PackColor(22, 26, 38), -10);
        AddRect(queue, white, k2d::Matrix2D::Translation(640.0f, 360.0f),
                1000.0f, 28.0f, PackColor(115, 55, 45), 0);

        k2d::Occluder occluder;
        occluder.xform = k2d::Matrix2D::Translation(640.0f, 360.0f);
        occluder.points = &occluderPoints;
        occluder.version = 1;
        queue.AddLight(point);
        queue.AddDirectionalLight(directional);
        queue.AddOccluder(occluder);
        litParticles.Submit(queue, 2);
        queue.Flush(canvas);

        unlitParticles.Draw(batch);
        batch.SetColor((unsigned char)255, (unsigned char)255,
                       (unsigned char)255, (unsigned char)255);
        batch.DrawText(24.0f, 24.0f, 20.0f, "DIRECT RENDER PASS TEST");
        batch.DrawText(24.0f, 52.0f, 16.0f,
                       "orange: CanvasRenderer + lights/shadows   blue: BatchRenderer without lights");
        batch.EndFrame();
        device.Swap();
    }

    assets.Clear();
    batch.Shutdown();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}