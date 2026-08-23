
#include <k2d/k2d.h>

#include <glad/glad.h>

int main()
{
    k2d::Device device;
    if (!device.Init("K2D Line/NinePatch Diag", 1000, 700, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    k2d::Assets assets;

    unsigned char panel[32 * 32 * 4];
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x)
        {
            int i = (y * 32 + x) * 4;
            bool cornerX = x < 8 || x >= 24;
            bool cornerY = y < 8 || y >= 24;
            unsigned char r, g, b;
            if (cornerX && cornerY) { r = 220; g = 60; b = 60; }   
            else if (cornerX || cornerY) { r = 60; g = 200; b = 90; } 
            else { r = 60; g = 110; b = 220; }                      
            panel[i + 0] = r; panel[i + 1] = g; panel[i + 2] = b; panel[i + 3] = 255;
        }
    k2d::Texture *panelTex = assets.CreateTexture("panel", 32, 32, panel, true, false);

    k2d::Scene scene;

    k2d::GameObject *patchObject = scene.createObject("patch");
    patchObject->setPosition(glm::vec2(60.0f, 60.0f));
    k2d::NinePatchComponent *patch = patchObject->addComponent<k2d::NinePatchComponent>();
    patch->setTexture(panelTex);
    patch->setSize(glm::vec2(400.0f, 260.0f));
    patch->setMargins(8.0f, 8.0f, 8.0f, 8.0f);

    k2d::GameObject *lineObject = scene.createObject("zigzag");
    lineObject->setPosition(glm::vec2(550.0f, 80.0f));
    k2d::Line2D *line = lineObject->addComponent<k2d::Line2D>();
    glm::vec2 zigzag[6] = {{0, 0}, {60, 120}, {120, 0}, {180, 120}, {240, 0}, {300, 120}};
    line->setPoints(zigzag, 6);
    line->setWidth(14.0f);
    line->setColor(255, 210, 60, 255);

    k2d::GameObject *pentObject = scene.createObject("pentagon");
    pentObject->setPosition(glm::vec2(720.0f, 420.0f));
    k2d::Line2D *pentagon = pentObject->addComponent<k2d::Line2D>();
    glm::vec2 penta[5];
    for (int i = 0; i < 5; ++i)
    {
        float a = (float)i / 5.0f * 6.2831853f - 1.5707963f;
        penta[i] = glm::vec2(cosf(a), sinf(a)) * 90.0f;
    }
    pentagon->setPoints(penta, 5);
    pentagon->setClosed(true);
    pentagon->setWidth(8.0f);
    pentagon->setColor(120, 220, 255, 255);

    bool running = true;
    int frame = 0;
    while (running)
    {
        running = device.PollEvents();
        if (device.GetInput().KeyDown(41))
            running = false;

        scene.update(device.DeltaTime());

        glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.SetOrtho((float)device.Width(), (float)device.Height());
        scene.render(canvas);

        ++frame;
        if (frame == 15)
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