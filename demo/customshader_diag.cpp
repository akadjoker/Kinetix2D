
#include <k2d/k2d.h>

#include <cstdio>

static const char* kInvertFragmentShader = R"(#version 300 es
precision highp float;
in vec2 v_texcoord;
in vec4 v_color;
uniform sampler2D u_texture;
out vec4 FragColor;
void main()
{
    vec4 texColor = texture(u_texture, v_texcoord);
    FragColor = vec4(1.0 - texColor.rgb, texColor.a) * v_color;
}
)";

int main()
{
    k2d::Device device;
    if (!device.Init("K2D CustomShader Diag", 900, 400, true))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    if (!canvas.Init())
        return 1;

    unsigned int invertShader = canvas.CreateShader(kInvertFragmentShader);
    std::printf("invertShader program id = %u\n", invertShader);
    std::fflush(stdout);
    if (invertShader == 0)
        return 1;

    k2d::Assets assets;
    unsigned char orangePx[4] = {230, 140, 40, 255};
    k2d::Texture* orange = assets.CreateTexture("orange", 1, 1, orangePx);

    k2d::Scene scene;

    k2d::GameObject* backdropObject = scene.createObject("backdrop");
    backdropObject->setPosition(Math::Vec2(450.0f, 200.0f));
    k2d::SpriteComponent* backdrop = backdropObject->addComponent<k2d::SpriteComponent>(orange);
    backdrop->setSize(Math::Vec2(900.0f, 400.0f));
    backdrop->setColor(25, 25, 30);

    k2d::GameObject* litObject1 = scene.createObject("lit1");
    litObject1->setPosition(Math::Vec2(200.0f, 200.0f));
    k2d::SpriteComponent* lit1 = litObject1->addComponent<k2d::SpriteComponent>(orange);
    lit1->setSize(Math::Vec2(120.0f, 120.0f));
    lit1->setColor(230, 140, 40);

    k2d::GameObject* invertedObject = scene.createObject("inverted");
    invertedObject->setPosition(Math::Vec2(450.0f, 200.0f));
    k2d::SpriteComponent* inverted = invertedObject->addComponent<k2d::SpriteComponent>(orange);
    inverted->setSize(Math::Vec2(120.0f, 120.0f));
    inverted->setColor(230, 140, 40);
    inverted->setCustomShader(invertShader);

    k2d::GameObject* litObject2 = scene.createObject("lit2");
    litObject2->setPosition(Math::Vec2(700.0f, 200.0f));
    k2d::SpriteComponent* lit2 = litObject2->addComponent<k2d::SpriteComponent>(orange);
    lit2->setSize(Math::Vec2(120.0f, 120.0f));
    lit2->setColor(230, 140, 40);

    k2d::GameObject* lightObject = scene.createObject("light");
    lightObject->setPosition(Math::Vec2(450.0f, 100.0f));
    k2d::Light2D* light = lightObject->addComponent<k2d::Light2D>();
    light->setColor(1.0f, 1.0f, 1.0f);
    light->setEnergy(1.0f);
    light->setRadius(600.0f);
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
        if (frame == 5)
        {
            device.CaptureScreenshot();
            running = false;
        }
        device.Swap();
    }

    canvas.DestroyShader(invertShader);
    assets.Clear();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}
