#include <k2d/k2d.h>

#include <imgui.h>

#include <cstdio>

namespace
{
class BulletScript : public k2d::ScriptComponent
{
  public:
    void Fire(const Math::Vec2& velocity, float lifetime)
    {
        mVelocity = velocity;
        mLifetime = lifetime;
    }

  protected:
    void onUpdate(float deltaTime) override
    {
        owner()->translate(mVelocity * deltaTime);
        mLifetime -= deltaTime;
        if (mLifetime <= 0.0f)
            owner()->dispose();
    }

  private:
    Math::Vec2 mVelocity{0.0f, 0.0f};
    float mLifetime = 0.0f;
};
} // namespace

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Prefab Bullet Demo", 1280, 720, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;
    k2d::Pixmap bulletPixmap(6, 6);
    bulletPixmap.Clear(255, 255, 255, 255);
    k2d::Texture* bulletTexture = bulletPixmap.CreateTexture(assets, "bullet");

    k2d::Pixmap gunPixmap(16, 16);
    gunPixmap.Clear(255, 255, 255, 255);
    k2d::Texture* gunTexture = gunPixmap.CreateTexture(assets, "gun");

    // The prefab only carries what k2d::Serializer knows how to write/read
    // (there's no ComponentType::Script entry in its type table -- a
    // user-defined ScriptComponent subclass like BulletScript has no
    // registered name/factory for it to serialize by, so it would be
    // silently dropped if added here). BulletScript is attached fresh after
    // each Instantiate() instead; the prefab itself is just the visual.
    k2d::Scene templateScene;
    k2d::GameObject* bulletTemplate = templateScene.createObject("Bullet");
    k2d::SpriteComponent* templateSprite = bulletTemplate->addComponent<k2d::SpriteComponent>(bulletTexture);
    templateSprite->setColor(255, 200, 40, 255);

    k2d::Prefab bulletPrefab;
    bulletPrefab.SaveFromObject(*bulletTemplate, &assets);

    k2d::Scene scene;
    const Math::Vec2 gunPos(1280.0f * 0.5f, 720.0f - 40.0f);
    k2d::GameObject* gun = scene.createObject("Gun");
    gun->setPosition(gunPos);
    k2d::SpriteComponent* gunSprite = gun->addComponent<k2d::SpriteComponent>(gunTexture);
    gunSprite->setColor(120, 200, 255, 255);

    const float kBulletSpeed = 700.0f;
    const float kBulletLifetime = 2.5f;
    int shotsFired = 0;

    bool running = true;
    while (running)
    {
        running = device.PollEvents();
        const k2d::Input& input = device.GetInput();
        if (input.KeyDown(41))
            running = false;

        float dt = device.DeltaTime();

        if (input.MousePressed(0) && !device.ImGuiWantsMouse())
        {
            Math::Vec2 target(input.MouseX(), input.MouseY());
            Math::Vec2 toTarget = target - gunPos;
            Math::Vec2 direction = toTarget.LengthSquared() > 0.0001f ? toTarget.Normalized() : Math::Vec2(0.0f, -1.0f);

            k2d::GameObject* bullet = bulletPrefab.Instantiate(scene, nullptr, &assets);
            if (bullet)
            {
                bullet->setPosition(gunPos);
                BulletScript* script = bullet->addComponent<BulletScript>();
                script->Fire(direction * kBulletSpeed, kBulletLifetime);
                ++shotsFired;
            }
        }

        scene.update(dt);

        glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetOrtho((float)device.Width(), (float)device.Height());
        scene.render(canvas);

        device.BeginUI();
        ImGui::Begin("Prefab Bullet Demo");
        ImGui::Text("Left-click anywhere to fire a bullet toward the cursor.");
        ImGui::Text("Bullets are k2d::Prefab::Instantiate() calls (loaded once, spawned many times).");
        ImGui::Text("Shots fired: %d", shotsFired);
        ImGui::Text("Active bullets: %zu", scene.objectCount());
        ImGui::End();
        device.EndUI();
        device.Swap();
    }

    assets.Clear();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}
