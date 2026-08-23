
#include <k2d/k2d.h>

#include <glad/glad.h>

int main()
{
    k2d::Device device;
    if (!device.Init("K2D ScreenFade Diag", 800, 600, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    k2d::BatchRenderer batch;
    if (!canvas.Init() || !batch.Init())
        return 1;

    k2d::Assets assets;
    unsigned char px[4 * 4 * 4];
    for (int i = 0; i < 4 * 4; ++i)
    {
        px[i * 4 + 0] = 220; px[i * 4 + 1] = 160; px[i * 4 + 2] = 40; px[i * 4 + 3] = 255;
    }
    k2d::Texture *tex = assets.CreateTexture("sq", 4, 4, px);

    k2d::Scene scene;
    k2d::GameObject *obj = scene.createObject("square");
    obj->setPosition(glm::vec2(400.0f, 300.0f));
    k2d::SpriteComponent *sprite = obj->addComponent<k2d::SpriteComponent>(tex);
    sprite->setSize(glm::vec2(120.0f, 120.0f));

    k2d::Camera2D camera;
    camera.position = glm::vec2(400.0f, 300.0f);

    k2d::ScreenFade fade;
    fade.SetColor(0, 0, 0);
    fade.FadeOut(1.0f);

    bool running = true;
    int frame = 0;
    while (running)
    {
        running = device.PollEvents();
        if (device.GetInput().KeyDown(41))
            running = false;

        float dt = 1.0f / 30.0f; 
        scene.update(dt);
        fade.Update(dt);
        camera.position.x += 40.0f * dt; 

        float w = (float)device.Width();
        float h = (float)device.Height();

        glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetProjection(camera.Projection(w, h));
        scene.render(canvas);

        batch.Resize(device.Width(), device.Height());
        batch.SetProjection(glm::ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f));
        batch.BeginFrame();
        fade.Draw(batch, w, h);
        batch.EndFrame();

        ++frame;
        if (frame == 15)
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