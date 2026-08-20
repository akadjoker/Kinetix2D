#include <k2d/k2d.h>

#include <glad/glad.h>
#include <ct/string.hpp>

static const int SCANCODE_ESCAPE = 41;

class Spin : public k2d::ScriptComponent
{
public:
    explicit Spin(float degreesPerSecond) : mDegreesPerSecond(degreesPerSecond)
    {
    }

protected:
    void onUpdate(float deltaTime) override
    {
        owner()->rotate(mDegreesPerSecond * deltaTime);
    }

private:
    float mDegreesPerSecond;
};

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D", 1280, 720, true))
        return 1;

    k2d::BatchRenderer batch;
    k2d::BatchRenderer::Config config;
    if (!batch.Init(config))
        return 1;

    batch.Resize(device.Width(), device.Height());

    k2d::Assets assets;

    unsigned char checker[8 * 8 * 4];
    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            bool light = ((x + y) & 1) == 0;
            unsigned char v = light ? 200 : 90;
            int i = (y * 8 + x) * 4;
            checker[i + 0] = v;
            checker[i + 1] = v;
            checker[i + 2] = v;
            checker[i + 3] = 255;
        }
    }

    k2d::Texture *checkerTex = assets.CreateTexture("checker", 8, 8, checker);

    k2d::Scene scene;

    k2d::GameObject *sun = scene.createObject("sun");
    sun->setPosition(glm::vec2(device.Width() * 0.5f, device.Height() * 0.5f));
    k2d::SpriteComponent *sunSprite = sun->addComponent<k2d::SpriteComponent>(checkerTex);
    sunSprite->setSize(glm::vec2(100.0f, 100.0f));
    sun->addComponent<Spin>(30.0f);

    k2d::GameObject *planet = scene.createObject("planet", sun);
    planet->setPosition(glm::vec2(180.0f, 0.0f));
    k2d::SpriteComponent *planetSprite = planet->addComponent<k2d::SpriteComponent>(checkerTex);
    planetSprite->setSize(glm::vec2(40.0f, 40.0f));

    k2d::GameObject *moon = scene.createObject("moon", planet);
    moon->setPosition(glm::vec2(60.0f, 0.0f));
    k2d::SpriteComponent *moonSprite = moon->addComponent<k2d::SpriteComponent>(checkerTex);
    moonSprite->setSize(glm::vec2(16.0f, 16.0f));
    moon->addComponent<Spin>(180.0f);

    bool running = true;
    while (running)
    {
        running = device.PollEvents();

        k2d::Input &input = device.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;

        if (device.WasResized())
        {
            batch.Resize(device.Width(), device.Height());
            glViewport(0, 0, device.Width(), device.Height());
        }

        float dt = device.DeltaTime();

        scene.update(dt);

        glClearColor(0.05f, 0.08f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        batch.BeginFrame();

        scene.render(batch);

        batch.SetColor((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255);
        batch.DrawText(20.0f, 650.0f, 32.0f, "Kinetix2D");

        ct::String info;
        info += "FPS ";
        info.append_number((int)(dt > 0.0f ? 1.0f / dt : 0.0f));
        batch.DrawText(20.0f, 690.0f, 16.0f, info.c_str());

        batch.EndFrame();

        device.Swap();
    }

    assets.Clear();
    batch.Shutdown();
    device.Shutdown();

    return 0;
}
