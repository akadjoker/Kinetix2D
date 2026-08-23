// Diagnostic: a world sprite under a panning Camera2D, plus a "HUD" sprite
// drawn via a SECOND Scene with a plain screen-space ortho projection.
// Confirms two CanvasRenderer.DrawItems passes with different projections
// compose cleanly (world sprite drifts with the pan, HUD sprite doesn't).
#include <k2d/k2d.h>

#include <glad/glad.h>

int main()
{
    k2d::Device device;
    if (!device.Init("K2D CanvasLayer Diag", 800, 600, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;
    unsigned char orangePx[4] = {230, 150, 40, 255};
    unsigned char cyanPx[4] = {60, 220, 230, 255};
    k2d::Texture *orange = assets.CreateTexture("orange", 1, 1, orangePx);
    k2d::Texture *cyan = assets.CreateTexture("cyan", 1, 1, cyanPx);

    // World scene: orange square at world (400,300), same as the camera
    // target initially -- panning the camera should visibly move it.
    k2d::Scene worldScene;
    k2d::GameObject *worldObj = worldScene.createObject("world_square");
    worldObj->setPosition(glm::vec2(400.0f, 300.0f));
    k2d::SpriteComponent *worldSprite = worldObj->addComponent<k2d::SpriteComponent>(orange);
    worldSprite->setSize(glm::vec2(80.0f, 80.0f));

    // HUD layer: k2d::CanvasLayer, the thin wrapper around exactly the
    // pattern proven above (own Scene, screen-space ortho on render()).
    k2d::CanvasLayer hud;
    k2d::GameObject *hudObj = hud.scene().createObject("hud_badge");
    hudObj->setPosition(glm::vec2(60.0f, 60.0f)); // top-left corner, screen pixels
    k2d::SpriteComponent *hudSprite = hudObj->addComponent<k2d::SpriteComponent>(cyan);
    hudSprite->setSize(glm::vec2(50.0f, 50.0f));

    k2d::Camera2D camera;
    camera.position = glm::vec2(400.0f, 300.0f);

    bool running = true;
    int frame = 0;
    while (running)
    {
        running = device.PollEvents();
        if (device.GetInput().KeyDown(41))
            running = false;

        float dt = 1.0f / 30.0f;
        worldScene.update(dt);
        hud.update(dt);
        camera.position.x += 200.0f * dt; // pan hard -- 10 frames = 66px

        float w = (float)device.Width();
        float h = (float)device.Height();

        glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.SetProjection(camera.Projection(w, h));
        worldScene.render(canvas);

        hud.render(canvas, w, h);

        ++frame;
        if (frame == 10)
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
