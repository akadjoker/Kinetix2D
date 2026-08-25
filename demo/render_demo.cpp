#include <k2d/k2d.h>

#include <ct/string.hpp>

static const int SCANCODE_ESCAPE = 41;
static const int SCANCODE_W = 26;
static const int SCANCODE_A = 4;
static const int SCANCODE_S = 22;
static const int SCANCODE_D = 7;
static const int SCANCODE_Q = 20;
static const int SCANCODE_E = 8;

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Render Queue", 1280, 720, true))
        return 1;
    device.Focus();

    k2d::BatchRenderer batch;
    k2d::BatchRenderer::Config config;
    config.maxVertices = 65532;
    config.maxDrawCalls = 8192;
    if (!batch.Init(config))
        return 1;

    batch.Resize(device.Width(), device.Height());

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;

    unsigned char white[4] = {255, 255, 255, 255};
    k2d::Texture* whiteTex = assets.CreateTexture("white", 1, 1, white);

    unsigned char checker[8 * 8 * 4];
    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            bool light = ((x + y) & 1) == 0;
            unsigned char v = (unsigned char)((light ? 220 : 80) * 0.45f);
            int i = (y * 8 + x) * 4;
            checker[i + 0] = v;
            checker[i + 1] = v;
            checker[i + 2] = v;
            checker[i + 3] = 255;
        }
    }
    k2d::Texture* checkerTex = assets.CreateTexture("checker", 8, 8, checker);

    unsigned char stripes[4 * 16 * 4];
    for (int y = 0; y < 16; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            int i = (y * 4 + x) * 4;
            if ((y / 4) & 1)
            {
                stripes[i + 0] = (unsigned char)(60 * 0.45f);
                stripes[i + 1] = (unsigned char)(60 * 0.45f);
                stripes[i + 2] = (unsigned char)(95 * 0.45f);
            }
            else
            {
                stripes[i + 0] = (unsigned char)(130 * 0.45f);
                stripes[i + 1] = (unsigned char)(205 * 0.45f);
                stripes[i + 2] = (unsigned char)(255 * 0.45f);
            }
            stripes[i + 3] = 255;
        }
    }
    k2d::Texture* stripesTex = assets.CreateTexture("stripes", 4, 16, stripes);

    k2d::Scene scene;

    k2d::GameObject* bg = scene.createObject("bg");
    k2d::SpriteComponent* bgSprite = bg->addComponent<k2d::SpriteComponent>(whiteTex);
    bgSprite->setSize(Math::Vec2(4000.0f, 4000.0f));
    bgSprite->setColor(10, 12, 16);
    bg->setPosition(Math::Vec2(640.0f, 360.0f));
    bg->setZIndex(-10);

    k2d::GameObject* a = scene.createObject("a");
    k2d::SpriteComponent* sa = a->addComponent<k2d::SpriteComponent>(checkerTex);
    sa->setSize(Math::Vec2(220.0f, 220.0f));
    a->setPosition(Math::Vec2(300.0f, 220.0f));
    a->setZIndex(0);

    k2d::GameObject* b = scene.createObject("b");
    k2d::SpriteComponent* sb = b->addComponent<k2d::SpriteComponent>(stripesTex);
    sb->setSize(Math::Vec2(220.0f, 220.0f));
    b->setPosition(Math::Vec2(360.0f, 260.0f));
    b->setZIndex(1);

    k2d::GameObject* c = scene.createObject("c");
    k2d::SpriteComponent* sc = c->addComponent<k2d::SpriteComponent>(whiteTex);
    sc->setSize(Math::Vec2(220.0f, 220.0f));
    sc->setColor(130, 45, 45);
    c->setPosition(Math::Vec2(420.0f, 300.0f));
    c->setZIndex(2);

    k2d::GameObject* y1 = scene.createObject("y1");
    k2d::SpriteComponent* sy1 = y1->addComponent<k2d::SpriteComponent>(checkerTex);
    sy1->setSize(Math::Vec2(160.0f, 160.0f));
    sy1->setYSort(true);
    y1->setPosition(Math::Vec2(700.0f, 220.0f));

    k2d::GameObject* y2 = scene.createObject("y2");
    k2d::SpriteComponent* sy2 = y2->addComponent<k2d::SpriteComponent>(stripesTex);
    sy2->setSize(Math::Vec2(160.0f, 160.0f));
    sy2->setYSort(true);
    y2->setPosition(Math::Vec2(700.0f, 300.0f));

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
            atlas[i + 0] = (unsigned char)((60 + ((tx + ty * kAtlas) * 37) % 160) * 0.45f);
            atlas[i + 1] = (unsigned char)((50 + ((tx * 3 + ty * 5) * 23) % 140) * 0.45f);
            atlas[i + 2] = (unsigned char)((120 + ((tx * 7 + ty * 11) * 17) % 120) * 0.45f);
            atlas[i + 3] = 255;
        }
    }
    k2d::Texture* atlasTex = assets.CreateTexture("atlas", 128, 128, atlas);

    k2d::GameObject* tileMapObj = scene.createObject("tilemap");
    k2d::TileMapComponent* tileMap = tileMapObj->addComponent<k2d::TileMapComponent>();
    tileMap->setTexture(atlasTex);
    tileMap->setCellSize(32.0f, 32.0f);
    tileMap->setMapSize(50, 38);
    tileMap->setAtlasTilesX(kAtlas);
    for (int y = 0; y < 38; ++y)
    {
        for (int x = 0; x < 50; ++x)
        {
            int id;
            if (x == 0 || y == 0 || x == 49 || y == 37)
                id = 2;
            else if ((x + y) % 5 == 0)
                id = 4;
            else
                id = 1 + ((x / 3 + y / 3) % 14);
            tileMap->setTile(x, y, id);
        }
    }

    k2d::GameObject* lightObj = scene.createObject("light");
    k2d::Light2D* light = lightObj->addComponent<k2d::Light2D>();
    light->setColor(1.0f, 0.85f, 0.55f);
    light->setEnergy(2.2f);
    light->setRadius(340.0f);
    light->setCastShadow(true);
    light->setShadowColor(0.0f, 0.0f, 0.0f, 1.0f);
    light->setShadowFilter(k2d::SHADOW_FILTER_PCF5);
    lightObj->setPosition(Math::Vec2(640.0f, 360.0f));

    k2d::GameObject* directionalObj = scene.createObject("directional_light");
    k2d::DirectionalLight2D* directional = directionalObj->addComponent<k2d::DirectionalLight2D>();
    directional->setColor(0.35f, 0.4f, 0.5f);
    directional->setEnergy(0.2f);
    directional->setCastShadow(true);
    directional->setShadowColor(0.0f, 0.0f, 0.0f, 1.0f);
    directional->setShadowFilter(k2d::SHADOW_FILTER_PCF13);
    directionalObj->setRotationDegrees(-35.0f);

    const Math::Vec2 columnA[4] = {Math::Vec2(-22.0f, -130.0f), Math::Vec2(22.0f, -130.0f), Math::Vec2(22.0f, 130.0f),
                                   Math::Vec2(-22.0f, 130.0f)};
    k2d::GameObject* col1 = scene.createObject("col1");
    k2d::LightOccluder2D* occl1 = col1->addComponent<k2d::LightOccluder2D>();
    occl1->setPolygon(columnA, 4);
    col1->setPosition(Math::Vec2(500.0f, 270.0f));
    k2d::SpriteComponent* col1Sprite = col1->addComponent<k2d::SpriteComponent>(whiteTex);
    col1Sprite->setSize(Math::Vec2(44.0f, 260.0f));
    col1Sprite->setColor(150, 60, 60);

    const Math::Vec2 columnB[4] = {Math::Vec2(-30.0f, -100.0f), Math::Vec2(30.0f, -100.0f), Math::Vec2(30.0f, 100.0f),
                                   Math::Vec2(-30.0f, 100.0f)};
    k2d::GameObject* col2 = scene.createObject("col2");
    k2d::LightOccluder2D* occl2 = col2->addComponent<k2d::LightOccluder2D>();
    occl2->setPolygon(columnB, 4);
    col2->setPosition(Math::Vec2(830.0f, 420.0f));
    k2d::SpriteComponent* col2Sprite = col2->addComponent<k2d::SpriteComponent>(whiteTex);
    col2Sprite->setSize(Math::Vec2(60.0f, 200.0f));
    col2Sprite->setColor(150, 60, 60);

    const Math::Vec2 barC[4] = {Math::Vec2(-150.0f, -18.0f), Math::Vec2(150.0f, -18.0f), Math::Vec2(150.0f, 18.0f),
                                Math::Vec2(-150.0f, 18.0f)};
    k2d::GameObject* col3 = scene.createObject("col3");
    k2d::LightOccluder2D* occl3 = col3->addComponent<k2d::LightOccluder2D>();
    occl3->setPolygon(barC, 4);
    col3->setPosition(Math::Vec2(670.0f, 160.0f));
    k2d::SpriteComponent* col3Sprite = col3->addComponent<k2d::SpriteComponent>(whiteTex);
    col3Sprite->setSize(Math::Vec2(300.0f, 36.0f));
    col3Sprite->setColor(150, 60, 60);

    k2d::Camera2D camera;
    camera.position = Math::Vec2(640.0f, 360.0f);
    camera.zoom = Math::Vec2(1.0f, 1.0f);

    int lastDrawCalls = 0;
    bool running = true;
    while (running)
    {
        running = device.PollEvents();

        k2d::Input& input = device.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;

        float dt = device.DeltaTime();
        float panSpeed = 500.0f / camera.zoom.x * dt;
        if (input.KeyDown(SCANCODE_W))
            camera.position.y -= panSpeed;
        if (input.KeyDown(SCANCODE_S))
            camera.position.y += panSpeed;
        if (input.KeyDown(SCANCODE_A))
            camera.position.x -= panSpeed;
        if (input.KeyDown(SCANCODE_D))
            camera.position.x += panSpeed;
        if (input.KeyDown(SCANCODE_Q))
            camera.rotationDegrees -= 60.0f * dt;
        if (input.KeyDown(SCANCODE_E))
            camera.rotationDegrees += 60.0f * dt;
        if (input.WheelY() != 0.0f)
        {
            float z = camera.zoom.x * (input.WheelY() > 0.0f ? 1.1f : 0.9f);
            if (z < 0.1f)
                z = 0.1f;
            if (z > 8.0f)
                z = 8.0f;
            camera.zoom = Math::Vec2(z, z);
        }

        float cullX, cullY, cullX2, cullY2;
        camera.VisibleRect(cullX, cullY, cullX2, cullY2, (float)device.Width(), (float)device.Height());
        tileMap->setCullRect(cullX, cullY, cullX2 - cullX, cullY2 - cullY);

        Math::Vec2 mouseWorld =
            camera.ScreenToWorld(input.MouseX(), input.MouseY(), (float)device.Width(), (float)device.Height());
        lightObj->setPosition(mouseWorld);

        if (device.WasResized())
        {
            batch.Resize(device.Width(), device.Height());
            glViewport(0, 0, device.Width(), device.Height());
        }

        scene.update(dt);

        glClearColor(0.04f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        batch.ResetStats();
        canvas.ResetStats();
        batch.BeginFrame();

        canvas.SetProjection(camera.Projection((float)device.Width(), (float)device.Height()));
        scene.render(canvas);

        batch.SetProjection(Math::Mat4::Ortho(0.0f, (float)device.Width(), (float)device.Height(), 0.0f, -1.0f, 1.0f));

        ct::String hud;
        hud += "tiles ";
        hud.append_number((int)scene.renderCommandCount());
        hud += "   items ";
        hud.append_number((int)scene.renderItemCount());
        hud += "   draw calls ";
        hud.append_number(lastDrawCalls);
        hud += "   cam ";
        hud.append_number((int)camera.position.x);
        hud += ",";
        hud.append_number((int)camera.position.y);
        hud += " r";
        hud.append_number((int)camera.rotationDegrees);
        hud += "   fps ";
        hud.append_number((int)(dt > 0.0f ? 1.0f / dt : 0.0f));
        batch.SetColor((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255);
        batch.DrawText(16.0f, 16.0f, 18.0f, hud.c_str());

        ct::String hud2;
        hud2 += "WASD pan   Q/E rotate   wheel zoom   light follows mouse, casts shadows   lights ";
        hud2.append_number((int)scene.renderLightCount());
        batch.SetColor((unsigned char)200, (unsigned char)210, (unsigned char)220, (unsigned char)255);
        batch.DrawText(16.0f, 40.0f, 14.0f, hud2.c_str());

        batch.EndFrame();

        lastDrawCalls = (int)canvas.GetStats().drawCalls;

        device.Swap();
    }

    assets.Clear();
    canvas.Shutdown();
    batch.Shutdown();
    device.Shutdown();

    return 0;
}
