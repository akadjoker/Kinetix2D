
#include <k2d/k2d.h>

#include <glad/glad.h>

int main()
{
    k2d::Device device;
    if (!device.Init("K2D CullMask Diag", 900, 500, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;
    unsigned char whitePx[4] = {255, 255, 255, 255};
    k2d::Texture *white = assets.CreateTexture("white", 1, 1, whitePx);

    k2d::Scene scene;

    k2d::GameObject *backdropObject = scene.createObject("backdrop");
    backdropObject->setPosition(Math::Vec2(450.0f, 250.0f));
    k2d::SpriteComponent *backdrop = backdropObject->addComponent<k2d::SpriteComponent>(white);
    backdrop->setSize(Math::Vec2(900.0f, 500.0f));
    backdrop->setColor(30, 28, 34);
    backdrop->setLightMask(3u); 

    k2d::GameObject *aObj = scene.createObject("spriteA");
    aObj->setPosition(Math::Vec2(250.0f, 250.0f));
    k2d::SpriteComponent *spriteA = aObj->addComponent<k2d::SpriteComponent>(white);
    spriteA->setSize(Math::Vec2(80.0f, 80.0f));
    spriteA->setColor(50, 45, 45);
    spriteA->setLightMask(1u);

    k2d::GameObject *bObj = scene.createObject("spriteB");
    bObj->setPosition(Math::Vec2(450.0f, 250.0f));
    k2d::SpriteComponent *spriteB = bObj->addComponent<k2d::SpriteComponent>(white);
    spriteB->setSize(Math::Vec2(80.0f, 80.0f));
    spriteB->setColor(45, 45, 50);
    spriteB->setLightMask(2u);

    k2d::GameObject *cObj = scene.createObject("spriteC");
    cObj->setPosition(Math::Vec2(650.0f, 250.0f));
    k2d::SpriteComponent *spriteC = cObj->addComponent<k2d::SpriteComponent>(white);
    spriteC->setSize(Math::Vec2(80.0f, 80.0f));
    spriteC->setColor(45, 45, 45);
    spriteC->setLightMask(3u);

    k2d::GameObject *lightAObj = scene.createObject("lightA");
    lightAObj->setPosition(Math::Vec2(250.0f, 250.0f));
    k2d::Light2D *lightA = lightAObj->addComponent<k2d::Light2D>();
    lightA->setColor(1.0f, 0.2f, 0.2f);
    lightA->setEnergy(1.2f);
    lightA->setRadius(500.0f); 
    lightA->setCastShadow(false);
    lightA->setCullMask(1u);

    k2d::GameObject *lightBObj = scene.createObject("lightB");
    lightBObj->setPosition(Math::Vec2(450.0f, 250.0f));
    k2d::Light2D *lightB = lightBObj->addComponent<k2d::Light2D>();
    lightB->setColor(0.2f, 0.6f, 1.0f);
    lightB->setEnergy(1.2f);
    lightB->setRadius(500.0f);
    lightB->setCastShadow(false);
    lightB->setCullMask(2u);

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