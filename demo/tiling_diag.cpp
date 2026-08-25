
#include <k2d/k2d.h>

int main()
{
    k2d::Device device;
    if (!device.Init("K2D Tiling Diag", 1280, 640, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;

    unsigned char checker[16 * 16 * 4];
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
        {
            int i = (y * 16 + x) * 4;
            bool on = ((x / 4) + (y / 4)) % 2 == 0;
            unsigned char v = on ? 235 : 60;
            checker[i + 0] = v;
            checker[i + 1] = on ? 200 : 40;
            checker[i + 2] = on ? 120 : 90;
            checker[i + 3] = 255;
        }
    k2d::Texture* tex = assets.CreateTexture("checker", 16, 16, checker, true, true);

    k2d::Scene scene;

    k2d::GameObject* tiledObject = scene.createObject("tiled");
    tiledObject->setPosition(Math::Vec2(320.0f, 320.0f));
    k2d::SpriteComponent* tiled = tiledObject->addComponent<k2d::SpriteComponent>(tex);
    tiled->setSize(Math::Vec2(600.0f, 560.0f));
    tiled->setPivot(Math::Vec2(0.5f, 0.5f));
    tiled->setTiling(8.0f, 7.0f);

    k2d::GameObject* stretchedObject = scene.createObject("stretched");
    stretchedObject->setPosition(Math::Vec2(960.0f, 320.0f));
    k2d::SpriteComponent* stretched = stretchedObject->addComponent<k2d::SpriteComponent>(tex);
    stretched->setSize(Math::Vec2(600.0f, 560.0f));
    stretched->setPivot(Math::Vec2(0.5f, 0.5f));

    bool running = true;
    int frame = 0;
    while (running)
    {
        running = device.PollEvents();
        if (device.GetInput().KeyDown(41))
            running = false;

        scene.update(device.DeltaTime());

        glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetOrtho((float)device.Width(), (float)device.Height());
        scene.render(canvas);

        ++frame;
        if (frame == 15)
        {
            device.CaptureScreenshot();
            running = false;
        }
        device.Swap();
    }

    assets.Clear();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}
