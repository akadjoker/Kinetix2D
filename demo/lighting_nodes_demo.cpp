// Demo mirroring Godot's "complete 2D lighting setup" node list, to see the
// K2D equivalents working together in one scene:
//   CanvasModulate     -> dark background rect (no dedicated node in K2D yet)
//   PointLight2D       -> k2d::Light2D              (4 colored lights)
//   DirectionalLight2D -> k2d::DirectionalLight2D    (one faint moonlight)
//   LightOccluder2D    -> k2d::LightOccluder2D       (small squares, one per marker)
//   Sprite2D           -> k2d::SpriteComponent       (wabbit sprites receiving light)
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

    // Godot's default Light2D texture (a soft radial-gradient cookie), used
    // here instead of a procedural falloff -- see CanvasRenderer::
    // SetDefaultLightTexture. Falls back to the procedural formula if missing.
    k2d::Texture *lightTexture = assets.LoadTexture(
        "light_cookie", "assets/2d_lights_and_shadows_neutral_point_light.png");
    if (!lightTexture)
        lightTexture = assets.LoadTexture(
            "light_cookie", "../../assets/2d_lights_and_shadows_neutral_point_light.png");
    if (lightTexture)
        canvas.SetDefaultLightTexture(lightTexture->Id());

    // CanvasModulate: darken the whole unlit scene so lights carry the image
    // (Godot's own 2D lights tutorial does the same with a CanvasModulate
    // node). Lights below reveal the true surface colors inside their radius.
    canvas.SetCanvasModulate(0.18f, 0.18f, 0.22f);

    k2d::Pixmap markerPixmap(1, 1);
    markerPixmap.Clear(255, 255, 255, 255);
    k2d::Texture *markerTex = markerPixmap.CreateTexture(assets, "occluder_marker");

    k2d::Scene scene;

    // A lit backdrop with actual surface detail -- a flat color, once lit,
    // can only ever read as "a colored circle" (nothing on it for the light
    // to reveal). A tiled floor (same procedural atlas as render_demo.cpp)
    // makes the falloff and the occluder shadows visibly land on something,
    // like the checkered floor in the reference image. Created first so it
    // draws first (underneath everything else).
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
            // Full-brightness tiles: the darkening is CanvasModulate's job
            // (set below), exactly as in Godot -- lights then reveal these
            // true colors inside their radius.
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
    floor->setMapSize(40, 25); // 40*32=1280, 25*32=800 -- exactly the canvas
    floor->setAtlasTilesX(kAtlas);
    for (int y = 0; y < 25; ++y)
        for (int x = 0; x < 40; ++x)
            floor->setTile(x, y, 1 + ((x / 2 + y / 2) % (kAtlas * kAtlas)));

    // "monster head" sprites receiving light -- roughly the reference image's
    // 2x2 grid layout.
    const glm::vec2 creaturePositions[4] = {
        glm::vec2(480.0f, 320.0f), glm::vec2(800.0f, 300.0f),
        glm::vec2(500.0f, 500.0f), glm::vec2(780.0f, 520.0f)};
    for (int i = 0; i < 4; ++i)
    {
        k2d::GameObject *obj = scene.createObject("creature");
        obj->setPosition(creaturePositions[i]);
        k2d::SpriteComponent *sprite = obj->addComponent<k2d::SpriteComponent>(wabbit);
        sprite->setSize(glm::vec2(96.0f, 96.0f));
        sprite->setPivot(glm::vec2(0.5f, 0.5f));
    }

    // Small square shadow casters scattered around, like the black squares in
    // the reference image. Each gets a LightOccluder2D (the actual shadow
    // caster, invisible on its own) plus a small dark sprite so you can see
    // where it is.
    const glm::vec2 markerPositions[14] = {
        glm::vec2(120.0f, 90.0f), glm::vec2(260.0f, 60.0f), glm::vec2(430.0f, 130.0f),
        glm::vec2(620.0f, 70.0f), glm::vec2(920.0f, 90.0f), glm::vec2(1120.0f, 60.0f),
        glm::vec2(160.0f, 300.0f), glm::vec2(300.0f, 440.0f), glm::vec2(620.0f, 250.0f),
        glm::vec2(690.0f, 400.0f), glm::vec2(950.0f, 260.0f), glm::vec2(1090.0f, 420.0f),
        glm::vec2(430.0f, 620.0f), glm::vec2(880.0f, 660.0f)};
    for (int i = 0; i < 14; ++i)
    {
        k2d::GameObject *obj = scene.createObject("occluder_marker");
        obj->setPosition(markerPositions[i]);
        k2d::SpriteComponent *sprite = obj->addComponent<k2d::SpriteComponent>(markerTex);
        sprite->setSize(glm::vec2(28.0f, 28.0f));
        sprite->setPivot(glm::vec2(0.5f, 0.5f));
        sprite->setColor(10, 10, 12);

        k2d::LightOccluder2D *occluder = obj->addComponent<k2d::LightOccluder2D>();
        glm::vec2 square[4] = {glm::vec2(-14.0f, -14.0f), glm::vec2(14.0f, -14.0f),
                                glm::vec2(14.0f, 14.0f), glm::vec2(-14.0f, 14.0f)};
        occluder->setPolygon(square, 4);
    }

    // Four PointLight2D-equivalents, one per corner, each a different color,
    // all shadow-casting -- like the orange/blue/green/white lights in the
    // reference image.
    struct LightSetup
    {
        glm::vec2 position;
        glm::vec3 color;
        float radius;
    };
    const LightSetup lightSetups[4] = {
        {glm::vec2(280.0f, 620.0f), glm::vec3(1.0f, 0.55f, 0.15f), 620.0f},  // orange, bottom-left
        {glm::vec2(1020.0f, 200.0f), glm::vec3(0.55f, 0.65f, 1.0f), 650.0f}, // blue, top-right
        {glm::vec2(950.0f, 600.0f), glm::vec3(0.35f, 1.0f, 0.45f), 600.0f},  // green, bottom-right
        {glm::vec2(300.0f, 180.0f), glm::vec3(1.0f, 1.0f, 0.95f), 550.0f}};  // white, top-left
    k2d::Light2D *lights[4];
    for (int i = 0; i < 4; ++i)
    {
        k2d::GameObject *obj = scene.createObject("point_light");
        obj->setPosition(lightSetups[i].position);
        k2d::Light2D *light = obj->addComponent<k2d::Light2D>();
        light->setColor(lightSetups[i].color.r, lightSetups[i].color.g, lightSetups[i].color.b);
        light->setEnergy(1.0f); // Godot's own PointLight2D default energy
        light->setRadius(lightSetups[i].radius);
        light->setCastShadow(true);
        light->setShadowColor(0.0f, 0.0f, 0.0f, 0.9f);
        light->setShadowFilter(k2d::SHADOW_FILTER_PCF5);
        lights[i] = light;
    }

    // One DirectionalLight2D -- faint moonlight tint, also shadow-casting.
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
        // Slowly orbit the lights so the shadows visibly sweep -- makes any
        // seam/artifact at a given angle much easier to spot than a static shot.
        for (int i = 0; i < 4; ++i)
        {
            glm::vec2 base = lightSetups[i].position;
            float wobble = std::sin(time * 0.35f + (float)i * 1.7f) * 25.0f;
            lights[i]->owner()->setPosition(base + glm::vec2(wobble, wobble * 0.5f));
        }

        scene.update(dt);

        float width = (float)device.Width();
        float height = (float)device.Height();

        // CanvasModulate stand-in: dark backdrop everything else is composited
        // over, so the additive lights read clearly against black.
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
