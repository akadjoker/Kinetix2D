
#include <k2d/k2d.h>

#include <glad/glad.h>

int main()
{
    k2d::Device device;
    if (!device.Init("K2D NormalMap Diag", 900, 500, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;
    unsigned char grayPx[4] = {150, 150, 150, 255};
    k2d::Texture *gray = assets.CreateTexture("gray", 1, 1, grayPx);

    unsigned char normalPx[2 * 4] = {
        0, 128, 255, 255,   
        255, 128, 255, 255, 
    };
    k2d::Texture *normalMap = assets.CreateTexture("normal", 2, 1, normalPx, true);

    k2d::Scene scene;

    k2d::GameObject *backdropObject = scene.createObject("backdrop");
    backdropObject->setPosition(Math::Vec2(450.0f, 250.0f));
    k2d::SpriteComponent *backdrop = backdropObject->addComponent<k2d::SpriteComponent>(gray);
    backdrop->setSize(Math::Vec2(900.0f, 500.0f));
    backdrop->setColor(20, 20, 22);

    k2d::GameObject *spriteObject = scene.createObject("panel");
    spriteObject->setPosition(Math::Vec2(350.0f, 250.0f));
    k2d::SpriteComponent *sprite = spriteObject->addComponent<k2d::SpriteComponent>(gray);
    sprite->setSize(Math::Vec2(240.0f, 160.0f));
    sprite->setColor(150, 150, 150);
    sprite->setNormalMap(normalMap);

    k2d::GameObject *controlObject = scene.createObject("control");
    controlObject->setPosition(Math::Vec2(700.0f, 250.0f));
    k2d::SpriteComponent *control = controlObject->addComponent<k2d::SpriteComponent>(gray);
    control->setSize(Math::Vec2(120.0f, 160.0f));
    control->setColor(150, 150, 150);

    k2d::GameObject *lightObject = scene.createObject("light");
    lightObject->setPosition(Math::Vec2(650.0f, 250.0f));
    k2d::Light2D *light = lightObject->addComponent<k2d::Light2D>();
    light->setColor(1.0f, 1.0f, 1.0f);
    light->setEnergy(1.0f);
    light->setRadius(700.0f);
    light->setCastShadow(false);
    light->setHeight(150.0f);

    bool running = true;
    int frame = 0;
    while (running)
    {
        running = device.PollEvents();
        if (device.GetInput().KeyDown(41))
            running = false;

        scene.update(device.DeltaTime());

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetOrtho((float)device.Width(), (float)device.Height());
        scene.render(canvas);

        ++frame;
        if (frame == 5)
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