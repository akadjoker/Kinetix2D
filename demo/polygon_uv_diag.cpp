
#include <k2d/k2d.h>

#include <cstdio>

int main()
{
    k2d::Device device;
    if (!device.Init("K2D Polygon UV Diag", 256, 256, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;

    unsigned char pixels[2 * 4] = {255, 0, 0, 255, 0, 0, 255, 255};
    k2d::Texture* texture = assets.CreateTexture("split", 2, 1, pixels, true);
    if (!texture)
        return 1;

    k2d::Scene scene;
    k2d::GameObject* obj = scene.createObject("quad");
    obj->setPosition(Math::Vec2(128.0f, 128.0f));
    k2d::Polygon2D* poly = obj->addComponent<k2d::Polygon2D>();
    poly->setTexture(texture);

    Math::Vec2 quad[4] = {{-100.0f, -50.0f}, {100.0f, -50.0f}, {100.0f, 50.0f}, {-100.0f, 50.0f}};
    poly->setPolygon(quad, 4);

    int frame = 0;
    bool running = true;
    while (running)
    {
        running = device.PollEvents();
        float width = (float)device.Width(), height = (float)device.Height();
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetOrtho(width, height);
        scene.render(canvas);
        device.Swap();

        if (frame == 2)
        {
            unsigned char left[4], right[4];

            glReadPixels(80, 128, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, left);
            glReadPixels(176, 128, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, right);
            bool leftIsRed = left[0] > 200 && left[2] < 60;
            bool rightIsBlue = right[2] > 200 && right[0] < 60;
            bool mapped = leftIsRed && rightIsBlue;
            std::printf("left=(%d,%d,%d) right=(%d,%d,%d) uv_mapped=%s\n", left[0], left[1], left[2], right[0],
                        right[1], right[2], mapped ? "PASS" : "FAIL");
            device.Shutdown();
            return mapped ? 0 : 1;
        }
        ++frame;
    }

    device.Shutdown();
    return 1;
}
