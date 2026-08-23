// Diagnostic: a flat gray sprite with a normal map whose left half points
// -X and right half points +X, lit by a light to its right. Physically the
// right half's normal aligns with the light direction (higher cNdotL) so it
// must render brighter than the left half. Self-captures then measures both
// halves via GetPixel-equivalent (screenshot, checked separately).
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

    // 2x1 normal map: left texel points -X (0,128,255), right texel +X
    // (255,128,255). Nearest filtering so the split stays a hard edge, easy
    // to sample two clean halves.
    unsigned char normalPx[2 * 4] = {
        0, 128, 255, 255,   // left: -X
        255, 128, 255, 255, // right: +X
    };
    k2d::Texture *normalMap = assets.CreateTexture("normal", 2, 1, normalPx, /*nearest=*/true);

    k2d::Scene scene;

    // Backdrop so the falloff itself is visible too (sanity check).
    k2d::GameObject *backdropObject = scene.createObject("backdrop");
    backdropObject->setPosition(glm::vec2(450.0f, 250.0f));
    k2d::SpriteComponent *backdrop = backdropObject->addComponent<k2d::SpriteComponent>(gray);
    backdrop->setSize(glm::vec2(900.0f, 500.0f));
    backdrop->setColor(20, 20, 22);

    // Normal-mapped sprite: left half normal -X, right half normal +X.
    k2d::GameObject *spriteObject = scene.createObject("panel");
    spriteObject->setPosition(glm::vec2(350.0f, 250.0f));
    k2d::SpriteComponent *sprite = spriteObject->addComponent<k2d::SpriteComponent>(gray);
    sprite->setSize(glm::vec2(240.0f, 160.0f));
    sprite->setColor(150, 150, 150);
    sprite->setNormalMap(normalMap);

    // Control sprite (same gray, NO normal map) so flat lighting is
    // unaffected -- confirms u_hasNormalMap correctly stays off elsewhere.
    k2d::GameObject *controlObject = scene.createObject("control");
    controlObject->setPosition(glm::vec2(700.0f, 250.0f));
    k2d::SpriteComponent *control = controlObject->addComponent<k2d::SpriteComponent>(gray);
    control->setSize(glm::vec2(120.0f, 160.0f));
    control->setColor(150, 150, 150);

    // Light to the right of the normal-mapped panel, with height so a
    // neutral (0,0,1) normal still picks up some light.
    k2d::GameObject *lightObject = scene.createObject("light");
    lightObject->setPosition(glm::vec2(650.0f, 250.0f));
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
