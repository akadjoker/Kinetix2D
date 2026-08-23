#include <k2d/k2d.h>

#include <glad/glad.h>
#include <mathc.h>
#include <imgui.h>

#include <cmath>

namespace
{
    void AddRect(k2d::RenderQueue &queue, k2d::Texture *texture, float x, float y,
                 float width, float height, unsigned int color, int z)
    {
        k2d::RenderItem &item = queue.AddItem(z);
        item.commands.push_back(k2d::RenderCommand::MakeTransform(
            k2d::Matrix2D::Translation(x, y)));
        k2d::RenderCommand rect = k2d::RenderCommand::MakeRect(texture->Id(), 0.0f, 0.0f,
                                                                 width, height);
        rect.texWidth = texture->Width();
        rect.texHeight = texture->Height();
        rect.pivotX = 0.5f;
        rect.pivotY = 0.5f;
        rect.color = color;
        item.commands.push_back(rect);
    }

    unsigned int Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
    {
        return (unsigned int)r | ((unsigned int)g << 8) |
               ((unsigned int)b << 16) | ((unsigned int)a << 24);
    }
}

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Particle FX", 1280, 720, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;
    k2d::Texture *particleTexture = assets.LoadTexture("fire", "assets/fire.png");
    if (!particleTexture)
        particleTexture = assets.LoadTexture("fire", "../../assets/fire.png");
    if (!particleTexture)
        return 1;
    const unsigned char whitePixel[4] = {255, 255, 255, 255};
    k2d::Texture *white = assets.CreateTexture("fx_white", 1, 1, whitePixel);

    const Math::Vec2 firePosition(360.0f, 590.0f);
    k2d::ParticlePrefab firePrefab;
    firePrefab.direction = Math::Vec2(0.0f, -1.0f);
    firePrefab.spreadDegrees = 14.0f;
    firePrefab.speedMin = 65.0f;
    firePrefab.speedMax = 105.0f;
    firePrefab.lifeMin = 0.6f;
    firePrefab.lifeMax = 1.0f;
    firePrefab.sizeMin = 22.0f;
    firePrefab.sizeMax = 32.0f;
    firePrefab.endSize = 8.0f;
    firePrefab.rotationMin = 0.0f;
    firePrefab.rotationMax = 360.0f;
    firePrefab.angularVelocityMin = -60.0f;
    firePrefab.angularVelocityMax = 60.0f;

    firePrefab.colorStart = k2d::Color(1.0f, 0.95f, 0.55f, 0.9f);
    firePrefab.colorEnd = k2d::Color(0.9f, 0.25f, 0.05f, 0.0f);
    firePrefab.fadeIn = 0.05f;
    firePrefab.atlasBounds = Math::Vec4(0.0f, 0.0f, 32.0f, 32.0f);
    k2d::ParticleSystem fire(220);
    fire.SetTexture(particleTexture); fire.SetMode(k2d::ParticleMode::Loop);
    fire.SetPrefab(firePrefab); fire.SetEmitterPosition(firePosition);
    fire.SetEmitterShape(k2d::ParticleEmitterShape::Rectangle);
    fire.SetEmitterSize(Math::Vec2(18.0f, 4.0f));
    fire.SetEmissionRate(34.0f); fire.Start();

    k2d::ParticlePrefab smokePrefab = firePrefab;
    smokePrefab.spreadDegrees = 26.0f;
    smokePrefab.speedMin = 28.0f;
    smokePrefab.speedMax = 48.0f;
    smokePrefab.lifeMin = 2.2f; smokePrefab.lifeMax = 3.4f;
    smokePrefab.sizeMin = 30.0f; smokePrefab.sizeMax = 46.0f;
    smokePrefab.endSize = 60.0f; 
    smokePrefab.angularVelocityMin = -20.0f;
    smokePrefab.angularVelocityMax = 20.0f;
    smokePrefab.colorStart = k2d::Color(0.7f, 0.72f, 0.78f, 0.42f);
    smokePrefab.colorEnd = k2d::Color(0.5f, 0.5f, 0.55f, 0.0f);
    smokePrefab.fadeIn = 0.15f;
    smokePrefab.atlasBounds = Math::Vec4(0.0f, 0.0f, 32.0f, 32.0f);
    k2d::ParticleSystem smoke(100);
    smoke.SetTexture(particleTexture); smoke.SetMode(k2d::ParticleMode::Loop);
    smoke.SetPrefab(smokePrefab); smoke.SetEmitterPosition(firePosition + Math::Vec2(0, -25));
    smoke.SetEmitterShape(k2d::ParticleEmitterShape::Circle);
    smoke.SetEmitterRadius(12.0f);
    smoke.SetEmissionRate(10.0f); smoke.SetGravity(Math::Vec2(10.0f, -4.0f)); smoke.Start();

    k2d::ParticleSystem fireworks(700);
    fireworks.SetTexture(particleTexture);
    float fireworkTimer = 0.0f;
    int burst = 0;
    bool running = true;
    while (running)
    {
        running = device.PollEvents();
        if (device.GetInput().KeyDown(41)) running = false;
        float dt = device.DeltaTime();
        fire.Update(dt); smoke.Update(dt); fireworks.Update(dt);

        fireworkTimer -= dt;
        if (fireworkTimer <= 0.0f)
        {
            fireworkTimer = 2.2f;
            Math::Vec2 center(760.0f + (burst % 3) * 120.0f, 180.0f + (burst % 2) * 70.0f);
            ++burst;
            for (int i = 0; i < 48; ++i)
            {
                float angle = (float)i / 48.0f * 6.2831853f;
                float speed = 75.0f + (float)((i * 17) % 45);
                fireworks.Emit(center, Math::Vec2(std::cos(angle), std::sin(angle)) * speed,
                                1.5f, 14.0f, k2d::Color(1.0f, 0.65f + (i % 3) * 0.1f, 0.2f, 1.0f),
                                0.0f, 0.0f, Math::Vec4(0.0f, 0.0f, 32.0f, 32.0f));
            }
        }

        float width = (float)device.Width(), height = (float)device.Height();
        glClearColor(0.018f, 0.022f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetOrtho(width, height);
        k2d::RenderQueue queue;
        AddRect(queue, white, width * 0.5f, height * 0.5f, width, height, Color(10, 14, 28), -10);
        AddRect(queue, white, firePosition.x, firePosition.y + 22.0f, 100.0f, 18.0f, Color(75, 45, 35), -1);
        k2d::PointLight light;
        light.position = firePosition; light.color = k2d::Color(1.0f, 0.35f, 0.08f, 2.0f);
        light.radius = 240.0f; light.useShadow = false;
        queue.AddLight(light);
        smoke.Submit(queue, 1);
        fire.Submit(queue, 2);
        fireworks.Submit(queue, 3);
        queue.Flush(canvas);

        device.BeginUI();
        ImGui::Begin("Particle FX");
        ImGui::Text("fogueira: fogo + smoke (Loop)");
        ImGui::Text("fireworks: explosao emitida manualmente");
        ImGui::Text("Fire: %d  Smoke: %d  Fireworks: %d", (int)fire.ActiveCount(),
                    (int)smoke.ActiveCount(), (int)fireworks.ActiveCount());
        ImGui::Text("ESC quit");
        ImGui::End();
        device.EndUI();
        device.Swap();
    }

    assets.Clear();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}