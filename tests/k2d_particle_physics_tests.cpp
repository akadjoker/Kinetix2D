#include <k2d/BoxCollider2D.h>
#include <k2d/Collider2D.h>
#include <k2d/GameObject.h>
#include <k2d/ParticlePhysics2D.h>
#include <k2d/RigidBody2D.h>
#include <k2d/RenderQueue.h>
#include <k2d/Scene.h>
#include <k2d/Texture.h>

#include <cmath>
#include <cstdio>

namespace
{

struct HitLog
{
    int count = 0;
    Math::Vec2 point = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 normal = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 velocity = Math::Vec2(0.0f, 0.0f);
    uint32_t tag = 0;
    k2d::GameObject* other = nullptr;
};

void recordHit(const k2d::ParticleHit2D& hit, void* user)
{
    HitLog* log = static_cast<HitLog*>(user);
    ++log->count;
    log->point = hit.point;
    log->normal = hit.normal;
    log->velocity = hit.incomingVelocity;
    log->tag = hit.userTag;
    log->other = hit.other;
}

bool nearEqual(float a, float b, float tolerance = 0.01f)
{
    return std::fabs(a - b) < tolerance;
}

k2d::GameObject* makeFloor(k2d::Scene& scene, const Math::Vec2& position, const Math::Vec2& size,
                           uint16_t category = 0x0001)
{
    k2d::GameObject* object = scene.createObject("floor");
    object->setPosition(position);
    object->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    k2d::BoxCollider2D* collider = object->addComponent<k2d::BoxCollider2D>();
    collider->setSize(size);
    collider->setFilter(category, 0xFFFF);
    return object;
}

k2d::ParticleSpawn2D straightDown(float speed, k2d::ParticleResponse response)
{
    k2d::ParticleSpawn2D spawn;
    spawn.position = Math::Vec2(0.0f, 0.0f);
    spawn.velocity = Math::Vec2(0.0f, speed);
    spawn.radius = 1.0f;
    spawn.life = 10.0f;
    spawn.drag = 0.0f;
    spawn.response = response;
    return spawn;
}

bool testBounceReflectsAndRestitutionDamps()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);
    makeFloor(scene, Math::Vec2(0.0f, 100.0f), Math::Vec2(400.0f, 20.0f));

    HitLog log;
    k2d::ParticlePhysics2D& particles = scene.particles();
    particles.setGravity(Math::Vec2(0.0f, 0.0f));
    particles.setHitCallback(&recordHit, &log);

    k2d::ParticleSpawn2D spawn = straightDown(500.0f, k2d::ParticleResponse::Bounce);
    spawn.restitution = 0.5f;
    particles.emit(spawn);

    for (int i = 0; i < 4 && log.count == 0; ++i)
        scene.update(0.1f);

    bool ok = log.count == 1 && particles.count() == 1;
    const k2d::PhysicsParticle2D& particle = particles.at(0);
    ok = ok && particle.velocity.y < 0.0f;
    ok = ok && nearEqual(particle.velocity.y, -250.0f, 1.0f);
    ok = ok && nearEqual(particle.velocity.x, 0.0f, 0.001f);
    // The box top edge is at y = 90; the particle must end up above it.
    ok = ok && particle.position.y < 90.0f;
    ok = ok && log.other != nullptr;

    std::printf("  bounce: hit=%d normal=(%g,%g) point=(%g,%g) out_v=(%g,%g) pos_y=%g\n", log.count,
                log.normal.x, log.normal.y, log.point.x, log.point.y, particle.velocity.x,
                particle.velocity.y, particle.position.y);
    return ok;
}

bool testKillRemovesParticle()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);
    makeFloor(scene, Math::Vec2(0.0f, 100.0f), Math::Vec2(400.0f, 20.0f));

    HitLog log;
    k2d::ParticlePhysics2D& particles = scene.particles();
    particles.setGravity(Math::Vec2(0.0f, 0.0f));
    particles.setHitCallback(&recordHit, &log);
    particles.emit(straightDown(500.0f, k2d::ParticleResponse::Kill));

    bool ok = particles.count() == 1;
    for (int i = 0; i < 4 && log.count == 0; ++i)
        scene.update(0.1f);

    ok = ok && log.count == 1 && particles.count() == 0;
    return ok;
}

bool testStickSettlesAndStopsIntegrating()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);
    makeFloor(scene, Math::Vec2(0.0f, 100.0f), Math::Vec2(400.0f, 20.0f));

    HitLog log;
    k2d::ParticlePhysics2D& particles = scene.particles();
    particles.setGravity(Math::Vec2(0.0f, 980.0f));
    particles.setHitCallback(&recordHit, &log);
    particles.emit(straightDown(500.0f, k2d::ParticleResponse::Stick));

    for (int i = 0; i < 4 && log.count == 0; ++i)
        scene.update(0.1f);

    bool ok = log.count == 1 && particles.count() == 1;
    ok = ok && particles.at(0).settled;
    ok = ok && nearEqual(particles.at(0).velocity.x, 0.0f, 0.001f);
    ok = ok && nearEqual(particles.at(0).velocity.y, 0.0f, 0.001f);

    const Math::Vec2 resting = particles.at(0).position;
    for (int i = 0; i < 5; ++i)
        scene.update(0.1f);

    ok = ok && particles.count() == 1;
    ok = ok && nearEqual(particles.at(0).position.x, resting.x, 0.0001f);
    ok = ok && nearEqual(particles.at(0).position.y, resting.y, 0.0001f);
    // Gravity kept accumulating would show up here; a settled particle skips it.
    ok = ok && nearEqual(particles.at(0).velocity.y, 0.0f, 0.001f);
    ok = ok && log.count == 1;
    return ok;
}

bool testCategoryMaskFiltersCollider()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);
    makeFloor(scene, Math::Vec2(0.0f, 100.0f), Math::Vec2(400.0f, 20.0f), 0x0002);

    HitLog log;
    k2d::ParticlePhysics2D& particles = scene.particles();
    particles.setGravity(Math::Vec2(0.0f, 0.0f));
    particles.setCategoryMask(0x0001);
    particles.setHitCallback(&recordHit, &log);
    particles.emit(straightDown(500.0f, k2d::ParticleResponse::Bounce));

    for (int i = 0; i < 6; ++i)
        scene.update(0.1f);

    bool ok = log.count == 0 && particles.count() == 1;
    ok = ok && particles.at(0).position.y > 110.0f;

    particles.setCategoryMask(0x0002);
    particles.clear();
    particles.emit(straightDown(-500.0f, k2d::ParticleResponse::Bounce));
    for (int i = 0; i < 6 && log.count == 0; ++i)
        scene.update(0.1f);
    ok = ok && log.count == 0;

    particles.clear();
    k2d::ParticleSpawn2D spawn = straightDown(500.0f, k2d::ParticleResponse::Bounce);
    particles.emit(spawn);
    for (int i = 0; i < 4 && log.count == 0; ++i)
        scene.update(0.1f);
    ok = ok && log.count == 1;
    return ok;
}

bool testEmptySystemStepsSafely()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);

    k2d::ParticlePhysics2D& particles = scene.particles();
    particles.step(0.016f);
    particles.step(0.0f);
    particles.step(-1.0f);

    k2d::RenderQueue& queue = scene.buildRenderQueue();
    return particles.count() == 0 && queue.ItemCount() == 0 && queue.CommandCount() == 0;
}

bool testExplodeSpawnsRequestedCount()
{
    k2d::Scene scene;
    k2d::ParticlePhysics2D& particles = scene.particles();
    particles.setSeed(1234u);
    particles.explode(Math::Vec2(10.0f, 20.0f), 24, 50.0f, 200.0f, 1.0f, 2.0f, 1.5f, 7u,
                      k2d::ParticleResponse::Bounce);

    bool ok = particles.count() == 24;
    bool spread = false;
    for (std::size_t i = 0; ok && i < particles.count(); ++i)
    {
        const k2d::PhysicsParticle2D& particle = particles.at(i);
        ok = ok && particle.userTag == 7u;
        ok = ok && particle.response == k2d::ParticleResponse::Bounce;
        ok = ok && nearEqual(particle.position.x, 10.0f) && nearEqual(particle.position.y, 20.0f);
        const float speed = particle.velocity.Length();
        ok = ok && speed >= 49.0f && speed <= 201.0f;
        if (particle.velocity.x < 0.0f)
            spread = true;
    }
    return ok && spread;
}

bool testMaxParticlesCapsThePool()
{
    k2d::Scene scene;
    k2d::ParticlePhysics2D& particles = scene.particles();
    particles.setMaxParticles(4);
    particles.explode(Math::Vec2(0.0f, 0.0f), 100, 10.0f, 20.0f, 1.0f, 1.0f, 1.0f);

    bool ok = particles.count() == 4;

    k2d::ParticleSpawn2D spawn = straightDown(10.0f, k2d::ParticleResponse::Kill);
    ok = ok && particles.emit(spawn) == k2d::ParticlePhysics2D::InvalidId;
    ok = ok && particles.count() == 4;

    particles.clear();
    ok = ok && particles.count() == 0;
    ok = ok && particles.emit(spawn) != k2d::ParticlePhysics2D::InvalidId;
    return ok;
}

bool testTexturelessParticlesSimulateButDrawNothing()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);
    makeFloor(scene, Math::Vec2(0.0f, 100.0f), Math::Vec2(400.0f, 20.0f));

    HitLog log;
    k2d::ParticlePhysics2D& particles = scene.particles();
    particles.setGravity(Math::Vec2(0.0f, 0.0f));
    particles.setHitCallback(&recordHit, &log);
    particles.emit(straightDown(500.0f, k2d::ParticleResponse::Bounce));

    for (int i = 0; i < 4 && log.count == 0; ++i)
        scene.update(0.1f);

    k2d::RenderQueue& queue = scene.buildRenderQueue();
    return log.count == 1 && particles.count() == 1 && queue.ItemCount() == 0 && queue.CommandCount() == 0;
}

bool testBurstBatchesIntoOneRenderItem()
{
    k2d::Scene scene;
    k2d::Texture texture;
    k2d::Texture other;

    k2d::ParticlePhysics2D& particles = scene.particles();
    particles.setSeed(99u);
    particles.explode(Math::Vec2(0.0f, 0.0f), 8, 10.0f, 20.0f, 1.0f, 1.0f, 1.0f, 0u,
                      k2d::ParticleResponse::Bounce, &texture, Math::Vec2(4.0f, 4.0f));

    k2d::RenderQueue& queue = scene.buildRenderQueue();
    bool ok = queue.ItemCount() == 1 && queue.CommandCount() == 16;

    particles.explode(Math::Vec2(0.0f, 0.0f), 3, 10.0f, 20.0f, 1.0f, 1.0f, 1.0f, 0u,
                      k2d::ParticleResponse::Bounce, &other, Math::Vec2(4.0f, 4.0f));
    k2d::RenderQueue& mixed = scene.buildRenderQueue();
    ok = ok && mixed.ItemCount() == 2 && mixed.CommandCount() == 22;
    return ok;
}

bool testColorInterpolatesOverLife()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);

    k2d::ParticlePhysics2D& particles = scene.particles();
    particles.setGravity(Math::Vec2(0.0f, 0.0f));

    k2d::ParticleSpawn2D spawn;
    spawn.position = Math::Vec2(0.0f, 0.0f);
    spawn.velocity = Math::Vec2(10.0f, 0.0f);
    spawn.life = 1.0f;
    spawn.drag = 0.0f;
    spawn.colorStart = k2d::Color(1.0f, 1.0f, 1.0f, 1.0f);
    spawn.colorEnd = k2d::Color(0.0f, 0.0f, 0.0f, 0.0f);
    particles.emit(spawn);

    bool ok = nearEqual(particles.colorAt(0).a, 1.0f);
    scene.update(0.5f);
    ok = ok && particles.count() == 1;

    const k2d::Color half = particles.colorAt(0);
    ok = ok && nearEqual(half.a, 0.5f) && nearEqual(half.r, 0.5f);
    ok = ok && half.a < 1.0f && half.a > 0.0f;

    scene.update(0.5f);
    ok = ok && particles.count() == 0;
    return ok;
}

} // namespace

int main()
{
    const bool bounce = testBounceReflectsAndRestitutionDamps();
    const bool kill = testKillRemovesParticle();
    const bool stick = testStickSettlesAndStopsIntegrating();
    const bool mask = testCategoryMaskFiltersCollider();
    const bool empty = testEmptySystemStepsSafely();
    const bool explode = testExplodeSpawnsRequestedCount();
    const bool cap = testMaxParticlesCapsThePool();
    const bool noTexture = testTexturelessParticlesSimulateButDrawNothing();
    const bool batching = testBurstBatchesIntoOneRenderItem();
    const bool colorRamp = testColorInterpolatesOverLife();

    std::printf("particle_physics: bounce=%s kill=%s stick=%s mask=%s empty=%s explode=%s max_particles=%s "
                "no_texture=%s batching=%s color_ramp=%s\n",
                bounce ? "pass" : "fail", kill ? "pass" : "fail", stick ? "pass" : "fail",
                mask ? "pass" : "fail", empty ? "pass" : "fail", explode ? "pass" : "fail",
                cap ? "pass" : "fail", noTexture ? "pass" : "fail", batching ? "pass" : "fail",
                colorRamp ? "pass" : "fail");

    return bounce && kill && stick && mask && empty && explode && cap && noTexture && batching && colorRamp
               ? 0
               : 1;
}
