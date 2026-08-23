
#include <k2d/k2d.h>

#include <glad/glad.h>
#include <cstdio>

int main()
{
    k2d::Device device;
    if (!device.Init("K2D Light Falloff Diag", 1280, 800, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;

    k2d::Texture *lightTexture = assets.LoadTexture("light_cookie", "fire.png");
    std::printf("lightTexture=%p\n", (void *)lightTexture);
    std::fflush(stdout);
    if (lightTexture)
        canvas.SetDefaultLightTexture(lightTexture->Id());

    canvas.SetCanvasModulate(0.2f, 0.2f, 0.2f);

    k2d::Pixmap whitePixmap(1, 1);
    whitePixmap.Clear(255, 255, 255, 255);
    k2d::Texture *white = whitePixmap.CreateTexture(assets, "diag_white");

    k2d::Scene scene;
    k2d::GameObject *floorObject = scene.createObject("floor");
    floorObject->setPosition(glm::vec2(640.0f, 400.0f));
    k2d::SpriteComponent *floor = floorObject->addComponent<k2d::SpriteComponent>(white);
    floor->setSize(glm::vec2(1280.0f, 800.0f));
    floor->setPivot(glm::vec2(0.5f, 0.5f));
    floor->setColor(255, 255, 255);

    k2d::GameObject *lightObject = scene.createObject("light");
    lightObject->setPosition(glm::vec2(640.0f, 400.0f));
    k2d::Light2D *light = lightObject->addComponent<k2d::Light2D>();
    light->setColor(1.0f, 1.0f, 1.0f);
    light->setEnergy(1.0f);
    light->setRadius(360.0f);
    light->setCastShadow(false);

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
        if (frame == 30)
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