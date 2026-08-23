#include <k2d/k2d.h>

#include <glad/glad.h>
#include <imgui.h>

static unsigned int Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
{
    return (unsigned int)r | ((unsigned int)g << 8) |
           ((unsigned int)b << 16) | ((unsigned int)a << 24);
}

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D Parallax", 1280, 720, true))
        return 1;

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;
    k2d::Texture *tiles = assets.LoadTexture("parallax_tiles", "assets/tiles.png");
    if (!tiles)
        tiles = assets.LoadTexture("parallax_tiles", "../../assets/tiles.png");
    if (!tiles)
        return 1;

    k2d::ParallaxBackground background;
    k2d::ParallaxLayer2D *sky = background.createLayer(tiles, Math::Vec2(0.10f), -30);
    sky->setTileSize(Math::Vec2(192.0f, 240.0f));
    sky->setColor(Color(45, 55, 90, 255));

    k2d::ParallaxLayer2D *hills = background.createLayer(tiles, Math::Vec2(0.35f), -20);
    hills->setTileSize(Math::Vec2(144.0f, 180.0f));
    hills->setColor(Color(90, 75, 90, 220));

    k2d::ParallaxLayer2D *foreground = background.createLayer(tiles, Math::Vec2(0.70f), -10);
    foreground->setTileSize(Math::Vec2(96.0f, 120.0f));
    foreground->setColor(Color(150, 105, 90, 220));

    k2d::Camera2D camera;
    camera.position = Math::Vec2(640.0f, 360.0f);
    bool running = true;
    while (running)
    {
        running = device.PollEvents();
        k2d::Input &input = device.GetInput();
        if (input.KeyDown(41))
            running = false;
        float speed = 420.0f * device.DeltaTime();
        if (input.KeyDown(4)) camera.position.x -= speed;
        if (input.KeyDown(7)) camera.position.x += speed;

        float width = (float)device.Width();
        float height = (float)device.Height();
        glClearColor(0.025f, 0.03f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetProjection(camera.Projection(width, height));

        k2d::RenderQueue queue;
        background.submit(queue, camera, width, height);
        queue.Flush(canvas);

        device.BeginUI();
        ImGui::Begin("Parallax test");
        ImGui::Text("A/D move camera");
        ImGui::Text("Sky 0.10   Hills 0.35   Foreground 0.70");
        ImGui::Text("Camera X: %.1f", camera.position.x);
        ImGui::End();
        device.EndUI();
        device.Swap();
    }

    assets.Clear();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}