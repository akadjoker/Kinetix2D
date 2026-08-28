#include <k2d/Component.h>
#include <k2d/BoxCollider2D.h>
#include <k2d/PhysicsWorld2D.h>
#include <k2d/RenderQueue.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Scene.h>
#include <kx/kx.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

class BenchmarkComponent final : public k2d::Component
{
  public:
    BenchmarkComponent()
        : Component(k2d::ComponentType::Script, k2d::ComponentEventUpdate | k2d::ComponentEventRender), mUpdates(0)
    {
    }

    int updates() const
    {
        return mUpdates;
    }

  protected:
    void onUpdate(float) override
    {
        ++mUpdates;
    }

    void onRender(k2d::RenderQueue& queue) override
    {
        queue.AddItem(owner()->zIndex(), true);
    }

  private:
    int mUpdates;
};

double elapsedMs(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

double profileClock()
{
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

void printStats(const char* name, std::vector<double>& samples)
{
    std::sort(samples.begin(), samples.end());
    double total = 0.0;
    for (double sample : samples)
        total += sample;
    const size_t p95Index = (samples.size() * 95) / 100;
    std::printf("%s_avg_ms=%.4f %s_p95_ms=%.4f\n", name, total / samples.size(), name,
                samples[p95Index < samples.size() ? p95Index : samples.size() - 1]);
}

void benchmarkScene()
{
    constexpr int objectCount = 2000;
    constexpr int warmupFrames = 30;
    constexpr int sampleFrames = 120;

    k2d::Scene scene;
    std::vector<BenchmarkComponent*> components;
    components.reserve(objectCount);
    for (int i = 0; i < objectCount; ++i)
    {
        k2d::GameObject* object = scene.createObject("benchmark");
        object->setZIndex(i % 8);
        components.push_back(object->addComponent<BenchmarkComponent>());
    }

    for (int i = 0; i < warmupFrames; ++i)
    {
        scene.update(1.0f / 60.0f);
        scene.buildRenderQueue();
    }

    std::vector<double> updateSamples;
    std::vector<double> renderSamples;
    updateSamples.reserve(sampleFrames);
    renderSamples.reserve(sampleFrames);
    for (int i = 0; i < sampleFrames; ++i)
    {
        auto start = Clock::now();
        scene.update(1.0f / 60.0f);
        auto updateEnd = Clock::now();
        scene.buildRenderQueue();
        auto renderEnd = Clock::now();
        updateSamples.push_back(elapsedMs(start, updateEnd));
        renderSamples.push_back(elapsedMs(updateEnd, renderEnd));
    }

    std::printf("scene_objects=%d scene_updates=%d render_items=%zu\n", objectCount, components[0]->updates(),
                scene.renderItemCount());
    printStats("scene_update", updateSamples);
    printStats("scene_render_queue", renderSamples);
}

void benchmarkPhysics()
{
    constexpr int bodyCount = 1000;
    constexpr int warmupFrames = 30;
    constexpr int sampleFrames = 120;

    kx::World world(Math::Vec2(0.0f, 0.0f));
    world.SetTimeSource(profileClock);
    for (int i = 0; i < bodyCount; ++i)
    {
        const float x = (i % 50) * 1.5f;
        const float y = (i / 50) * 1.5f;
        world.CreateBox(Math::Vec2(x, y), 0.8f, 0.8f, 1.0f);
    }

    for (int i = 0; i < warmupFrames; ++i)
        world.Step(1.0f / 60.0f);

    std::vector<double> samples;
    samples.reserve(sampleFrames);
    for (int i = 0; i < sampleFrames; ++i)
    {
        auto start = Clock::now();
        world.Step(1.0f / 60.0f);
        samples.push_back(elapsedMs(start, Clock::now()));
    }

    std::printf("physics_bodies=%d physics_contacts=%zu\n", bodyCount, world.ContactCount());
    const kx::StepProfile& profile = world.Profile();
    std::printf("physics_profile_integrate_ms=%.4f physics_profile_broadphase_ms=%.4f "
                "physics_profile_narrowphase_ms=%.4f physics_profile_solve_velocity_ms=%.4f "
                "physics_profile_solve_velocity_joints_ms=%.4f "
                "physics_profile_solve_velocity_contacts_ms=%.4f "
                "physics_profile_solve_position_ms=%.4f\n",
                profile.integrate, profile.broadphase, profile.narrowphase, profile.solveVelocity,
                profile.solveVelocityJoints, profile.solveVelocityContacts, profile.solvePosition);
    printStats("physics_step", samples);
}

void benchmarkPhysics2D()
{
    constexpr int bodyCount = 1000;
    constexpr int warmupFrames = 30;
    constexpr int sampleFrames = 120;

    k2d::Scene scene;
    for (int i = 0; i < bodyCount; ++i)
    {
        k2d::GameObject* object = scene.createObject("physics2d_benchmark");
        object->setPosition(Math::Vec2((i % 50) * 1.5f, (i / 50) * 1.5f));
        object->addComponent<k2d::RigidBody2D>();
        object->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(1.6f, 1.6f));
    }

    k2d::PhysicsWorld2D world(Math::Vec2(0.0f, 0.0f));
    world.setFixedTimeStep(1.0f / 60.0f);
    world.build(scene.root());
    for (int i = 0; i < warmupFrames; ++i)
        world.step(1.0f / 60.0f);

    std::vector<double> samples;
    samples.reserve(sampleFrames);
    for (int i = 0; i < sampleFrames; ++i)
    {
        auto start = Clock::now();
        world.step(1.0f / 60.0f);
        samples.push_back(elapsedMs(start, Clock::now()));
    }

    std::printf("physics2d_bodies=%zu physics2d_contacts=%zu\n", world.bodyCount(), world.contactCount());
    printStats("physics2d_step", samples);
    k2d::PhysicsWorld2D::SetActive(nullptr);
}
} // namespace

int main()
{
    benchmarkScene();
    benchmarkPhysics();
    benchmarkPhysics2D();
    return 0;
}
