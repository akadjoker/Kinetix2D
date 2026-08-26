#include <k2d/BoxCollider2D.h>
#include <k2d/ChainCollider2D.h>
#include <k2d/CharacterBody2D.h>
#include <k2d/CircleCollider2D.h>
#include <k2d/Collider2D.h>
#include <k2d/EdgeCollider2D.h>
#include <k2d/PolygonCollider2D.h>
#include <k2d/GameObject.h>
#include <k2d/Physics2DSerializer.h>
#include <k2d/PhysicsWorld2D.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>
#include <k2d/TileMapComponent.h>

#include <cmath>
#include <cstdio>

static bool nearEqual(float a, float b, float tolerance = 1.0f)
{
    return std::fabs(a - b) < tolerance;
}

static k2d::GameObject* makeBox(k2d::Scene& scene, const char* name, const Math::Vec2& position, const Math::Vec2& size,
                                kx::BodyType type)
{
    k2d::GameObject* object = scene.createObject(name);
    object->setPosition(position);

    k2d::RigidBody2D* body = object->addComponent<k2d::RigidBody2D>();
    body->setBodyType(type);

    k2d::BoxCollider2D* collider = object->addComponent<k2d::BoxCollider2D>();
    collider->setSize(size);
    return object;
}

static bool testBoxFallsAndRests()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);
    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());

    bool ok = world.bodyCount() == 2;

    for (int i = 0; i < 180; ++i)
        world.step(1.0f / 60.0f);

    const float expectedRest = 300.0f - 20.0f - 20.0f;
    ok = ok && nearEqual(box->position().y, expectedRest, 2.0f);
    ok = ok && nearEqual(box->position().x, 0.0f, 1.0f);

    std::printf("  falls: bodies=%d y=%.1f (expected ~%.1f)\n", (int)world.bodyCount(), box->position().y,
                expectedRest);
    return ok;
}

static bool testPaintedTileMapCollision()
{
    k2d::Scene scene;
    k2d::GameObject* mapObject = scene.createObject("tile floor");
    mapObject->setPosition(Math::Vec2(0.0f, 200.0f));
    k2d::TileMapComponent* map = mapObject->addComponent<k2d::TileMapComponent>();
    map->setCellSize(16.0f, 16.0f);
    map->setMapSize(5, 5);
    map->setCollision(0, 0, true);

    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(8.0f, 0.0f), Math::Vec2(16.0f, 16.0f), kx::BodyType::Dynamic);
    k2d::PhysicsWorld2D world;
    world.build(scene.root());
    for (int i = 0; i < 180; ++i)
        world.step(1.0f / 60.0f);

    const bool ok = world.bodyCount() == 2 && nearEqual(box->position().y, 192.0f, 2.0f) &&
                    world.objectAtPoint(Math::Vec2(8.0f, 208.0f)) == mapObject;
    std::printf("  tilemap: bodies=%d y=%.1f\n", (int)world.bodyCount(), box->position().y);
    return ok;
}

struct ContactLog
{
    int begins = 0;
    int ends = 0;
    int sensors = 0;
    k2d::GameObject* lastOther = nullptr;
};

static void onContact(const k2d::CollisionInfo& info, void* user)
{
    ContactLog& log = *static_cast<ContactLog*>(user);
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
    k2d::GameObject* floor =
        makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);
    makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);

    ContactLog log;
    k2d::PhysicsWorld2D world;
    world.setCollisionCallback(&onContact, &log);
    world.build(scene.root());

    for (int i = 0; i < 180; ++i)
        world.step(1.0f / 60.0f);

    const bool ok = log.begins > 0 && log.lastOther != nullptr &&
                    (log.lastOther == floor || log.lastOther->name() == ct::String("box"));

    std::printf("  contacts: begins=%d ends=%d contacts_now=%d\n", log.begins, log.ends, (int)world.contactCount());
    return ok;
}

static bool testSensorReportsWithoutBlocking()
{
    k2d::Scene scene;
    k2d::GameObject* trigger = scene.createObject("trigger");
    trigger->setPosition(Math::Vec2(0.0f, 150.0f));
    k2d::RigidBody2D* triggerBody = trigger->addComponent<k2d::RigidBody2D>();
    triggerBody->setBodyType(kx::BodyType::Static);
    k2d::BoxCollider2D* triggerShape = trigger->addComponent<k2d::BoxCollider2D>();
    triggerShape->setSize(Math::Vec2(400.0f, 20.0f));
    triggerShape->setSensor(true);

    makeBox(scene, "floor", Math::Vec2(0.0f, 400.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);
    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);

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

    std::printf("  sensor: sensor_events=%d box_y=%.1f (passed through)\n", log.sensors, box->position().y);
    return ok;
}

static bool testRaycastAndQueries()
{
    k2d::Scene scene;
    k2d::GameObject* wall =
        makeBox(scene, "wall", Math::Vec2(200.0f, 0.0f), Math::Vec2(40.0f, 200.0f), kx::BodyType::Static);

    k2d::PhysicsWorld2D world;
    world.setGravity(Math::Vec2(0.0f, 0.0f));
    world.build(scene.root());
    world.step(1.0f / 60.0f);

    Math::Vec2 point(0.0f, 0.0f);
    Math::Vec2 normal(0.0f, 0.0f);
    k2d::GameObject* hit = world.raycast(Math::Vec2(0.0f, 0.0f), Math::Vec2(1.0f, 0.0f), 500.0f, &point, &normal);

    bool ok = hit == wall;
    ok = ok && nearEqual(point.x, 180.0f, 2.0f);

    ct::Vector<k2d::GameObject*> found;
    world.overlapCircle(Math::Vec2(200.0f, 0.0f), 60.0f, found);
    ok = ok && found.size() == 1 && found[0] == wall;

    std::printf("  queries: ray_hit=%s point=(%.1f, %.1f) overlap=%d\n", hit ? hit->name().c_str() : "none", point.x,
                point.y, (int)found.size());
    return ok;
}

static bool testCharacterBodyMotion()
{
    k2d::Scene scene;
    k2d::GameObject* wall =
        makeBox(scene, "wall", Math::Vec2(0.0f, 0.0f), Math::Vec2(20.0f, 160.0f), kx::BodyType::Static);
    k2d::GameObject* player =
        makeBox(scene, "player", Math::Vec2(-80.0f, 0.0f), Math::Vec2(20.0f, 20.0f), kx::BodyType::Kinematic);
    k2d::CharacterBody2D* character = player->addComponent<k2d::CharacterBody2D>();

    k2d::PhysicsWorld2D world(Math::Vec2(0.0f, 0.0f));
    world.build(scene.root());

    const bool freeBefore = character->placeFree(-80.0f, 0.0f);
    const bool occupiedWall = !character->placeFree(0.0f, 0.0f) && character->placeMeeting(0.0f, 0.0f) == wall;
    const k2d::CollisionInfo hit = character->moveAndCollide(Math::Vec2(160.0f, 0.0f));
    bool ok = freeBefore && occupiedWall && hit.hit && hit.other == wall && hit.normal.x < -0.99f &&
              nearEqual(player->position().x, -20.5f, 2.0f);

    player->setPosition(Math::Vec2(-80.0f, -40.0f));
    player->getComponent<k2d::RigidBody2D>()->body()->SetPosition(player->globalPosition());
    character->setVelocity(Math::Vec2(4800.0f, 2400.0f));
    const bool slid = character->moveAndSlide();
    ok = ok && slid && character->isOnWall() && character->velocity().x < 1.0f && character->velocity().y > 2000.0f;

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf(
        "  character: free=%d occupied=%d hit=%s normal=(%.2f, %.2f) x=%.1f slide=%d wall=%d velocity=(%.1f, %.1f)\n",
        freeBefore ? 1 : 0, occupiedWall ? 1 : 0, hit.hit ? "yes" : "no", hit.normal.x, hit.normal.y,
        player->position().x, slid ? 1 : 0, character->isOnWall() ? 1 : 0, character->velocity().x,
        character->velocity().y);
    return ok;
}

static bool testStaticBodyFollowsItsTransform()
{
    k2d::Scene scene;
    k2d::GameObject* platform =
        makeBox(scene, "platform", Math::Vec2(0.0f, 300.0f), Math::Vec2(200.0f, 20.0f), kx::BodyType::Static);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());
    world.step(1.0f / 60.0f);

    ct::Vector<k2d::GameObject*> found;
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
    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);
    k2d::RigidBody2D* body = box->getComponent<k2d::RigidBody2D>();
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
        makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);
        k2d::GameObject* box =
            makeBox(scene, "box", Math::Vec2(13.0f, -200.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);
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

static bool testDestroyingAnObjectRemovesItsBody()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);
    k2d::GameObject* doomed =
        makeBox(scene, "doomed", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);
    k2d::GameObject* keeper =
        makeBox(scene, "keeper", Math::Vec2(200.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());
    bool ok = world.bodyCount() == 3;

    for (int i = 0; i < 30; ++i)
    {
        scene.update(1.0f / 60.0f);
        world.step(1.0f / 60.0f);
    }

    scene.destroy(doomed);
    ok = ok && world.bodyCount() == 3;
    scene.update(1.0f / 60.0f);
    ok = ok && world.bodyCount() == 2;

    for (int i = 0; i < 150; ++i)
    {
        scene.update(1.0f / 60.0f);
        world.step(1.0f / 60.0f);
    }

    ok = ok && nearEqual(keeper->position().y, 260.0f, 2.0f);

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  destroy: bodies=%d keeper_y=%.1f (no dangling body)\n", (int)world.bodyCount(),
                keeper->position().y);
    return ok;
}

static bool testObjectSpawnedDuringPlayGetsABody()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());
    bool ok = world.bodyCount() == 1;

    world.step(1.0f / 60.0f);

    k2d::GameObject* late =
        makeBox(scene, "late", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);

    for (int i = 0; i < 180; ++i)
        world.step(1.0f / 60.0f);

    ok = ok && world.bodyCount() == 2;
    ok = ok && nearEqual(late->position().y, 260.0f, 2.0f);

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  late_spawn: bodies=%d y=%.1f (fell and landed)\n", (int)world.bodyCount(), late->position().y);
    return ok;
}

static bool testColliderChangeRebuildsTheBody()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);
    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());

    for (int i = 0; i < 180; ++i)
        world.step(1.0f / 60.0f);

    bool ok = nearEqual(box->position().y, 260.0f, 2.0f);

    box->getComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(40.0f, 120.0f));
    for (int i = 0; i < 120; ++i)
        world.step(1.0f / 60.0f);

    ok = ok && nearEqual(box->position().y, 220.0f, 3.0f);
    ok = ok && world.bodyCount() == 2;

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  collider_change: taller box now rests at y=%.1f (was 260)\n", box->position().y);
    return ok;
}

static bool testBodyTypeAndDensityApplyLive()
{
    k2d::Scene scene;
    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);
    k2d::RigidBody2D* body = box->getComponent<k2d::RigidBody2D>();

    k2d::PhysicsWorld2D world;
    world.build(scene.root());

    for (int i = 0; i < 60; ++i)
        world.step(1.0f / 60.0f);

    const float fellTo = box->position().y;
    bool ok = fellTo > 100.0f;

    body->setBodyType(kx::BodyType::Static);
    for (int i = 0; i < 60; ++i)
        world.step(1.0f / 60.0f);

    ok = ok && nearEqual(box->position().y, fellTo, 0.001f);
    ok = ok && body->body() && body->body()->Type() == kx::BodyType::Static;

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  live_type: fell to %.1f then froze at %.1f\n", fellTo, box->position().y);
    return ok;
}

static bool testColliderDirtyUsesOwningWorld()
{
    k2d::Scene sceneA;
    k2d::GameObject* objectA =
        makeBox(sceneA, "box_a", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);
    k2d::PhysicsWorld2D worldA;
    worldA.build(sceneA.root());
    k2d::RigidBody2D* rigidBodyA = objectA->getComponent<k2d::RigidBody2D>();
    const float oldExtent = rigidBodyA->body()->Shapes()[0].polygon.vertices[0].x;

    k2d::Scene sceneB;
    k2d::PhysicsWorld2D worldB;
    worldB.build(sceneB.root());

    objectA->getComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(80.0f, 80.0f));
    worldB.step(1.0f / 60.0f);
    bool ok = nearEqual(rigidBodyA->body()->Shapes()[0].polygon.vertices[0].x, oldExtent);

    worldA.step(1.0f / 60.0f);
    ok = ok && !nearEqual(rigidBodyA->body()->Shapes()[0].polygon.vertices[0].x, oldExtent);

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  collider_world: rebuilt_in_owner=%s\n", ok ? "yes" : "no");
    return ok;
}

static bool testFiltersKeepShapesApart()
{
    k2d::Scene scene;
    k2d::GameObject* floor =
        makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);
    floor->getComponent<k2d::BoxCollider2D>()->setFilter(0x0001, 0xFFFF);

    k2d::GameObject* ghost =
        makeBox(scene, "ghost", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);
    ghost->getComponent<k2d::BoxCollider2D>()->setFilter(0x0002, 0x0004);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());

    for (int i = 0; i < 180; ++i)
        world.step(1.0f / 60.0f);

    const bool ok = ghost->position().y > 400.0f;

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  filters: ghost fell through to y=%.1f\n", ghost->position().y);
    return ok;
}

static bool testObjectAtPointFindsStatics()
{
    k2d::Scene scene;
    k2d::GameObject* ground =
        makeBox(scene, "ground", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());
    world.step(1.0f / 60.0f);

    bool ok = world.objectAtPoint(Math::Vec2(0.0f, 300.0f)) == ground;
    ok = ok && world.objectAtPoint(Math::Vec2(0.0f, -200.0f)) == nullptr;

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  point_query: static ground picked by objectAtPoint\n");
    return ok;
}

static bool testCircleCollider()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);

    k2d::GameObject* ball = scene.createObject("ball");
    ball->addComponent<k2d::RigidBody2D>()->setBodyType(kx::BodyType::Dynamic);
    ball->addComponent<k2d::CircleCollider2D>()->setRadius(20.0f);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());
    for (int i = 0; i < 240; ++i)
        world.step(1.0f / 60.0f);

    const bool ok = nearEqual(ball->position().y, 260.0f, 2.0f);

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  circle: ball rests at y=%.1f (expected ~260)\n", ball->position().y);
    return ok;
}

static bool testEdgeCollider()
{
    k2d::Scene scene;
    k2d::GameObject* ground = scene.createObject("ground");
    ground->setPosition(Math::Vec2(0.0f, 300.0f));
    ground->addComponent<k2d::RigidBody2D>()->setBodyType(kx::BodyType::Static);
    ground->addComponent<k2d::EdgeCollider2D>()->setPoints(Math::Vec2(-300.0f, 0.0f), Math::Vec2(300.0f, 0.0f));

    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());
    for (int i = 0; i < 240; ++i)
        world.step(1.0f / 60.0f);

    const bool ok = nearEqual(box->position().y, 280.0f, 2.0f);

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  edge: box rests on the edge at y=%.1f (expected ~280)\n", box->position().y);
    return ok;
}

static bool testPolygonCollider()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);

    k2d::GameObject* hex = scene.createObject("hex");
    hex->addComponent<k2d::RigidBody2D>()->setBodyType(kx::BodyType::Dynamic);
    k2d::PolygonCollider2D* shape = hex->addComponent<k2d::PolygonCollider2D>();
    shape->setRegular(6, 25.0f);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());
    for (int i = 0; i < 300; ++i)
        world.step(1.0f / 60.0f);

    bool ok = shape->points().size() == 6;
    ok = ok && hex->position().y > 230.0f && hex->position().y < 280.0f;

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  polygon: hexagon (%d points) rests at y=%.1f\n", (int)shape->points().size(), hex->position().y);
    return ok;
}

static bool testChainCollider()
{
    k2d::Scene scene;
    k2d::GameObject* ground = scene.createObject("ground");
    ground->setPosition(Math::Vec2(0.0f, 300.0f));
    ground->addComponent<k2d::RigidBody2D>()->setBodyType(kx::BodyType::Static);

    const Math::Vec2 points[4] = {Math::Vec2(-300.0f, 40.0f), Math::Vec2(-100.0f, 0.0f), Math::Vec2(100.0f, 0.0f),
                                  Math::Vec2(300.0f, 40.0f)};
    k2d::ChainCollider2D* chain = ground->addComponent<k2d::ChainCollider2D>();
    chain->setPoints(points, 4);

    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), kx::BodyType::Dynamic);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());
    for (int i = 0; i < 240; ++i)
        world.step(1.0f / 60.0f);

    bool ok = chain->points().size() == 4 && !chain->loop();
    ok = ok && nearEqual(box->position().y, 280.0f, 3.0f);

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  chain: box rests on the chain at y=%.1f (expected ~280)\n", box->position().y);
    return ok;
}

static bool testCompoundBodyKeepsShapesApart()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 400.0f), Math::Vec2(600.0f, 40.0f), kx::BodyType::Static);

    k2d::GameObject* dumbbell = scene.createObject("dumbbell");
    k2d::RigidBody2D* body = dumbbell->addComponent<k2d::RigidBody2D>();
    body->setBodyType(kx::BodyType::Dynamic);

    k2d::BoxCollider2D* bar = dumbbell->addComponent<k2d::BoxCollider2D>();
    bar->setSize(Math::Vec2(80.0f, 20.0f));

    k2d::CircleCollider2D* weight = dumbbell->addComponent<k2d::CircleCollider2D>();
    weight->setRadius(20.0f);
    weight->setOffset(Math::Vec2(60.0f, 0.0f));
    weight->setSensor(true);

    k2d::PhysicsWorld2D world;
    world.build(scene.root());

    bool ok = body->body() && body->body()->ShapeCount() == 2;
    ok = ok && bar->shapeIndex() == 0 && weight->shapeIndex() == 1;
    ok = ok && body->body() && !body->body()->IsSensor(bar->shapeIndex());
    ok = ok && body->body() && body->body()->IsSensor(weight->shapeIndex());

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  compound: shapes=%d bar=%d(solid) weight=%d(sensor)\n",
                body->body() ? body->body()->ShapeCount() : -1, bar->shapeIndex(), weight->shapeIndex());
    return ok;
}

static bool testSerializerRoundTrip()
{
    k2d::RegisterPhysics2DSerializers();

    k2d::Scene source;
    k2d::GameObject* object = source.createObject("crate");
    object->setPosition(Math::Vec2(30.0f, -40.0f));

    k2d::RigidBody2D* body = object->addComponent<k2d::RigidBody2D>();
    body->setBodyType(kx::BodyType::Kinematic);
    body->setDensity(2.5f);
    body->setFriction(0.8f);
    body->setRestitution(0.4f);
    body->setLinearDamping(0.2f);
    body->setAngularDamping(0.3f);
    body->setGravityScale(0.5f);
    body->setFixedRotation(true);
    body->setBullet(true);

    k2d::CharacterBody2D* character = object->addComponent<k2d::CharacterBody2D>();
    character->setVelocity(Math::Vec2(123.0f, -456.0f));
    character->setSafeMargin(1.5f);
    character->setMaxSlides(6);
    character->setMotionMode(k2d::CharacterBody2D::MotionMode::Grounded);
    character->setUpDirection(Math::Vec2(0.0f, -1.0f));
    character->setFloorMaxAngleDegrees(30.0f);

    k2d::BoxCollider2D* box = object->addComponent<k2d::BoxCollider2D>();
    box->setSize(Math::Vec2(64.0f, 24.0f));
    box->setOffset(Math::Vec2(5.0f, -3.0f));
    box->setFilter(0x0004, 0x0008);

    k2d::CircleCollider2D* circle = object->addComponent<k2d::CircleCollider2D>();
    circle->setRadius(12.5f);
    circle->setSensor(true);

    k2d::EdgeCollider2D* edge = object->addComponent<k2d::EdgeCollider2D>();
    edge->setPoints(Math::Vec2(-10.0f, 1.0f), Math::Vec2(10.0f, 2.0f));

    k2d::PolygonCollider2D* polygon = object->addComponent<k2d::PolygonCollider2D>();
    polygon->setRegular(5, 20.0f);

    const Math::Vec2 chainPoints[3] = {Math::Vec2(-40.0f, 0.0f), Math::Vec2(0.0f, 10.0f), Math::Vec2(40.0f, 0.0f)};
    k2d::ChainCollider2D* chain = object->addComponent<k2d::ChainCollider2D>();
    chain->setPoints(chainPoints, 3);
    chain->setLoop(true);

    const ct::Json json = k2d::Serializer::WriteObject(*object);

    k2d::Scene target;
    k2d::GameObject* loaded = k2d::Serializer::ReadObject(target, json);
    bool ok = loaded != nullptr;
    if (!loaded)
        return false;

    k2d::RigidBody2D* outBody = loaded->getComponent<k2d::RigidBody2D>();
    ok = ok && outBody && outBody->bodyType() == kx::BodyType::Kinematic;
    ok = ok && outBody && nearEqual(outBody->density(), 2.5f, 0.001f);
    ok = ok && outBody && nearEqual(outBody->friction(), 0.8f, 0.001f);
    ok = ok && outBody && nearEqual(outBody->restitution(), 0.4f, 0.001f);
    ok = ok && outBody && nearEqual(outBody->linearDamping(), 0.2f, 0.001f);
    ok = ok && outBody && nearEqual(outBody->angularDamping(), 0.3f, 0.001f);
    ok = ok && outBody && nearEqual(outBody->gravityScale(), 0.5f, 0.001f);
    ok = ok && outBody && outBody->fixedRotation() && outBody->bullet();

    k2d::CharacterBody2D* outCharacter = loaded->getComponent<k2d::CharacterBody2D>();
    ok = ok && outCharacter && nearEqual(outCharacter->velocity().x, 123.0f, 0.001f) &&
         nearEqual(outCharacter->velocity().y, -456.0f, 0.001f) &&
         nearEqual(outCharacter->safeMargin(), 1.5f, 0.001f) && outCharacter->maxSlides() == 6 &&
         outCharacter->motionMode() == k2d::CharacterBody2D::MotionMode::Grounded &&
         nearEqual(outCharacter->floorMaxAngleDegrees(), 30.0f, 0.001f);

    ok = ok && loaded->componentCount<k2d::Collider2D>() == 5;

    k2d::BoxCollider2D* outBox = loaded->getComponent<k2d::BoxCollider2D>();
    ok = ok && outBox && nearEqual(outBox->size().x, 64.0f, 0.001f) && nearEqual(outBox->size().y, 24.0f, 0.001f);
    ok = ok && outBox && nearEqual(outBox->offset().x, 5.0f, 0.001f);
    ok = ok && outBox && outBox->category() == 0x0004 && outBox->mask() == 0x0008;

    k2d::CircleCollider2D* outCircle = loaded->getComponent<k2d::CircleCollider2D>();
    ok = ok && outCircle && nearEqual(outCircle->radius(), 12.5f, 0.001f) && outCircle->isSensor();

    k2d::EdgeCollider2D* outEdge = loaded->getComponent<k2d::EdgeCollider2D>();
    ok = ok && outEdge && nearEqual(outEdge->start().x, -10.0f, 0.001f) && nearEqual(outEdge->end().y, 2.0f, 0.001f);

    k2d::PolygonCollider2D* outPolygon = loaded->getComponent<k2d::PolygonCollider2D>();
    ok = ok && outPolygon && outPolygon->points().size() == 5;

    k2d::ChainCollider2D* outChain = loaded->getComponent<k2d::ChainCollider2D>();
    ok = ok && outChain && outChain->points().size() == 3 && outChain->loop();

    k2d::PhysicsWorld2D world;
    world.build(target.root());
    ok = ok && world.bodyCount() == 1;
    ok = ok && outBody && outBody->body() && outBody->body()->ShapeCount() == 7;
    ok = ok && outChain && outChain->shapeCount() == 3;

    k2d::PhysicsWorld2D::SetActive(nullptr);
    std::printf("  serializer: colliders=%d shapes=%d character=%s (chain owns %d)\n",
                (int)loaded->componentCount<k2d::Collider2D>(),
                outBody && outBody->body() ? outBody->body()->ShapeCount() : -1, outCharacter ? "yes" : "no",
                outChain ? outChain->shapeCount() : -1);
    return ok;
}

int main()
{
    const bool falls = testBoxFallsAndRests();
    const bool tileMap = testPaintedTileMapCollision();
    const bool contacts = testContactCallbackFires();
    const bool sensor = testSensorReportsWithoutBlocking();
    const bool queries = testRaycastAndQueries();
    const bool character = testCharacterBodyMotion();
    const bool staticFollow = testStaticBodyFollowsItsTransform();
    const bool velocity = testImpulseAndVelocity();
    const bool deterministic = testFixedStepIsDeterministic();
    const bool destroy = testDestroyingAnObjectRemovesItsBody();
    const bool lateSpawn = testObjectSpawnedDuringPlayGetsABody();
    const bool colliderChange = testColliderChangeRebuildsTheBody();
    const bool liveType = testBodyTypeAndDensityApplyLive();
    const bool colliderWorld = testColliderDirtyUsesOwningWorld();
    const bool filters = testFiltersKeepShapesApart();
    const bool pointQuery = testObjectAtPointFindsStatics();
    const bool circle = testCircleCollider();
    const bool edge = testEdgeCollider();
    const bool polygon = testPolygonCollider();
    const bool chain = testChainCollider();
    const bool compound = testCompoundBodyKeepsShapesApart();
    const bool serialized = testSerializerRoundTrip();

    std::printf("physics2d: falls=%s tilemap=%s contacts=%s sensor=%s queries=%s character=%s static_follow=%s "
                "velocity=%s determinism=%s destroy=%s late_spawn=%s collider_change=%s "
                "live_type=%s collider_world=%s filters=%s point_query=%s circle=%s edge=%s polygon=%s "
                "chain=%s compound=%s serializer=%s\n",
                falls ? "pass" : "fail", tileMap ? "pass" : "fail", contacts ? "pass" : "fail",
                sensor ? "pass" : "fail", queries ? "pass" : "fail", character ? "pass" : "fail",
                staticFollow ? "pass" : "fail", velocity ? "pass" : "fail", deterministic ? "pass" : "fail",
                destroy ? "pass" : "fail", lateSpawn ? "pass" : "fail", colliderChange ? "pass" : "fail",
                liveType ? "pass" : "fail", colliderWorld ? "pass" : "fail", filters ? "pass" : "fail",
                pointQuery ? "pass" : "fail",
                circle ? "pass" : "fail", edge ? "pass" : "fail", polygon ? "pass" : "fail", chain ? "pass" : "fail",
                compound ? "pass" : "fail", serialized ? "pass" : "fail");
    return falls && tileMap && contacts && sensor && queries && character && staticFollow && velocity &&
                   deterministic && destroy && lateSpawn && colliderChange && liveType && colliderWorld && filters && pointQuery &&
                   circle && edge && polygon && chain && compound && serialized
               ? 0
               : 1;
}
