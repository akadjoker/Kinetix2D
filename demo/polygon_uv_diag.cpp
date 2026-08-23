// Diagnostic: Polygon2D texture UV mapping. Before the fix, CanvasRenderer::
// EmitPolygon wrote a hardcoded UV(0,0) on every vertex, so any textured
// polygon rendered as a flat fill of the texture's (0,0) texel everywhere.
// Builds a 2x1 texture (left texel red, right texel blue), a polygon whose
// local-space width matches the texture width 1:1, and reads back pixels
// from the left and right halves of the rendered shape -- they must differ
// (real UV mapping) instead of both coming back the same color (the bug).
#include <k2d/k2d.h>

#include <glad/glad.h>

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
    // 2x1: texel 0 red, texel 1 blue.
    unsigned char pixels[2 * 4] = {255, 0, 0, 255, 0, 0, 255, 255};
    k2d::Texture *texture = assets.CreateTexture("split", 2, 1, pixels, /*nearest*/ true);
    if (!texture)
        return 1;

    k2d::Scene scene;
    k2d::GameObject *obj = scene.createObject("quad");
    obj->setPosition(glm::vec2(128.0f, 128.0f));
    k2d::Polygon2D *poly = obj->addComponent<k2d::Polygon2D>();
    poly->setTexture(texture);
    // Local-space rectangle exactly texture-sized (2x1) scaled up 100x so it
    // fills a good chunk of the 256x256 window: (0,0)-(200,100), centered
    // via the object's own position.
    glm::vec2 quad[4] = {{-100.0f, -50.0f}, {100.0f, -50.0f}, {100.0f, 50.0f}, {-100.0f, 50.0f}};
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
            // Window is 256 tall, GL reads bottom-up -- object center (128,128)
            // in top-down k2d coords is screen row 128 either way (symmetric).
            glReadPixels(80, 128, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, left);
            glReadPixels(176, 128, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, right);
            bool leftIsRed = left[0] > 200 && left[2] < 60;
            bool rightIsBlue = right[2] > 200 && right[0] < 60;
            bool mapped = leftIsRed && rightIsBlue;
            std::printf("left=(%d,%d,%d) right=(%d,%d,%d) uv_mapped=%s\n",
                        left[0], left[1], left[2], right[0], right[1], right[2],
                        mapped ? "PASS" : "FAIL");
            device.Shutdown();
            return mapped ? 0 : 1;
        }
        ++frame;
    }

    device.Shutdown();
    return 1;
}
