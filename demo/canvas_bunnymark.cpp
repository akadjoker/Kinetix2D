#include <k2d/k2d.h>

#include <imgui.h>

#include <vector>

static const int SCANCODE_ESCAPE = 41;
static const int SCANCODE_1 = 30;
static const int SCANCODE_2 = 31;
static const int SCANCODE_R = 21;
static const int SCANCODE_F4 = 61;
static const int SCANCODE_C = 6;

static float gComponentMaxX = 1280.0f;
static float gComponentMaxY = 720.0f;

namespace
{
struct Bunny
{
    Math::Vec2 position;
    Math::Vec2 velocity;
};

static unsigned gSeed = 12345u;

float Random01()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return (float)(gSeed >> 8) / 16777216.0f;
}

class CanvasBunnyBehavior : public k2d::ScriptComponent
{
  public:
    CanvasBunnyBehavior() : mVelocity(80.0f + Random01() * 220.0f, -80.0f + Random01() * 160.0f)
    {
    }

  protected:
    void onUpdate(float deltaTime) override
    {
        Math::Vec2 position = owner()->position();
        mVelocity.y += 400.0f * deltaTime;
        position += mVelocity * deltaTime;

        if (position.x < 0.0f || position.x > gComponentMaxX)
        {
            position.x = position.x < 0.0f ? 0.0f : gComponentMaxX;
            mVelocity.x = -mVelocity.x;
        }
        if (position.y > gComponentMaxY)
        {
            position.y = gComponentMaxY;
            mVelocity.y = -mVelocity.y * 0.85f;
        }
        owner()->setPosition(position);
    }

  private:
    Math::Vec2 mVelocity;
};

unsigned int PackColor(unsigned char r, unsigned char g, unsigned char b)
{
    return (unsigned int)r | ((unsigned int)g << 8) | ((unsigned int)b << 16) | 0xFF000000u;
}

bool Intersects(const Bunny& bunny, float minX, float minY, float maxX, float maxY, float width, float height)
{
    return bunny.position.x + width >= minX && bunny.position.x <= maxX && bunny.position.y + height >= minY &&
           bunny.position.y <= maxY;
}

void AddBunnies(std::vector<Bunny>& bunnies, int count, float spawnWidth, float spawnHeight)
{
    for (int i = 0; i < count; ++i)
    {
        Bunny bunny;
        bunny.position = Math::Vec2(20.0f + Random01() * spawnWidth, 20.0f + Random01() * spawnHeight);
        bunny.velocity = Math::Vec2(80.0f + Random01() * 220.0f, -80.0f + Random01() * 160.0f);
        bunnies.push_back(bunny);
    }
}

void AddComponentBunnies(k2d::Scene& scene, k2d::Texture* texture, int count, float width, float height)
{
    for (int i = 0; i < count; ++i)
    {
        k2d::GameObject* object = scene.createObject("canvas_bunny");
        object->setPosition(Math::Vec2(20.0f + Random01() * width, 20.0f + Random01() * height));
        k2d::SpriteComponent* sprite = object->addComponent<k2d::SpriteComponent>(texture);
        sprite->setPivot(Math::Vec2(0.0f, 0.0f));
        object->addComponent<CanvasBunnyBehavior>();
    }
}
} // namespace

int main()
{
    k2d::Device device;
    if (!device.Init("Kinetix2D CanvasRenderer BunnyMark", 1280, 720, false))
        return 1;
    device.Focus();

    k2d::CanvasRenderer canvas;
    k2d::CanvasRenderer::Config config;
    config.maxVertices = 65532;
    config.maxDrawCalls = 64;
    if (!canvas.Init(config))
        return 1;

    k2d::Assets assets;
    k2d::Texture* bunnyTexture = assets.LoadTexture("canvas_bunny", "assets/wabbit_alpha.png");
    if (!bunnyTexture)
        bunnyTexture = assets.LoadTexture("canvas_bunny", "../../assets/wabbit_alpha.png");
    if (!bunnyTexture)
        return 1;

    unsigned char whitePixels[4] = {255, 255, 255, 255};
    k2d::Texture* white = assets.CreateTexture("canvas_bunny_white", 1, 1, whitePixels);

    std::vector<Bunny> bunnies;
    bunnies.reserve(10000);
    AddBunnies(bunnies, 1000, (float)device.Width() - 40.0f, (float)device.Height() - 40.0f);
    k2d::Scene componentScene;
    int componentBunnyCount = 1000;
    AddComponentBunnies(componentScene, bunnyTexture, componentBunnyCount, (float)device.Width() - 40.0f,
                        (float)device.Height() - 40.0f);

    k2d::Camera2D camera;
    camera.position = Math::Vec2(device.Width() * 0.5f, device.Height() * 0.5f);
    bool showProfiler = true;
    bool useComponents = false;
    k2d::Profiler::Get().SetEnabled(true);

    bool running = true;
    while (running)
    {
        k2d::Profiler::Get().beginFrame();
        running = device.PollEvents();
        k2d::Input& input = device.GetInput();
        if (input.KeyDown(SCANCODE_ESCAPE))
            running = false;

        if (input.KeyPressed(SCANCODE_1))
        {
            AddBunnies(bunnies, 100, (float)device.Width() - 40.0f, 180.0f);
            AddComponentBunnies(componentScene, bunnyTexture, 100, (float)device.Width() - 40.0f, 180.0f);
            componentBunnyCount += 100;
        }
        if (input.KeyPressed(SCANCODE_2))
        {
            AddBunnies(bunnies, 1000, (float)device.Width() - 40.0f, 180.0f);
            AddComponentBunnies(componentScene, bunnyTexture, 1000, (float)device.Width() - 40.0f, 180.0f);
            componentBunnyCount += 1000;
        }
        if (input.KeyPressed(SCANCODE_R))
        {
            bunnies.clear();
            componentScene.clear();
            componentBunnyCount = 0;
        }
        if (input.KeyPressed(SCANCODE_C))
            useComponents = !useComponents;
        if (input.KeyPressed(SCANCODE_F4))
            showProfiler = !showProfiler;
        if (input.KeyPressed(19))
            canvas.PrintStats();

        float dt = device.DeltaTime();
        const float screenWidth = (float)device.Width();
        const float screenHeight = (float)device.Height();
        const float bunnyWidth = (float)bunnyTexture->Width();
        const float bunnyHeight = (float)bunnyTexture->Height();
        gComponentMaxX = screenWidth - bunnyWidth;
        gComponentMaxY = screenHeight - bunnyHeight;

        camera.position = Math::Vec2(screenWidth * 0.5f, screenHeight * 0.5f);
        canvas.SetProjection(camera.Projection(screenWidth, screenHeight));

        float frustumMinX, frustumMinY, frustumMaxX, frustumMaxY;
        camera.VisibleRect(frustumMinX, frustumMinY, frustumMaxX, frustumMaxY, screenWidth, screenHeight);
        {
            k2d::ProfileScope profileSceneUpdate("scene.update");
            componentScene.update(dt);
        }
        {
            k2d::ProfileScope profileUpdate("canvas.update");
            float maxX = screenWidth - bunnyWidth;
            float maxY = screenHeight - bunnyHeight;
            for (std::size_t i = 0; i < bunnies.size(); ++i)
            {
                Bunny& bunny = bunnies[i];
                bunny.velocity.y += 400.0f * dt;
                bunny.position += bunny.velocity * dt;
                if (bunny.position.x < 0.0f || bunny.position.x > maxX)
                {
                    bunny.position.x = bunny.position.x < 0.0f ? 0.0f : maxX;
                    bunny.velocity.x = -bunny.velocity.x;
                }
                if (bunny.position.y > maxY)
                {
                    bunny.position.y = maxY;
                    bunny.velocity.y = -bunny.velocity.y * 0.85f;
                }
            }
        }

        if (device.WasResized())
        {
            camera.position = Math::Vec2(device.Width() * 0.5f, device.Height() * 0.5f);
            glViewport(0, 0, device.Width(), device.Height());
        }

        glClearColor(0.035f, 0.045f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        k2d::RenderQueue queue;
        int visibleBunnies = 0;
        {
            k2d::ProfileScope profileBuild("canvas.build_queue");
            k2d::RenderItem& background = queue.AddItem(-10);
            k2d::RenderCommand backgroundRect =
                k2d::RenderCommand::MakeRect(white->Id(), 0.0f, 0.0f, (float)device.Width(), (float)device.Height());
            backgroundRect.texWidth = 1;
            backgroundRect.texHeight = 1;
            backgroundRect.pivotX = 0.0f;
            backgroundRect.pivotY = 0.0f;
            backgroundRect.color = PackColor(18, 24, 38);
            background.commands.push_back(backgroundRect);

            for (std::size_t i = 0; i < bunnies.size(); ++i)
            {
                if (useComponents)
                    continue;
                if (!Intersects(bunnies[i], frustumMinX, frustumMinY, frustumMaxX, frustumMaxY, bunnyWidth,
                                bunnyHeight))
                    continue;

                ++visibleBunnies;
                k2d::RenderItem& item = queue.AddItem(0);
                item.commands.push_back(k2d::RenderCommand::MakeTransform(
                    k2d::Matrix2D::Translation(bunnies[i].position.x, bunnies[i].position.y)));
                k2d::RenderCommand bunny =
                    k2d::RenderCommand::MakeRect(bunnyTexture->Id(), 0.0f, 0.0f, bunnyWidth, bunnyHeight);
                bunny.texWidth = bunnyTexture->Width();
                bunny.texHeight = bunnyTexture->Height();
                bunny.pivotX = 0.0f;
                bunny.pivotY = 0.0f;
                bunny.color = PackColor(255, 255, 255);
                item.commands.push_back(bunny);
            }
        }
        if (useComponents)
        {
            k2d::ProfileScope profileComponents("scene.render");
            componentScene.render(canvas);
        }
        else
        {
            k2d::ProfileScope profileRender("canvas.render");
            queue.Flush(canvas);
        }
        device.BeginUI();
        ImGui::Begin("Canvas BunnyMark status");
        ImGui::Text("Modo: %s", useComponents ? "componentes" : "direto");
        ImGui::Text("Bunnies vivos: %d", useComponents ? componentBunnyCount : (int)bunnies.size());
        ImGui::Text("Bunnies na frustum: %d", useComponents ? componentBunnyCount : visibleBunnies);
        ImGui::Text("Frustum: %.0f,%.0f -> %.0f,%.0f", frustumMinX, frustumMinY, frustumMaxX, frustumMaxY);
        ImGui::Text("C: alternar modo   1: +100   2: +1000   R: limpar   P: stats");
        ImGui::End();
        if (showProfiler)
            k2d::ShowProfilerWindow(&showProfiler);
        device.EndUI();
        device.Swap();
        k2d::Profiler::Get().endFrame();
    }

    assets.Clear();
    canvas.Shutdown();
    device.Shutdown();
    return 0;
}
