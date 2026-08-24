#include <k2d/Collider2D.h>
#include <k2d/GameObject.h>
#include <k2d/PhysicsWorld2D.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Scene.h>

#include <cmath>
#include <cstdio>

static bool nearEqual(float a, float b, float tolerance = 1.0f)
{
    return std::fabs(a - b) < tolerance;
}

static k2d::GameObject *makeBox(k2d::Scene &scene, const char *name, const Math::Vec2 &position,
                                const Math::Vec2 &size, kx::BodyType type)
{
    k2d::GameObject *object = scene.createObject(name);
    object->setPosition(position);

    k2d::RigidBody2D *body = object->addComponent<k2d::RigidBody2D>();
    body->setBodyType(type);

    k2d::Collider2D *collider = object->addComponent<k2d::Collider2D>();
    collider->setShape(k2d::ColliderShape::Box);
    collider->setSize(size);
    return object;
}

static bool testBoxFallsAndRests()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f),
            kx::BodyType::Static);
    k2d::GameObject *box = makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f),
                                   kx::BodyType::Dynamic);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());

    bool ok = world.bodyCount() == 2;

    for (int i = 0; i < 180; ++i)
        world.step(1.0f / 60.0f);

    const float expectedRest = 300.0f - 20.0f - 20.0f;
    ok = ok && nearEqual(box->position().y, expectedRest, 2.0f);
    ok = ok && nearEqual(box->position().x, 0.0f, 1.0f);

    std::printf("  falls: bodies=%d y=%.1f (expected ~%.1f)\n", (int)world.bodyCount(),
                box->position().y, expectedRest);
    return ok;
}

struct ContactLog
{
    int begins = 0;
    int ends = 0;
    int sensors = 0;
    k2d::GameObject *lastOther = nullptr;
};

static void onContact(const k2d::CollisionInfo &info, void *user)
{
    ContactLog &log = *static_cast<ContactLog *>(user);
    if (info.began)
        ++log.begins;
    else
        ++log.ends;
    if (info.sensor)
        ++log.sensors;
    log.lastOther = info.other;
}

static bool testContactCallbackFires()
{
    k2d::Scene scene;
    k2d::GameObject *floor = makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f),
                                     Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);
    makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);

    ContactLog log;
    k2d::PhysicsWorld2D world;
    world.setCollisionCallback(&onContact, &log);
    world.build(scene.root());

    for (int i = 0; i < 180; ++i)
        world.step(1.0f / 60.0f);

    const bool ok = log.begins > 0 && log.lastOther != nullptr &&
                    (log.lastOther == floor || log.lastOther->name() == ct::String("box"));

    std::printf("  contacts: begins=%d ends=%d contacts_now=%d\n", log.begins, log.ends,
                (int)world.contactCount());
    return ok;
}

static bool testSensorReportsWithoutBlocking()
{
    k2d::Scene scene;
    k2d::GameObject *trigger = scene.createObject("trigger");
    trigger->setPosition(Math::Vec2(0.0f, 150.0f));
    k2d::RigidBody2D *triggerBody = trigger->addComponent<k2d::RigidBody2D>();
    triggerBody->setBodyType(kx::BodyType::Static);
    k2d::Collider2D *triggerShape = trigger->addComponent<k2d::Collider2D>();
    triggerShape->setShape(k2d::ColliderShape::Box);
    triggerShape->setSize(Math::Vec2(400.0f, 20.0f));
    triggerShape->setSensor(true);

    makeBox(scene, "floor", Math::Vec2(0.0f, 400.0f), Math::Vec2(600.0f, 40.0f),
            kx::BodyType::Static);
    k2d::GameObject *box = makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f),
                                   kx::BodyType::Dynamic);

    ContactLog log;
    k2d::PhysicsWorld2D world;
    world.setCollisionCallback(&onContact, &log);
    world.build(scene.root());

    bool ok = triggerShape->shapeIndex() == 0 && triggerBody->body() &&
              triggerBody->body()->IsSensor(triggerShape->shapeIndex());

    for (int i = 0; i < 240; ++i)
        world.step(1.0f / 60.0f);

    ok = ok && log.sensors > 0;
    ok = ok && box->position().y > 300.0f;

    std::printf("  sensor: sensor_events=%d box_y=%.1f (passed through)\n", log.sensors,
                box->position().y);
    return ok;
}

static bool testRaycastAndQueries()
{
    k2d::Scene scene;
    k2d::GameObject *wall = makeBox(scene, "wall", Math::Vec2(200.0f, 0.0f),
                                    Math::Vec2(40.0f, 200.0f), kx::BodyType::Static);

    k2d::PhysicsWorld2D world;
    world.setGravity(Math::Vec2(0.0f, 0.0f));
    world.build(scene.root());
    world.step(1.0f / 60.0f);

    Math::Vec2 point(0.0f, 0.0f);
    Math::Vec2 normal(0.0f, 0.0f);
    k2d::GameObject *hit =
        world.raycast(Math::Vec2(0.0f, 0.0f), Math::Vec2(1.0f, 0.0f), 500.0f, &point, &normal);

    bool ok = hit == wall;
    ok = ok && nearEqual(point.x, 180.0f, 2.0f);

    ct::Vector<k2d::GameObject *> found;
    world.overlapCircle(Math::Vec2(200.0f, 0.0f), 60.0f, found);
    ok = ok && found.size() == 1 && found[0] == wall;

    std::printf("  queries: ray_hit=%s point=(%.1f, %.1f) overlap=%d\n",
                hit ? hit->name().c_str() : "none", point.x, point.y, (int)found.size());
    return ok;
}

static bool testStaticBodyFollowsItsTransform()
{
    k2d::Scene scene;
    k2d::GameObject *platform = makeBox(scene, "platform", Math::Vec2(0.0f, 300.0f),
                                        Math::Vec2(200.0f, 20.0f), kx::BodyType::Static);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());
    world.step(1.0f / 60.0f);

    ct::Vector<k2d::GameObject *> found;
    world.overlapCircle(Math::Vec2(0.0f, 300.0f), 30.0f, found);
    bool ok = found.size() == 1 && found[0] == platform;

    platform->setPosition(Math::Vec2(400.0f, 300.0f));
    world.step(1.0f / 60.0f);

    world.overlapCircle(Math::Vec2(400.0f, 300.0f), 30.0f, found);
    ok = ok && found.size() == 1 && found[0] == platform;
    world.overlapCircle(Math::Vec2(0.0f, 300.0f), 30.0f, found);
    ok = ok && found.empty();

    std::printf("  static_follow: platform tracked after moving its transform\n");
    return ok;
}

static bool testImpulseAndVelocity()
{
    k2d::Scene scene;
    k2d::GameObject *box = makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f),
                                   kx::BodyType::Dynamic);
    k2d::RigidBody2D *body = box->getComponent<k2d::RigidBody2D>();
    body->setGravityScale(0.0f);
    body->setVelocity(Math::Vec2(100.0f, 0.0f));

    k2d::PhysicsWorld2D world;
    world.setGravity(Math::Vec2(0.0f, 0.0f));
    world.build(scene.root());

    for (int i = 0; i < 60; ++i)
        world.step(1.0f / 60.0f);

    bool ok = nearEqual(box->position().x, 100.0f, 5.0f);
    ok = ok && nearEqual(body->velocity().x, 100.0f, 5.0f);

    std::printf("  velocity: x=%.1f vx=%.1f\n", box->position().x, body->velocity().x);
    return ok;
}

static bool testFixedStepIsDeterministic()
{
    float results[2] = {0.0f, 0.0f};
    for (int run = 0; run < 2; ++run)
    {
        k2d::Scene scene;
        makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f),
                kx::BodyType::Static);
        k2d::GameObject *box = makeBox(scene, "box", Math::Vec2(13.0f, -200.0f),
                                       Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);
        box->setRotationDegrees(20.0f);

        k2d::PhysicsWorld2D world;
        world.build(scene.root());
        for (int i = 0; i < 300; ++i)
            world.step(1.0f / 60.0f);
        results[run] = box->position().y;
    }

    const bool ok = nearEqual(results[0], results[1], 0.0001f);
    std::printf("  determinism: run_a=%.4f run_b=%.4f\n", results[0], results[1]);
    return ok;
}

int main()
{
    const bool falls = testBoxFallsAndRests();
    const bool contacts = testContactCallbackFires();
    const bool sensor = testSensorReportsWithoutBlocking();
    const bool queries = testRaycastAndQueries();
    const bool staticFollow = testStaticBodyFollowsItsTransform();
    const bool velocity = testImpulseAndVelocity();
    const bool deterministic = testFixedStepIsDeterministic();

    std::printf("physics2d: falls=%s contacts=%s sensor=%s queries=%s static_follow=%s "
                "velocity=%s determinism=%s\n",
                falls ? "pass" : "fail", contacts ? "pass" : "fail", sensor ? "pass" : "fail",
                queries ? "pass" : "fail", staticFollow ? "pass" : "fail",
                velocity ? "pass" : "fail", deterministic ? "pass" : "fail");
    return falls && contacts && sensor && queries && staticFollow && velocity && deterministic ? 0
                                                                                              : 1;
}
