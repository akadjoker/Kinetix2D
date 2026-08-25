
#include <k2d/k2d.h>

int main()
{
    k2d::Device device;
    if (!device.Init("K2D SceneDebug Diag", 800, 600, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    k2d::BatchRenderer batch;
    if (!canvas.Init() || !batch.Init())
        return 1;

    k2d::Assets assets;
    unsigned char px[4] = {200, 90, 200, 200};
    k2d::Texture* tex = assets.CreateTexture("sq", 1, 1, px);

    k2d::Scene scene;
    k2d::GameObject* parent = scene.createObject("parent");
    parent->setPosition(Math::Vec2(300.0f, 300.0f));
    parent->setRotationDegrees(20.0f);
    k2d::SpriteComponent* parentSprite = parent->addComponent<k2d::SpriteComponent>(tex);
    parentSprite->setSize(Math::Vec2(100.0f, 60.0f));

    k2d::GameObject* child = scene.createObject("child", parent);
    child->setPosition(Math::Vec2(140.0f, 0.0f));
    k2d::SpriteComponent* childSprite = child->addComponent<k2d::SpriteComponent>(tex);
    childSprite->setSize(Math::Vec2(40.0f, 40.0f));

    k2d::GameObject* grandchild = scene.createObject("grandchild", child);
    grandchild->setPosition(Math::Vec2(0.0f, 70.0f));

    bool running = true;
    int frame = 0;
    while (running)
    {
        running = device.PollEvents();
        if (device.GetInput().KeyDown(41))
            running = false;

        scene.update(device.DeltaTime());

        float w = (float)device.Width();
        float h = (float)device.Height();

        glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetOrtho(w, h);
        scene.render(canvas);

        batch.Resize(device.Width(), device.Height());
        batch.SetProjection(Math::Mat4::Ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f));
        batch.BeginFrame();
        k2d::SceneDebugOptions opts;
        opts.showNames = true;
        k2d::DrawSceneDebug(batch, scene.root(), opts);
        batch.EndFrame();

        ++frame;
        if (frame == 5)
        {
            device.CaptureScreenshot();
            running = false;
        }
        device.Swap();
    }

    assets.Clear();
    batch.Shutdown();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}
