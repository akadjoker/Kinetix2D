
#include <k2d/k2d.h>

#include <glad/glad.h>
#include <cmath>

static const int SCANCODE_ESCAPE = 41;

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Lighting Nodes Demo", 1280, 800, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;
    k2d::Texture *wabbit = assets.LoadTexture("wabbit", "assets/wabbit_alpha.png");
    if (!wabbit)
        wabbit = assets.LoadTexture("wabbit", "../../assets/wabbit_alpha.png");
    if (!wabbit)
        return 1;

    k2d::Texture *lightTexture = assets.LoadTexture(
        "light_cookie", "assets/2d_lights_and_shadows_neutral_point_light.png");
    if (!lightTexture)
        lightTexture = assets.LoadTexture(
            "light_cookie", "../../assets/2d_lights_and_shadows_neutral_point_light.png");
    if (lightTexture)
        canvas.SetDefaultLightTexture(lightTexture->Id());

    canvas.SetCanvasModulate(0.18f, 0.18f, 0.22f);

    k2d::Pixmap markerPixmap(1, 1);
    markerPixmap.Clear(255, 255, 255, 255);
    k2d::Texture *markerTex = markerPixmap.CreateTexture(assets, "occluder_marker");

    k2d::Scene scene;

    const int kTile = 32;
    const int kAtlas = 4;
    unsigned char atlas[128 * 128 * 4];
    for (int y = 0; y < 128; ++y)
    {
        for (int x = 0; x < 128; ++x)
        {
            int tx = x / kTile;
            int ty = y / kTile;
            int i = (y * 128 + x) * 4;

            atlas[i + 0] = (unsigned char)(60 + ((tx + ty * kAtlas) * 37) % 160);
            atlas[i + 1] = (unsigned char)(50 + ((tx * 3 + ty * 5) * 23) % 140);
            atlas[i + 2] = (unsigned char)(120 + ((tx * 7 + ty * 11) * 17) % 120);
            atlas[i + 3] = 255;
        }
    }
    k2d::Texture *floorAtlas = assets.CreateTexture("floor_atlas", 128, 128, atlas);

    k2d::GameObject *floorObject = scene.createObject("floor");
    k2d::TileMapComponent *floor = floorObject->addComponent<k2d::TileMapComponent>();
    floor->setTexture(floorAtlas);
    floor->setCellSize(32.0f, 32.0f);
    floor->setMapSize(40, 25); 
    floor->setAtlasTilesX(kAtlas);
    for (int y = 0; y < 25; ++y)
        for (int x = 0; x < 40; ++x)
            floor->setTile(x, y, 1 + ((x / 2 + y / 2) % (kAtlas * kAtlas)));

    const Math::Vec2 creaturePositions[4] = {
        Math::Vec2(480.0f, 320.0f), Math::Vec2(800.0f, 300.0f),
        Math::Vec2(500.0f, 500.0f), Math::Vec2(780.0f, 520.0f)};
    for (int i = 0; i < 4; ++i)
    {
        k2d::GameObject *obj = scene.createObject("creature");
        obj->setPosition(creaturePositions[i]);
        k2d::SpriteComponent *sprite = obj->addComponent<k2d::SpriteComponent>(wabbit);
        sprite->setSize(Math::Vec2(96.0f, 96.0f));
        sprite->setPivot(Math::Vec2(0.5f, 0.5f));
    }

    const Math::Vec2 markerPositions[14] = {
        Math::Vec2(120.0f, 90.0f), Math::Vec2(260.0f, 60.0f), Math::Vec2(430.0f, 130.0f),
        Math::Vec2(620.0f, 70.0f), Math::Vec2(920.0f, 90.0f), Math::Vec2(1120.0f, 60.0f),
        Math::Vec2(160.0f, 300.0f), Math::Vec2(300.0f, 440.0f), Math::Vec2(620.0f, 250.0f),
        Math::Vec2(690.0f, 400.0f), Math::Vec2(950.0f, 260.0f), Math::Vec2(1090.0f, 420.0f),
        Math::Vec2(430.0f, 620.0f), Math::Vec2(880.0f, 660.0f)};
    for (int i = 0; i < 14; ++i)
    {
        k2d::GameObject *obj = scene.createObject("occluder_marker");
        obj->setPosition(markerPositions[i]);
        k2d::SpriteComponent *sprite = obj->addComponent<k2d::SpriteComponent>(markerTex);
        sprite->setSize(Math::Vec2(28.0f, 28.0f));
        sprite->setPivot(Math::Vec2(0.5f, 0.5f));
        sprite->setColor(10, 10, 12);

        k2d::LightOccluder2D *occluder = obj->addComponent<k2d::LightOccluder2D>();
        Math::Vec2 square[4] = {Math::Vec2(-14.0f, -14.0f), Math::Vec2(14.0f, -14.0f),
                                Math::Vec2(14.0f, 14.0f), Math::Vec2(-14.0f, 14.0f)};
        occluder->setPolygon(square, 4);
    }

    struct LightSetup
    {
        Math::Vec2 position;
        Math::Vec3 color;
        float radius;
    };
    const LightSetup lightSetups[4] = {
        {Math::Vec2(280.0f, 620.0f), Math::Vec3(1.0f, 0.55f, 0.15f), 620.0f},  
        {Math::Vec2(1020.0f, 200.0f), Math::Vec3(0.55f, 0.65f, 1.0f), 650.0f}, 
        {Math::Vec2(950.0f, 600.0f), Math::Vec3(0.35f, 1.0f, 0.45f), 600.0f},  
        {Math::Vec2(300.0f, 180.0f), Math::Vec3(1.0f, 1.0f, 0.95f), 550.0f}};  
    k2d::Light2D *lights[4];
    for (int i = 0; i < 4; ++i)
    {
        k2d::GameObject *obj = scene.createObject("point_light");
        obj->setPosition(lightSetups[i].position);
        k2d::Light2D *light = obj->addComponent<k2d::Light2D>();
        light->setColor(lightSetups[i].color.x, lightSetups[i].color.y, lightSetups[i].color.z);
        light->setEnergy(1.0f); 
        light->setRadius(lightSetups[i].radius);
        light->setCastShadow(true);
        light->setShadowColor(0.0f, 0.0f, 0.0f, 0.9f);
        light->setShadowFilter(k2d::SHADOW_FILTER_PCF5);
        lights[i] = light;
    }

    k2d::GameObject *moonObject = scene.createObject("moonlight");
    k2d::DirectionalLight2D *moon = moonObject->addComponent<k2d::DirectionalLight2D>();
    moon->setColor(0.4f, 0.45f, 0.6f);
    moon->setEnergy(0.35f);
    moon->setCastShadow(true);
    moon->setShadowColor(0.0f, 0.0f, 0.0f, 0.35f);
    moon->setShadowFilter(k2d::SHADOW_FILTER_PCF13);

    bool running = true;
    float time = 0.0f;
    while (running)
    {
        running = device.PollEvents();
        k2d::Input &input = device.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;

        float dt = device.DeltaTime();
        time += dt;

        for (int i = 0; i < 4; ++i)
        {
            Math::Vec2 base = lightSetups[i].position;
            float wobble = std::sin(time * 0.35f + (float)i * 1.7f) * 25.0f;
            lights[i]->owner()->setPosition(base + Math::Vec2(wobble, wobble * 0.5f));
        }

        scene.update(dt);

        float width = (float)device.Width();
        float height = (float)device.Height();

        glClearColor(0.03f, 0.03f, 0.045f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetOrtho(width, height);
        scene.render(canvas);

        device.Swap();
    }

    assets.Clear();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}