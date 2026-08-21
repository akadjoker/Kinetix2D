#include <k2d/k2d.h>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

static const int SCANCODE_ESCAPE = 41;
static const int SCANCODE_A = 4;
static const int SCANCODE_D = 7;
static const int SCANCODE_1 = 30;
static const int SCANCODE_2 = 31;
static const int SCANCODE_3 = 32;
static const int SCANCODE_4 = 33;
static const int SCANCODE_5 = 34;

static k2d::Texture *LoadAsset(k2d::Assets &assets, const char *name, const char *file)
{
    k2d::Texture *texture = assets.LoadTexture(name, file);
    if (!texture)
    {
        ct::String alternate("../../");
        alternate += file;
        texture = assets.LoadTexture(name, alternate.c_str());
    }
    return texture;
}

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Assets Demo", 1280, 720, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;
    k2d::Texture *idle = LoadAsset(assets, "player_idle", "assets/Gunner_Red_Idle.png");
    k2d::Texture *run = LoadAsset(assets, "player_run", "assets/Gunner_Red_Run.png");
    k2d::Texture *jump = LoadAsset(assets, "player_jump", "assets/Gunner_Red_Jump.png");
    k2d::Texture *crouch = LoadAsset(assets, "player_crouch", "assets/Gunner_Red_Crouch.png");
    k2d::Texture *death = LoadAsset(assets, "player_death", "assets/Gunner_Red_Death.png");
    k2d::Texture *fire = LoadAsset(assets, "fire", "assets/fire.png");
    k2d::Texture *props = LoadAsset(assets, "props", "assets/props_16x16.png");
    if (!idle || !run || !jump || !crouch || !death || !fire || !props)
        return 1;

    k2d::Scene scene;

    k2d::GameObject *mapObject = scene.createObject("tilemap");
    k2d::TileMapComponent *map = mapObject->addComponent<k2d::TileMapComponent>();
    if (!map->loadTMX(assets, "assets/map.tmx", "tmx_tiles"))
    {
        if (!map->loadTMX(assets, "../../assets/map.tmx", "tmx_tiles"))
            return 1;
    }

    k2d::GameObject *playerObject = scene.createObject("player");
    playerObject->setPosition(glm::vec2(600.0f, 560.0f));
    k2d::SpriteComponent *playerSprite = playerObject->addComponent<k2d::SpriteComponent>();
    playerSprite->setPivot(glm::vec2(0.5f, 1.0f));
    k2d::Animation2D *playerAnimation = playerObject->addComponent<k2d::Animation2D>();
    playerAnimation->addClip("idle", idle, 48, 48, 5, 7.0f, k2d::AnimationMode::Loop);
    playerAnimation->addClip("run", run, 48, 48, 6, 12.0f, k2d::AnimationMode::Loop);
    playerAnimation->addClip("jump", jump, 48, 48, 2, 5.0f, k2d::AnimationMode::OneShot);
    playerAnimation->addClip("crouch", crouch, 48, 48, 3, 8.0f, k2d::AnimationMode::PingPong);
    playerAnimation->addClip("death", death, 48, 48, 8, 8.0f, k2d::AnimationMode::OneShot);
    playerAnimation->play("idle");

    for (int i = 0; i < 12; ++i)
    {
        k2d::GameObject *propObject = scene.createObject("prop");
        propObject->setPosition(glm::vec2(80.0f + i * 96.0f, 608.0f));
        k2d::SpriteComponent *prop = propObject->addComponent<k2d::SpriteComponent>(props);
        prop->setSize(glm::vec2(48.0f, 48.0f));
        prop->setPivot(glm::vec2(0.5f, 1.0f));
        prop->setSourceRect((float)((i % 4) * 16), 0.0f, 16.0f, 16.0f);
    }

    k2d::GameObject *fireObject = scene.createObject("fire");
    fireObject->setPosition(glm::vec2(820.0f, 608.0f));
    k2d::SpriteComponent *fireSprite = fireObject->addComponent<k2d::SpriteComponent>(fire);
    fireSprite->setSize(glm::vec2(48.0f, 48.0f));
    fireSprite->setPivot(glm::vec2(0.5f, 1.0f));
    fireSprite->setColor(255, 130, 40);

    k2d::GameObject *lightObject = scene.createObject("player_light");
    k2d::Light2D *light = lightObject->addComponent<k2d::Light2D>();
    light->setColor(1.0f, 0.55f, 0.2f);
    light->setEnergy(1.5f);
    light->setRadius(260.0f);
    light->setCastShadow(false);

    k2d::Camera2D camera;
    bool running = true;
    while (running)
    {
        k2d::Profiler::Get().beginFrame();
        running = device.PollEvents();
        k2d::Input &input = device.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;

        if (input.KeyDown(SCANCODE_A))
            playerObject->translate(glm::vec2(-180.0f * device.DeltaTime(), 0.0f));
        if (input.KeyDown(SCANCODE_D))
            playerObject->translate(glm::vec2(180.0f * device.DeltaTime(), 0.0f));
        if (input.KeyPressed(SCANCODE_1)) playerAnimation->play("idle");
        if (input.KeyPressed(SCANCODE_2)) playerAnimation->play("run");
        if (input.KeyPressed(SCANCODE_3)) playerAnimation->play("jump");
        if (input.KeyPressed(SCANCODE_4)) playerAnimation->play("crouch");
        if (input.KeyPressed(SCANCODE_5)) playerAnimation->play("death");

        float width = (float)device.Width();
        float height = (float)device.Height();

        scene.update(device.DeltaTime());

        camera.position = playerObject->position();
        float minX = 0.0f;
        float minY = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        camera.VisibleRect(minX, minY, maxX, maxY, width, height);
        map->setCullRect(minX, minY, maxX - minX, maxY - minY);
        lightObject->setPosition(playerObject->position());

        glClearColor(0.035f, 0.045f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetProjection(camera.Projection(width, height));
        scene.render(canvas);

        device.BeginUI();
        ImGui::Begin("Assets demo");
        ImGui::Text("Clip: %s", playerAnimation->currentClip());
        ImGui::Text("Frame: %d / %d", playerAnimation->frame(), playerAnimation->frameCount());
        ImGui::Text("1 Idle  2 Run  3 Jump  4 Crouch  5 Death");
        ImGui::Text("A/D move player  ESC quit");
        ImGui::End();
        device.EndUI();
        device.Swap();
        k2d::Profiler::Get().endFrame();
    }

    assets.Clear();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}
