#include <k2d/Arrive2D.h>
#include <k2d/BoxCollider2D.h>
#include <k2d/ChainCollider2D.h>
#include <k2d/CharacterBody2D.h>
#include <k2d/CircleCollider2D.h>
#include <k2d/Collider2D.h>
#include <k2d/DistanceJoint2D.h>
#include <k2d/EdgeCollider2D.h>
#include <k2d/Flee2D.h>
#include <k2d/GearJoint2D.h>
#include <k2d/MaskContour2D.h>
#include <k2d/MotorJoint2D.h>
#include <k2d/MouseJoint2D.h>
#include <k2d/ObstacleAvoidance2D.h>
#include <k2d/PolygonCollider2D.h>
#include <k2d/RevoluteJoint2D.h>
#include <k2d/WheelJoint2D.h>
#include <k2d/GameObject.h>
#include <k2d/Physics2DSerializer.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>
#include <k2d/TileMapComponent.h>
#include <k2d/Seek2D.h>
#include <k2d/Separation2D.h>
#include <k2d/Steering2D.h>
#include <k2d/TileMapCollider2D.h>
#include <k2d/Wander2D.h>

#include <cmath>
#include <cstdio>

static bool nearEqual(float a, float b, float tolerance = 1.0f)
{
    return std::fabs(a - b) < tolerance;
}

static k2d::GameObject* makeBox(k2d::Scene& scene, const char* name, const Math::Vec2& position, const Math::Vec2& size,
                                k2d::BodyType type)
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
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);
    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    scene.setSimulationEnabled(true);

    bool ok = scene.bodyCount() == 2;

    for (int i = 0; i < 180; ++i)
        scene.update(1.0f / 60.0f);

    const float expectedRest = 300.0f - 20.0f - 20.0f;
    ok = ok && nearEqual(box->position().y, expectedRest, 2.0f);
    ok = ok && nearEqual(box->position().x, 0.0f, 1.0f);

    std::printf("  falls: bodies=%d y=%.1f (expected ~%.1f)\n", (int)scene.bodyCount(), box->position().y,
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
    mapObject->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    mapObject->addComponent<k2d::TileMapCollider2D>();

    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(8.0f, 0.0f), Math::Vec2(16.0f, 16.0f), k2d::BodyType::Dynamic);
    scene.setSimulationEnabled(true);
    for (int i = 0; i < 180; ++i)
        scene.update(1.0f / 60.0f);

    const bool ok = scene.bodyCount() == 2 && nearEqual(box->position().y, 192.0f, 2.0f) &&
                    scene.objectAtPoint(Math::Vec2(8.0f, 208.0f)) == mapObject;
    std::printf("  tilemap: bodies=%d y=%.1f\n", (int)scene.bodyCount(), box->position().y);
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
        makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);
    makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    ContactLog log;
    scene.setCollisionCallback(&onContact, &log);
    scene.setSimulationEnabled(true);

    for (int i = 0; i < 180; ++i)
        scene.update(1.0f / 60.0f);

    const bool ok = log.begins > 0 && log.lastOther != nullptr &&
                    (log.lastOther == floor || log.lastOther->name() == ct::String("box"));

    std::printf("  contacts: begins=%d ends=%d contacts_now=%d\n", log.begins, log.ends, (int)scene.contactCount());
    return ok;
}

static bool testSensorReportsWithoutBlocking()
{
    k2d::Scene scene;
    k2d::GameObject* trigger = scene.createObject("trigger");
    trigger->setPosition(Math::Vec2(0.0f, 150.0f));
    k2d::RigidBody2D* triggerBody = trigger->addComponent<k2d::RigidBody2D>();
    triggerBody->setBodyType(k2d::BodyType::Static);
    k2d::BoxCollider2D* triggerShape = trigger->addComponent<k2d::BoxCollider2D>();
    triggerShape->setSize(Math::Vec2(400.0f, 20.0f));
    triggerShape->setSensor(true);

    makeBox(scene, "floor", Math::Vec2(0.0f, 400.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);
    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    ContactLog log;
    scene.setCollisionCallback(&onContact, &log);
    scene.setSimulationEnabled(true);

    bool ok = triggerShape->shapeIndex() == 0 && triggerBody->inWorld() &&
              triggerBody->IsSensor(triggerShape->shapeIndex());

    for (int i = 0; i < 240; ++i)
        scene.update(1.0f / 60.0f);

    ok = ok && log.sensors > 0;
    ok = ok && box->position().y > 300.0f;

    std::printf("  sensor: sensor_events=%d box_y=%.1f (passed through)\n", log.sensors, box->position().y);
    return ok;
}

static bool testRaycastAndQueries()
{
    k2d::Scene scene;
    k2d::GameObject* wall =
        makeBox(scene, "wall", Math::Vec2(200.0f, 0.0f), Math::Vec2(40.0f, 200.0f), k2d::BodyType::Static);

    scene.setGravity(Math::Vec2(0.0f, 0.0f));
    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    Math::Vec2 point(0.0f, 0.0f);
    Math::Vec2 normal(0.0f, 0.0f);
    k2d::GameObject* hit = scene.raycast(Math::Vec2(0.0f, 0.0f), Math::Vec2(1.0f, 0.0f), 500.0f, &point, &normal);

    bool ok = hit == wall;
    ok = ok && nearEqual(point.x, 180.0f, 2.0f);

    ct::Vector<k2d::GameObject*> found;
    scene.overlapCircle(Math::Vec2(200.0f, 0.0f), 60.0f, found);
    ok = ok && found.size() == 1 && found[0] == wall;

    std::printf("  queries: ray_hit=%s point=(%.1f, %.1f) overlap=%d\n", hit ? hit->name().c_str() : "none", point.x,
                point.y, (int)found.size());
    return ok;
}

static bool testCharacterBodyMotion()
{
    k2d::Scene scene;
    k2d::GameObject* wall =
        makeBox(scene, "wall", Math::Vec2(0.0f, 0.0f), Math::Vec2(20.0f, 160.0f), k2d::BodyType::Static);
    k2d::GameObject* player =
        makeBox(scene, "player", Math::Vec2(-80.0f, 0.0f), Math::Vec2(20.0f, 20.0f), k2d::BodyType::Kinematic);
    k2d::CharacterBody2D* character = player->addComponent<k2d::CharacterBody2D>();

    scene.setGravity(Math::Vec2(0.0f, 0.0f));
    scene.setSimulationEnabled(true);

    const bool freeBefore = character->placeFree(-80.0f, 0.0f);
    const bool occupiedWall = !character->placeFree(0.0f, 0.0f) && character->placeMeeting(0.0f, 0.0f) == wall;
    const k2d::CollisionInfo hit = character->moveAndCollide(Math::Vec2(160.0f, 0.0f));
    bool ok = freeBefore && occupiedWall && hit.hit && hit.other == wall && hit.normal.x < -0.99f &&
              nearEqual(player->position().x, -20.5f, 2.0f);

    player->setPosition(Math::Vec2(-80.0f, -40.0f));
    player->getComponent<k2d::RigidBody2D>()->SetPosition(player->globalPosition());
    character->setVelocity(Math::Vec2(4800.0f, 2400.0f));
    const bool slid = character->moveAndSlide();
    ok = ok && slid && character->isOnWall() && character->velocity().x < 1.0f && character->velocity().y > 2000.0f;

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
        makeBox(scene, "platform", Math::Vec2(0.0f, 300.0f), Math::Vec2(200.0f, 20.0f), k2d::BodyType::Static);

    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    ct::Vector<k2d::GameObject*> found;
    scene.overlapCircle(Math::Vec2(0.0f, 300.0f), 30.0f, found);
    bool ok = found.size() == 1 && found[0] == platform;

    platform->setPosition(Math::Vec2(400.0f, 300.0f));
    scene.update(1.0f / 60.0f);

    scene.overlapCircle(Math::Vec2(400.0f, 300.0f), 30.0f, found);
    ok = ok && found.size() == 1 && found[0] == platform;
    scene.overlapCircle(Math::Vec2(0.0f, 300.0f), 30.0f, found);
    ok = ok && found.empty();

    std::printf("  static_follow: platform tracked after moving its transform\n");
    return ok;
}

static bool testImpulseAndVelocity()
{
    k2d::Scene scene;
    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);
    k2d::RigidBody2D* body = box->getComponent<k2d::RigidBody2D>();
    body->setGravityScale(0.0f);
    body->setVelocity(Math::Vec2(100.0f, 0.0f));

    scene.setGravity(Math::Vec2(0.0f, 0.0f));
    scene.setSimulationEnabled(true);

    for (int i = 0; i < 60; ++i)
        scene.update(1.0f / 60.0f);

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
        makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);
        k2d::GameObject* box =
            makeBox(scene, "box", Math::Vec2(13.0f, -200.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);
        box->setRotationDegrees(20.0f);

        scene.setSimulationEnabled(true);
        for (int i = 0; i < 300; ++i)
            scene.update(1.0f / 60.0f);
        results[run] = box->position().y;
    }

    const bool ok = nearEqual(results[0], results[1], 0.0001f);
    std::printf("  determinism: run_a=%.4f run_b=%.4f\n", results[0], results[1]);
    return ok;
}

static bool testDestroyingAnObjectRemovesItsBody()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);
    k2d::GameObject* doomed =
        makeBox(scene, "doomed", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);
    k2d::GameObject* keeper =
        makeBox(scene, "keeper", Math::Vec2(200.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    scene.setSimulationEnabled(true);
    bool ok = scene.bodyCount() == 3;

    for (int i = 0; i < 30; ++i)
        scene.update(1.0f / 60.0f);

    scene.destroy(doomed);
    ok = ok && scene.bodyCount() == 3;
    scene.update(1.0f / 60.0f);
    ok = ok && scene.bodyCount() == 2;

    for (int i = 0; i < 150; ++i)
        scene.update(1.0f / 60.0f);

    ok = ok && nearEqual(keeper->position().y, 260.0f, 2.0f);

    std::printf("  destroy: bodies=%d keeper_y=%.1f (no dangling body)\n", (int)scene.bodyCount(),
                keeper->position().y);
    return ok;
}

static bool testObjectSpawnedDuringPlayGetsABody()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);

    scene.setSimulationEnabled(true);
    bool ok = scene.bodyCount() == 1;

    scene.update(1.0f / 60.0f);

    k2d::GameObject* late =
        makeBox(scene, "late", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    for (int i = 0; i < 180; ++i)
        scene.update(1.0f / 60.0f);

    ok = ok && scene.bodyCount() == 2;
    ok = ok && nearEqual(late->position().y, 260.0f, 2.0f);

    std::printf("  late_spawn: bodies=%d y=%.1f (fell and landed)\n", (int)scene.bodyCount(), late->position().y);
    return ok;
}

static bool testColliderChangeRebuildsTheBody()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);
    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    scene.setSimulationEnabled(true);

    for (int i = 0; i < 180; ++i)
        scene.update(1.0f / 60.0f);

    bool ok = nearEqual(box->position().y, 260.0f, 2.0f);

    box->getComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(40.0f, 120.0f));
    for (int i = 0; i < 120; ++i)
        scene.update(1.0f / 60.0f);

    ok = ok && nearEqual(box->position().y, 220.0f, 3.0f);
    ok = ok && scene.bodyCount() == 2;

    std::printf("  collider_change: taller box now rests at y=%.1f (was 260)\n", box->position().y);
    return ok;
}

static bool testBodyTypeAndDensityApplyLive()
{
    k2d::Scene scene;
    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);
    k2d::RigidBody2D* body = box->getComponent<k2d::RigidBody2D>();

    scene.setSimulationEnabled(true);

    for (int i = 0; i < 60; ++i)
        scene.update(1.0f / 60.0f);

    const float fellTo = box->position().y;
    bool ok = fellTo > 100.0f;

    body->setBodyType(k2d::BodyType::Static);
    for (int i = 0; i < 60; ++i)
        scene.update(1.0f / 60.0f);

    ok = ok && nearEqual(box->position().y, fellTo, 0.001f);
    ok = ok && body->inWorld() && body->bodyType() == k2d::BodyType::Static;

    std::printf("  live_type: fell to %.1f then froze at %.1f\n", fellTo, box->position().y);
    return ok;
}

static bool testColliderDirtyUsesOwningWorld()
{
    k2d::Scene sceneA;
    k2d::GameObject* objectA =
        makeBox(sceneA, "box_a", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);
    sceneA.setSimulationEnabled(true);
    k2d::RigidBody2D* rigidBodyA = objectA->getComponent<k2d::RigidBody2D>();
    const float oldExtent = rigidBodyA->Shapes()[0].polygon.vertices[0].x;

    k2d::Scene sceneB;
    sceneB.setSimulationEnabled(true);

    objectA->getComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(80.0f, 80.0f));
    sceneB.update(1.0f / 60.0f);
    bool ok = nearEqual(rigidBodyA->Shapes()[0].polygon.vertices[0].x, oldExtent);

    sceneA.update(1.0f / 60.0f);
    ok = ok && !nearEqual(rigidBodyA->Shapes()[0].polygon.vertices[0].x, oldExtent);

    std::printf("  collider_world: rebuilt_in_owner=%s\n", ok ? "yes" : "no");
    return ok;
}

static bool testFiltersKeepShapesApart()
{
    k2d::Scene scene;
    k2d::GameObject* floor =
        makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);
    floor->getComponent<k2d::BoxCollider2D>()->setFilter(0x0001, 0xFFFF);

    k2d::GameObject* ghost =
        makeBox(scene, "ghost", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);
    ghost->getComponent<k2d::BoxCollider2D>()->setFilter(0x0002, 0x0004);

    scene.setSimulationEnabled(true);

    for (int i = 0; i < 180; ++i)
        scene.update(1.0f / 60.0f);

    const bool ok = ghost->position().y > 400.0f;

    std::printf("  filters: ghost fell through to y=%.1f\n", ghost->position().y);
    return ok;
}

static bool testObjectAtPointFindsStatics()
{
    k2d::Scene scene;
    k2d::GameObject* ground =
        makeBox(scene, "ground", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);

    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    bool ok = scene.objectAtPoint(Math::Vec2(0.0f, 300.0f)) == ground;
    ok = ok && scene.objectAtPoint(Math::Vec2(0.0f, -200.0f)) == nullptr;

    std::printf("  point_query: static ground picked by objectAtPoint\n");
    return ok;
}

static bool testCircleCollider()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);

    k2d::GameObject* ball = scene.createObject("ball");
    ball->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Dynamic);
    ball->addComponent<k2d::CircleCollider2D>()->setRadius(20.0f);

    scene.setSimulationEnabled(true);
    for (int i = 0; i < 240; ++i)
        scene.update(1.0f / 60.0f);

    const bool ok = nearEqual(ball->position().y, 260.0f, 2.0f);

    std::printf("  circle: ball rests at y=%.1f (expected ~260)\n", ball->position().y);
    return ok;
}

static bool testEdgeCollider()
{
    k2d::Scene scene;
    k2d::GameObject* ground = scene.createObject("ground");
    ground->setPosition(Math::Vec2(0.0f, 300.0f));
    ground->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    ground->addComponent<k2d::EdgeCollider2D>()->setPoints(Math::Vec2(-300.0f, 0.0f), Math::Vec2(300.0f, 0.0f));

    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    scene.setSimulationEnabled(true);
    for (int i = 0; i < 240; ++i)
        scene.update(1.0f / 60.0f);

    const bool ok = nearEqual(box->position().y, 280.0f, 2.0f);

    std::printf("  edge: box rests on the edge at y=%.1f (expected ~280)\n", box->position().y);
    return ok;
}

static bool testPolygonCollider()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);

    k2d::GameObject* hex = scene.createObject("hex");
    hex->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Dynamic);
    k2d::PolygonCollider2D* shape = hex->addComponent<k2d::PolygonCollider2D>();
    shape->setRegular(6, 25.0f);

    scene.setSimulationEnabled(true);
    for (int i = 0; i < 300; ++i)
        scene.update(1.0f / 60.0f);

    bool ok = shape->points().size() == 6;
    ok = ok && hex->position().y > 230.0f && hex->position().y < 280.0f;

    std::printf("  polygon: hexagon (%d points) rests at y=%.1f\n", (int)shape->points().size(), hex->position().y);
    return ok;
}

static bool testChainCollider()
{
    k2d::Scene scene;
    k2d::GameObject* ground = scene.createObject("ground");
    ground->setPosition(Math::Vec2(0.0f, 300.0f));
    ground->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);

    const Math::Vec2 points[4] = {Math::Vec2(-300.0f, 40.0f), Math::Vec2(-100.0f, 0.0f), Math::Vec2(100.0f, 0.0f),
                                  Math::Vec2(300.0f, 40.0f)};
    k2d::ChainCollider2D* chain = ground->addComponent<k2d::ChainCollider2D>();
    chain->setPoints(points, 4);

    k2d::GameObject* box =
        makeBox(scene, "box", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    scene.setSimulationEnabled(true);
    for (int i = 0; i < 240; ++i)
        scene.update(1.0f / 60.0f);

    bool ok = chain->points().size() == 4 && !chain->loop();
    ok = ok && nearEqual(box->position().y, 280.0f, 3.0f);

    std::printf("  chain: box rests on the chain at y=%.1f (expected ~280)\n", box->position().y);
    return ok;
}

static bool testCompoundBodyKeepsShapesApart()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 400.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);

    k2d::GameObject* dumbbell = scene.createObject("dumbbell");
    k2d::RigidBody2D* body = dumbbell->addComponent<k2d::RigidBody2D>();
    body->setBodyType(k2d::BodyType::Dynamic);

    k2d::BoxCollider2D* bar = dumbbell->addComponent<k2d::BoxCollider2D>();
    bar->setSize(Math::Vec2(80.0f, 20.0f));

    k2d::CircleCollider2D* weight = dumbbell->addComponent<k2d::CircleCollider2D>();
    weight->setRadius(20.0f);
    weight->setOffset(Math::Vec2(60.0f, 0.0f));
    weight->setSensor(true);

    scene.setSimulationEnabled(true);

    bool ok = body->inWorld() && body->ShapeCount() == 2;
    ok = ok && bar->shapeIndex() == 0 && weight->shapeIndex() == 1;
    ok = ok && body->inWorld() && !body->IsSensor(bar->shapeIndex());
    ok = ok && body->inWorld() && body->IsSensor(weight->shapeIndex());

    std::printf("  compound: shapes=%d bar=%d(solid) weight=%d(sensor)\n",
                body->inWorld() ? body->ShapeCount() : -1, bar->shapeIndex(), weight->shapeIndex());
    return ok;
}

static bool testDistanceJointKeepsDistance()
{
    k2d::Scene scene;
    k2d::GameObject* anchorBox = scene.createObject("anchorBox");
    anchorBox->setPosition(Math::Vec2(-50.0f, 0.0f));
    anchorBox->addComponent<k2d::RigidBody2D>();
    anchorBox->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(20.0f, 20.0f));

    k2d::GameObject* hangingBox = scene.createObject("hangingBox");
    hangingBox->setPosition(Math::Vec2(50.0f, 0.0f));
    hangingBox->addComponent<k2d::RigidBody2D>();
    hangingBox->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(20.0f, 20.0f));

    k2d::DistanceJoint2D* joint = anchorBox->addComponent<k2d::DistanceJoint2D>();
    joint->setTargetName("hangingBox");
    joint->setLength(100.0f);

    scene.setGravity(Math::Vec2(0.0f, 980.0f));
    scene.setSimulationEnabled(true);

    for (int i = 0; i < 120; ++i)
        scene.update(1.0f / 60.0f);

    const float dx = hangingBox->position().x - anchorBox->position().x;
    const float dy = hangingBox->position().y - anchorBox->position().y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const bool ok = nearEqual(distance, 100.0f, 3.0f);

    std::printf("  distance_joint: distance=%.2f (expected ~100.0)\n", distance);
    return ok;
}

static bool testRevoluteJointMotorRotates()
{
    k2d::Scene scene;
    scene.setGravity(Math::Vec2(0.0f, 0.0f));

    k2d::GameObject* hub = scene.createObject("hub");
    hub->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    hub->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(10.0f, 10.0f));

    k2d::GameObject* arm = scene.createObject("arm");
    arm->setPosition(Math::Vec2(50.0f, 0.0f));
    arm->addComponent<k2d::RigidBody2D>();
    arm->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(80.0f, 10.0f));

    k2d::RevoluteJoint2D* joint = hub->addComponent<k2d::RevoluteJoint2D>();
    joint->setTargetName("arm");
    joint->setMotor(true, 4.0f, 1.0e6f);

    scene.setSimulationEnabled(true);
    for (int i = 0; i < 60; ++i)
        scene.update(1.0f / 60.0f);

    const bool ok = std::fabs(arm->rotationDegrees()) > 1.0f;
    std::printf("  revolute_joint: arm_rotation=%.2f deg\n", arm->rotationDegrees());
    return ok;
}

static bool testJointSerializerRoundTrip()
{
    k2d::RegisterPhysics2DSerializers();

    k2d::Scene source;
    k2d::GameObject* anchor = source.createObject("anchor");
    anchor->addComponent<k2d::RigidBody2D>();

    k2d::DistanceJoint2D* distance = anchor->addComponent<k2d::DistanceJoint2D>();
    distance->setTargetName("other");
    distance->setCollideConnected(true);
    distance->setLocalAnchorA(Math::Vec2(1.0f, 2.0f));
    distance->setLocalAnchorB(Math::Vec2(-1.0f, -2.0f));
    distance->setLength(120.0f);
    distance->setLengthRange(80.0f, 160.0f);
    distance->setSpring(3.0f, 0.4f);

    k2d::RevoluteJoint2D* revolute = anchor->addComponent<k2d::RevoluteJoint2D>();
    revolute->setTargetName("other");
    revolute->setLocalAnchorA(Math::Vec2(2.0f, 0.0f));
    revolute->setLocalAnchorB(Math::Vec2(-2.0f, 0.0f));
    revolute->setReferenceAngle(0.3f);
    revolute->setMotor(true, 1.5f, 500.0f);
    revolute->setLimits(true, -0.5f, 0.5f);

    k2d::WheelJoint2D* wheel = anchor->addComponent<k2d::WheelJoint2D>();
    wheel->setTargetName("other");
    wheel->setLocalAnchorA(Math::Vec2(0.5f, 0.5f));
    wheel->setLocalAnchorB(Math::Vec2(-0.5f, -0.5f));
    wheel->setLocalAxisA(Math::Vec2(0.0f, 1.0f));
    wheel->setMotor(true, 2.0f, 300.0f);
    wheel->setSpring(5.0f, 0.6f);

    k2d::MotorJoint2D* motor = anchor->addComponent<k2d::MotorJoint2D>();
    motor->setTargetName("other");
    motor->setLinearOffset(Math::Vec2(3.0f, 4.0f));
    motor->setAngularOffset(0.2f);
    motor->setMaxForce(200.0f);
    motor->setMaxTorque(150.0f);
    motor->setCorrectionFactor(0.5f);

    k2d::MouseJoint2D* mouse = anchor->addComponent<k2d::MouseJoint2D>();
    mouse->setCollideConnected(true);
    mouse->setTarget(Math::Vec2(10.0f, -10.0f));
    mouse->setMaxForce(999.0f);
    mouse->setSpring(6.0f, 0.8f);

    k2d::GearJoint2D* gear = anchor->addComponent<k2d::GearJoint2D>();
    gear->setJointA("anchor", 0);
    gear->setJointB("anchor", 1);
    gear->setRatio(2.5f);

    const ct::Json json = k2d::Serializer::WriteObject(*anchor);

    k2d::Scene target;
    k2d::GameObject* loaded = k2d::Serializer::ReadObject(target, json);
    bool ok = loaded != nullptr;
    if (!loaded)
        return false;

    k2d::DistanceJoint2D* outDistance = loaded->getComponent<k2d::DistanceJoint2D>();
    ok = ok && outDistance && outDistance->targetName() == ct::String("other") && outDistance->collideConnected();
    ok = ok && outDistance && nearEqual(outDistance->localAnchorA().x, 1.0f, 0.001f) &&
         nearEqual(outDistance->localAnchorA().y, 2.0f, 0.001f);
    ok = ok && outDistance && nearEqual(outDistance->localAnchorB().x, -1.0f, 0.001f);
    ok = ok && outDistance && nearEqual(outDistance->length(), 120.0f, 0.001f);
    ok = ok && outDistance && nearEqual(outDistance->minLength(), 80.0f, 0.001f) &&
         nearEqual(outDistance->maxLength(), 160.0f, 0.001f);
    ok = ok && outDistance && nearEqual(outDistance->springFrequency(), 3.0f, 0.001f) &&
         nearEqual(outDistance->springDamping(), 0.4f, 0.001f);

    k2d::RevoluteJoint2D* outRevolute = loaded->getComponent<k2d::RevoluteJoint2D>();
    ok = ok && outRevolute && nearEqual(outRevolute->referenceAngle(), 0.3f, 0.001f);
    ok = ok && outRevolute && outRevolute->motorEnabled() && nearEqual(outRevolute->motorSpeed(), 1.5f, 0.001f) &&
         nearEqual(outRevolute->maxMotorTorque(), 500.0f, 0.001f);
    ok = ok && outRevolute && outRevolute->limitEnabled() && nearEqual(outRevolute->lowerAngle(), -0.5f, 0.001f) &&
         nearEqual(outRevolute->upperAngle(), 0.5f, 0.001f);

    k2d::WheelJoint2D* outWheel = loaded->getComponent<k2d::WheelJoint2D>();
    ok = ok && outWheel && nearEqual(outWheel->localAxisA().y, 1.0f, 0.001f);
    ok = ok && outWheel && outWheel->motorEnabled() && nearEqual(outWheel->motorSpeed(), 2.0f, 0.001f) &&
         nearEqual(outWheel->maxMotorTorque(), 300.0f, 0.001f);
    ok = ok && outWheel && nearEqual(outWheel->springFrequency(), 5.0f, 0.001f) &&
         nearEqual(outWheel->springDamping(), 0.6f, 0.001f);

    k2d::MotorJoint2D* outMotor = loaded->getComponent<k2d::MotorJoint2D>();
    ok = ok && outMotor && nearEqual(outMotor->linearOffset().x, 3.0f, 0.001f) &&
         nearEqual(outMotor->linearOffset().y, 4.0f, 0.001f);
    ok = ok && outMotor && nearEqual(outMotor->angularOffset(), 0.2f, 0.001f);
    ok = ok && outMotor && nearEqual(outMotor->maxForce(), 200.0f, 0.001f) &&
         nearEqual(outMotor->maxTorque(), 150.0f, 0.001f);
    ok = ok && outMotor && nearEqual(outMotor->correctionFactor(), 0.5f, 0.001f);

    k2d::MouseJoint2D* outMouse = loaded->getComponent<k2d::MouseJoint2D>();
    ok = ok && outMouse && outMouse->collideConnected();
    ok = ok && outMouse && nearEqual(outMouse->target().x, 10.0f, 0.001f) && nearEqual(outMouse->target().y, -10.0f, 0.001f);
    ok = ok && outMouse && nearEqual(outMouse->maxForce(), 999.0f, 0.001f);
    ok = ok && outMouse && nearEqual(outMouse->springFrequency(), 6.0f, 0.001f) &&
         nearEqual(outMouse->springDamping(), 0.8f, 0.001f);

    k2d::GearJoint2D* outGear = loaded->getComponent<k2d::GearJoint2D>();
    ok = ok && outGear && outGear->jointATargetName() == ct::String("anchor") && outGear->jointAIndex() == 0;
    ok = ok && outGear && outGear->jointBTargetName() == ct::String("anchor") && outGear->jointBIndex() == 1;
    ok = ok && outGear && nearEqual(outGear->ratio(), 2.5f, 0.001f);

    std::printf("  joint_serializer: distance=%s revolute=%s wheel=%s motor=%s mouse=%s gear=%s\n",
                outDistance ? "ok" : "missing", outRevolute ? "ok" : "missing", outWheel ? "ok" : "missing",
                outMotor ? "ok" : "missing", outMouse ? "ok" : "missing", outGear ? "ok" : "missing");
    return ok;
}

// The editor authoring flow: point + platform wired by name, serialized and
// reloaded like startPlay does, then simulated.
static bool testSceneJointAuthoringFlow()
{
    k2d::RegisterPhysics2DSerializers();

    k2d::Scene source;
    k2d::GameObject* point = source.createObject("point");
    point->setPosition(Math::Vec2(0.0f, 0.0f));
    point->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    point->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(10.0f, 10.0f));

    k2d::GameObject* platform = source.createObject("platform");
    platform->setPosition(Math::Vec2(0.0f, 150.0f));
    platform->addComponent<k2d::RigidBody2D>();
    platform->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(80.0f, 20.0f));

    k2d::DistanceJoint2D* joint = point->addComponent<k2d::DistanceJoint2D>();
    joint->setTargetName("platform");

    const ct::Json rootJson = k2d::Serializer::WriteObject(source.root());

    k2d::Scene target;
    const ct::Json& children = rootJson["children"];
    bool ok = children.is_array();
    if (ok)
        for (size_t i = 0; i < children.size(); ++i)
            k2d::Serializer::ReadObject(target, children[i], &target.root());

    k2d::GameObject* loadedPoint = target.find("point");
    k2d::GameObject* loadedPlatform = target.find("platform");
    ok = ok && loadedPoint && loadedPlatform;

    k2d::DistanceJoint2D* loadedJoint = ok ? loadedPoint->getComponent<k2d::DistanceJoint2D>() : nullptr;
    ok = ok && loadedJoint && loadedJoint->targetName() == ct::String("platform");
    ok = ok && loadedJoint && !loadedJoint->isConnected();

    target.setGravity(Math::Vec2(0.0f, 980.0f));
    target.setSimulationEnabled(true);

    for (int i = 0; i < 120; ++i)
        target.update(1.0f / 60.0f);

    float distance = -1.0f;
    if (ok)
    {
        const float dx = loadedPlatform->position().x - loadedPoint->position().x;
        const float dy = loadedPlatform->position().y - loadedPoint->position().y;
        distance = std::sqrt(dx * dx + dy * dy);
    }

    const bool connected = ok && loadedJoint->isConnected();
    ok = ok && connected && nearEqual(distance, 150.0f, 3.0f);

    // Stop and replay: a fresh runtime scene from the same authored source
    // must reproduce the same result.
    k2d::Scene replay;
    if (ok)
    {
        for (size_t i = 0; i < children.size(); ++i)
            k2d::Serializer::ReadObject(replay, children[i], &replay.root());
        replay.setGravity(Math::Vec2(0.0f, 980.0f));
        replay.setSimulationEnabled(true);
        for (int i = 0; i < 120; ++i)
            replay.update(1.0f / 60.0f);

        k2d::GameObject* replayPoint = replay.find("point");
        k2d::GameObject* replayPlatform = replay.find("platform");
        ok = ok && replayPoint && replayPlatform;
        if (ok)
        {
            const float dx = replayPlatform->position().x - replayPoint->position().x;
            const float dy = replayPlatform->position().y - replayPoint->position().y;
            const float replayDistance = std::sqrt(dx * dx + dy * dy);
            ok = ok && nearEqual(replayDistance, distance, 0.5f);
        }
    }

    std::printf("  scene_joint_flow: distance=%.2f (expected ~150.0) connected=%s\n", distance,
               connected ? "yes" : "no");
    return ok;
}

static bool testSerializerRoundTrip()
{
    k2d::RegisterPhysics2DSerializers();

    k2d::Scene source;
    k2d::GameObject* object = source.createObject("crate");
    object->setPosition(Math::Vec2(30.0f, -40.0f));

    k2d::RigidBody2D* body = object->addComponent<k2d::RigidBody2D>();
    body->setBodyType(k2d::BodyType::Kinematic);
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
    ok = ok && outBody && outBody->bodyType() == k2d::BodyType::Kinematic;
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

    target.setSimulationEnabled(true);
    ok = ok && target.bodyCount() == 1;
    ok = ok && outBody && outBody->inWorld() && outBody->ShapeCount() == 7;
    ok = ok && outChain && outChain->shapeCount() == 3;

    std::printf("  serializer: colliders=%d shapes=%d character=%s (chain owns %d)\n",
                (int)loaded->componentCount<k2d::Collider2D>(),
                outBody && outBody->inWorld() ? outBody->ShapeCount() : -1, outCharacter ? "yes" : "no",
                outChain ? outChain->shapeCount() : -1);
    return ok;
}

static bool testMaskContourTrace()
{
    const int width = 64;
    const int height = 64;
    ct::Vector<unsigned char> pixels((std::size_t)(width * height * 4), (unsigned char)255);

    auto setAlpha = [&](int x, int y, unsigned char a) { pixels[(std::size_t)(y * width + x) * 4 + 3] = a; };

    for (int y = 12; y < 52; ++y)
        for (int x = 12; x < 52; ++x)
            setAlpha(x, y, 0);

    for (int y = 15; y < 21; ++y)
        for (int x = 15; x < 21; ++x)
            setAlpha(x, y, 255);

    k2d::MaskContourOptions options;
    options.threshold = 127;
    options.simplifyTolerance = 1.0f;
    options.scale = 1.0f;
    options.minArea = 4.0f;

    ct::Vector<ct::Vector<Math::Vec2>> loops;
    const int loopCount = k2d::TraceMaskContours(pixels.data(), width, height, 4, options, loops);

    bool ok = loopCount == 2 && loops.size() == 2;

    int holeIndex = -1;
    int islandIndex = -1;
    for (std::size_t i = 0; ok && i < loops.size(); ++i)
    {
        float minX = loops[i][0].x;
        float maxX = loops[i][0].x;
        for (std::size_t j = 1; j < loops[i].size(); ++j)
        {
            if (loops[i][j].x < minX)
                minX = loops[i][j].x;
            if (loops[i][j].x > maxX)
                maxX = loops[i][j].x;
        }
        if (maxX - minX > 20.0f)
            holeIndex = (int)i;
        else
            islandIndex = (int)i;
    }

    ok = ok && holeIndex >= 0 && islandIndex >= 0;
    ok = ok && loops[(std::size_t)holeIndex].size() == 4;
    ok = ok && loops[(std::size_t)islandIndex].size() == 4;

    std::printf("  mask_trace: loops=%d hole_points=%d island_points=%d\n", loopCount,
                holeIndex >= 0 ? (int)loops[(std::size_t)holeIndex].size() : -1,
                islandIndex >= 0 ? (int)loops[(std::size_t)islandIndex].size() : -1);

    if (!ok)
        return false;

    k2d::Scene scene;
    k2d::GameObject* shore = scene.createObject("shoreline");
    shore->setPosition(Math::Vec2(0.0f, 0.0f));
    shore->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);

    k2d::ChainCollider2D* holeChain = shore->addComponent<k2d::ChainCollider2D>();
    holeChain->setPoints(loops[(std::size_t)holeIndex].data(), (int)loops[(std::size_t)holeIndex].size());
    holeChain->setLoop(true);

    k2d::ChainCollider2D* islandChain = shore->addComponent<k2d::ChainCollider2D>();
    islandChain->setPoints(loops[(std::size_t)islandIndex].data(), (int)loops[(std::size_t)islandIndex].size());
    islandChain->setLoop(true);

    const ct::Vector<Math::Vec2>& hole = loops[(std::size_t)holeIndex];
    float holeMinX = hole[0].x, holeMaxX = hole[0].x, holeMinY = hole[0].y, holeMaxY = hole[0].y;
    for (std::size_t i = 1; i < hole.size(); ++i)
    {
        holeMinX = hole[i].x < holeMinX ? hole[i].x : holeMinX;
        holeMaxX = hole[i].x > holeMaxX ? hole[i].x : holeMaxX;
        holeMinY = hole[i].y < holeMinY ? hole[i].y : holeMinY;
        holeMaxY = hole[i].y > holeMaxY ? hole[i].y : holeMaxY;
    }

    k2d::GameObject* boat = scene.createObject("boat");
    boat->setPosition(Math::Vec2((holeMinX + holeMaxX) * 0.5f, (holeMinY + holeMaxY) * 0.5f));
    k2d::RigidBody2D* boatBody = boat->addComponent<k2d::RigidBody2D>();
    boatBody->setBodyType(k2d::BodyType::Dynamic);
    boatBody->setBullet(true);
    boatBody->setVelocity(Math::Vec2(4000.0f, 0.0f));
    boat->addComponent<k2d::CircleCollider2D>()->setRadius(3.0f);

    scene.setGravity(Math::Vec2(0.0f, 0.0f));
    scene.setSimulationEnabled(true);

    for (int i = 0; i < 180; ++i)
        scene.update(1.0f / 60.0f);

    const bool insideHole = boat->position().x >= holeMinX - 0.5f && boat->position().x <= holeMaxX + 0.5f &&
                            boat->position().y >= holeMinY - 0.5f && boat->position().y <= holeMaxY + 0.5f;

    std::printf("  mask_trace: boat position=(%.2f, %.2f) inside_hole=%s\n", boat->position().x, boat->position().y,
                insideHole ? "yes" : "no");

    return ok && insideHole;
}

// A dynamic body under a rotated, scaled parent: the world-to-local write-back
// must invert the whole parent transform, not just subtract its position.
static bool testBodyUnderTransformedParent()
{
    k2d::Scene scene;
    k2d::GameObject* pivot = scene.createObject("pivot");
    pivot->setPosition(Math::Vec2(100.0f, -50.0f));
    pivot->setRotationDegrees(30.0f);
    pivot->setScale(Math::Vec2(2.0f, 2.0f));

    k2d::GameObject* box = scene.createObject("box", pivot);
    box->setPosition(Math::Vec2(10.0f, 0.0f));
    k2d::RigidBody2D* body = box->addComponent<k2d::RigidBody2D>();
    body->setBodyType(k2d::BodyType::Dynamic);
    body->setGravityScale(0.0f);
    box->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(8.0f, 8.0f));

    scene.setGravity(Math::Vec2(0.0f, 0.0f));
    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    const Math::Vec2 startWorld = box->globalPosition();
    body->setVelocity(Math::Vec2(60.0f, 0.0f));
    for (int i = 0; i < 60; ++i)
        scene.update(1.0f / 60.0f);

    // The body moved +60 world units in x over one second; the object's world
    // position must agree, which only holds if the local write-back inverted
    // the parent's rotation and scale.
    const Math::Vec2 endWorld = box->globalPosition();
    const bool movedInWorld = nearEqual(endWorld.x - startWorld.x, 60.0f, 3.0f) &&
                              nearEqual(endWorld.y - startWorld.y, 0.0f, 3.0f);
    const bool bodyAgrees = nearEqual(endWorld.x, body->Position().x, 0.5f) &&
                            nearEqual(endWorld.y, body->Position().y, 0.5f);

    std::printf("  parent_transform: world moved (%.1f, %.1f) body=(%.1f, %.1f)\n", endWorld.x - startWorld.x,
                endWorld.y - startWorld.y, body->Position().x, body->Position().y);
    return movedInWorld && bodyAgrees;
}


static bool testBodyWithoutColliderGetsDefaultBox()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);

    k2d::GameObject* bare = scene.createObject("bare");
    bare->setPosition(Math::Vec2(0.0f, 0.0f));
    k2d::RigidBody2D* body = bare->addComponent<k2d::RigidBody2D>();
    body->setBodyType(k2d::BodyType::Dynamic);

    scene.setSimulationEnabled(true);

    bool ok = body->ShapeCount() == 1;
    for (int i = 0; i < 180; ++i)
        scene.update(1.0f / 60.0f);

    const float expectedRest = 300.0f - 20.0f - 16.0f;
    ok = ok && nearEqual(bare->position().y, expectedRest, 2.0f);

    std::printf("  no_collider: shapes=%d rest_y=%.1f (expected %.1f)\n", body->ShapeCount(), bare->position().y,
                expectedRest);
    return ok;
}

static bool testChainColliderMinimumPoints()
{
    k2d::Scene scene;
    k2d::GameObject* open = scene.createObject("open_chain");
    open->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    const Math::Vec2 twoPoints[] = {Math::Vec2(-40.0f, 0.0f), Math::Vec2(40.0f, 0.0f)};
    open->addComponent<k2d::ChainCollider2D>()->setPoints(twoPoints, 2);

    k2d::GameObject* looped = scene.createObject("looped_chain");
    k2d::ChainCollider2D* loopCollider = looped->addComponent<k2d::ChainCollider2D>();
    loopCollider->setPoints(twoPoints, 2);
    loopCollider->setLoop(true);
    looped->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);

    scene.setSimulationEnabled(true);

    k2d::RigidBody2D* openBody = open->getComponent<k2d::RigidBody2D>();
    k2d::RigidBody2D* loopBody = looped->getComponent<k2d::RigidBody2D>();

    // Two points make exactly one segment; closing that loop needs three, so
    // the body falls back to the default box instead of getting no shape.
    bool ok = openBody->ShapeCount() == 1;
    ok = ok && loopBody->ShapeCount() == 1;
    ok = ok && loopCollider->shapeIndex() < 0;

    for (int i = 0; i < 30; ++i)
        scene.update(1.0f / 60.0f);
    ok = ok && scene.bodyCount() == 2;

    std::printf("  chain_min_points: open_shapes=%d loop_shapes=%d loop_attached=%s\n", openBody->ShapeCount(),
                loopBody->ShapeCount(), loopCollider->attached() ? "yes" : "no");
    return ok;
}

static bool testJointToItsOwnBodyStaysUnconnected()
{
    k2d::Scene scene;
    k2d::GameObject* box =
        makeBox(scene, "self", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);
    k2d::DistanceJoint2D* joint = box->addComponent<k2d::DistanceJoint2D>();
    joint->setTargetName("self");

    scene.setSimulationEnabled(true);
    for (int i = 0; i < 60; ++i)
        scene.update(1.0f / 60.0f);

    const bool finite = std::isfinite(box->position().x) && std::isfinite(box->position().y);
    const bool ok = !joint->isConnected() && finite;

    std::printf("  joint_self_target: connected=%s position=(%.1f, %.1f)\n", joint->isConnected() ? "yes" : "no",
                box->position().x, box->position().y);
    return ok;
}

static bool testJointSurvivesDestroyedBody()
{
    k2d::Scene scene;
    k2d::GameObject* anchor =
        makeBox(scene, "anchor", Math::Vec2(0.0f, 0.0f), Math::Vec2(10.0f, 10.0f), k2d::BodyType::Static);
    k2d::GameObject* hanging =
        makeBox(scene, "hanging", Math::Vec2(0.0f, 120.0f), Math::Vec2(20.0f, 20.0f), k2d::BodyType::Dynamic);

    k2d::DistanceJoint2D* joint = anchor->addComponent<k2d::DistanceJoint2D>();
    joint->setTargetName("hanging");

    scene.setSimulationEnabled(true);
    for (int i = 0; i < 30; ++i)
        scene.update(1.0f / 60.0f);

    bool ok = joint->isConnected();

    scene.destroy(hanging);
    for (int i = 0; i < 30; ++i)
        scene.update(1.0f / 60.0f);

    ok = ok && !joint->isConnected() && scene.bodyCount() == 1;
    ok = ok && std::isfinite(anchor->position().x) && std::isfinite(anchor->position().y);

    std::printf("  joint_dead_body: connected_after_destroy=%s bodies=%d\n", joint->isConnected() ? "yes" : "no",
                (int)scene.bodyCount());
    return ok;
}

static bool testColliderResizedToZero()
{
    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);
    k2d::GameObject* box =
        makeBox(scene, "shrinking", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    scene.setSimulationEnabled(true);
    for (int i = 0; i < 120; ++i)
        scene.update(1.0f / 60.0f);

    box->getComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(0.0f, 0.0f));
    for (int i = 0; i < 120; ++i)
        scene.update(1.0f / 60.0f);

    k2d::RigidBody2D* body = box->getComponent<k2d::RigidBody2D>();
    bool ok = body->ShapeCount() == 1;
    ok = ok && std::isfinite(box->position().x) && std::isfinite(box->position().y);
    // A degenerate box is clamped to a minimum extent, so it still lands on the
    // floor instead of tunnelling through it or producing a NaN mass.
    ok = ok && nearEqual(box->position().y, 280.0f, 2.0f);
    ok = ok && std::isfinite(body->Mass());

    std::printf("  collider_zero_size: shapes=%d rest_y=%.2f mass=%.3f\n", body->ShapeCount(), box->position().y,
                body->Mass());
    return ok;
}

static bool testMaskContourEdgeCases()
{
    const int width = 32;
    const int height = 32;

    ct::Vector<unsigned char> opaque((std::size_t)(width * height * 4), (unsigned char)255);
    ct::Vector<unsigned char> transparent((std::size_t)(width * height * 4), (unsigned char)255);
    for (int i = 0; i < width * height; ++i)
        transparent[(std::size_t)i * 4 + 3] = 0;

    ct::Vector<unsigned char> sliver((std::size_t)(1 * height * 4), (unsigned char)255);
    for (int y = 0; y < height; ++y)
        sliver[(std::size_t)y * 4 + 3] = (y >= 8 && y < 24) ? 0 : 255;

    ct::Vector<unsigned char> margin((std::size_t)(width * height * 4), (unsigned char)255);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const bool inside = x >= 8 && x < 24 && y >= 8 && y < 24;
            margin[(std::size_t)(y * width + x) * 4 + 3] = inside ? 255 : 0;
        }

    k2d::MaskContourOptions options;
    options.threshold = 127;
    options.simplifyTolerance = 1.0f;
    options.scale = 1.0f;
    options.minArea = 4.0f;

    ct::Vector<ct::Vector<Math::Vec2>> loops;

    // A uniform mask has one boundary or none depending on which side counts as
    // land: the traced loop is always the image border, never a crash.
    options.outsideIsSolid = true;
    const int opaqueSolid = k2d::TraceMaskContours(opaque.data(), width, height, 4, options, loops);
    const int transparentSolid = k2d::TraceMaskContours(transparent.data(), width, height, 4, options, loops);
    const int transparentSolidLoops = (int)loops.size();
    const int sliverSolid = k2d::TraceMaskContours(sliver.data(), 1, height, 4, options, loops);

    options.outsideIsSolid = false;
    const int opaqueOpen = k2d::TraceMaskContours(opaque.data(), width, height, 4, options, loops);
    const int transparentOpen = k2d::TraceMaskContours(transparent.data(), width, height, 4, options, loops);
    const int transparentOpenLoops = (int)loops.size();
    const int sliverOpen = k2d::TraceMaskContours(sliver.data(), 1, height, 4, options, loops);

    options.outsideIsSolid = true;
    ct::Vector<ct::Vector<Math::Vec2>> solidOutside;
    const int marginSolid = k2d::TraceMaskContours(margin.data(), width, height, 4, options, solidOutside);

    options.outsideIsSolid = false;
    ct::Vector<ct::Vector<Math::Vec2>> openOutside;
    const int marginOpen = k2d::TraceMaskContours(margin.data(), width, height, 4, options, openOutside);

    const auto spansWholeImage = [&](const ct::Vector<ct::Vector<Math::Vec2>>& traced)
    {
        for (std::size_t i = 0; i < traced.size(); ++i)
        {
            float minX = traced[i][0].x;
            float maxX = traced[i][0].x;
            for (std::size_t j = 1; j < traced[i].size(); ++j)
            {
                if (traced[i][j].x < minX)
                    minX = traced[i][j].x;
                if (traced[i][j].x > maxX)
                    maxX = traced[i][j].x;
            }
            if (maxX - minX >= (float)width - 0.5f)
                return true;
        }
        return false;
    };

    const bool solidHasBorder = spansWholeImage(solidOutside);
    const bool openHasBorder = spansWholeImage(openOutside);

    bool ok = opaqueSolid == 0 && transparentSolid == 1 && transparentSolidLoops == 1;
    ok = ok && opaqueOpen == 1 && transparentOpen == 0 && transparentOpenLoops == 0;
    // One pixel across encloses no area, whichever side is land.
    ok = ok && sliverSolid == 0 && sliverOpen == 0;
    ok = ok && marginSolid == 2 && marginOpen == 1;
    ok = ok && solidHasBorder && !openHasBorder;

    std::printf("  mask_edges: opaque=%d/%d transparent=%d/%d sliver=%d/%d (solid/open) "
                "margin_solid=%d(border=%s) margin_open=%d(border=%s)\n",
                opaqueSolid, opaqueOpen, transparentSolid, transparentOpen, sliverSolid, sliverOpen, marginSolid,
                solidHasBorder ? "yes" : "no", marginOpen, openHasBorder ? "yes" : "no");
    return ok;
}


static bool testSteeringSerializerRoundTrip()
{
    k2d::RegisterPhysics2DSerializers();

    k2d::Scene source;
    k2d::GameObject* walker = source.createObject("walker");

    k2d::Seek2D* seek = walker->addComponent<k2d::Seek2D>();
    seek->setTargetName("prize");
    seek->setWeight(1.5f);

    k2d::Flee2D* flee = walker->addComponent<k2d::Flee2D>();
    flee->setTargetPosition(Math::Vec2(12.0f, -34.0f));
    flee->setRadius(220.0f);
    flee->setWeight(0.75f);

    k2d::Arrive2D* arrive = walker->addComponent<k2d::Arrive2D>();
    arrive->setTargetPosition(Math::Vec2(400.0f, 250.0f));
    arrive->setSlowRadius(180.0f);
    arrive->setStopRadius(12.0f);

    k2d::Wander2D* wander = walker->addComponent<k2d::Wander2D>();
    wander->setRadius(30.0f);
    wander->setDistance(70.0f);
    wander->setJitter(4.5f);

    k2d::Separation2D* separation = walker->addComponent<k2d::Separation2D>();
    separation->setRadius(64.0f);
    separation->setMask(0x00F0);

    k2d::ObstacleAvoidance2D* avoidance = walker->addComponent<k2d::ObstacleAvoidance2D>();
    avoidance->setLookAhead(0.8f);
    avoidance->setMask(0x0003);

    const ct::String firstText = k2d::Serializer::WriteObject(*walker).dump();

    ct::Json::Error err;
    ct::Json parsed = ct::Json::parse(firstText, &err);
    k2d::Scene target;
    k2d::GameObject* copy = k2d::Serializer::ReadObject(target, parsed);
    bool ok = copy != nullptr;

    const ct::String secondText = ok ? k2d::Serializer::WriteObject(*copy).dump() : ct::String();
    ok = ok && firstText == secondText;
    ok = ok && target.steeringCount() == 6;

    k2d::Seek2D* copySeek = ok ? copy->getComponent<k2d::Seek2D>() : nullptr;
    ok = ok && copySeek && copySeek->targetName() == ct::String("prize") && nearEqual(copySeek->weight(), 1.5f, 0.001f);

    k2d::Flee2D* copyFlee = ok ? copy->getComponent<k2d::Flee2D>() : nullptr;
    ok = ok && copyFlee && nearEqual(copyFlee->radius(), 220.0f, 0.001f) &&
         nearEqual(copyFlee->weight(), 0.75f, 0.001f) && nearEqual(copyFlee->targetPosition().x, 12.0f, 0.001f) &&
         nearEqual(copyFlee->targetPosition().y, -34.0f, 0.001f);

    k2d::Arrive2D* copyArrive = ok ? copy->getComponent<k2d::Arrive2D>() : nullptr;
    ok = ok && copyArrive && nearEqual(copyArrive->slowRadius(), 180.0f, 0.001f) &&
         nearEqual(copyArrive->stopRadius(), 12.0f, 0.001f);

    k2d::Wander2D* copyWander = ok ? copy->getComponent<k2d::Wander2D>() : nullptr;
    ok = ok && copyWander && nearEqual(copyWander->radius(), 30.0f, 0.001f) &&
         nearEqual(copyWander->distance(), 70.0f, 0.001f) && nearEqual(copyWander->jitter(), 4.5f, 0.001f);

    k2d::Separation2D* copySeparation = ok ? copy->getComponent<k2d::Separation2D>() : nullptr;
    ok = ok && copySeparation && nearEqual(copySeparation->radius(), 64.0f, 0.001f) &&
         copySeparation->mask() == 0x00F0;

    k2d::ObstacleAvoidance2D* copyAvoidance = ok ? copy->getComponent<k2d::ObstacleAvoidance2D>() : nullptr;
    ok = ok && copyAvoidance && nearEqual(copyAvoidance->lookAhead(), 0.8f, 0.001f) &&
         copyAvoidance->mask() == 0x0003;

    std::printf("  steering_serializer: components=%d stable=%s\n", (int)target.steeringCount(),
                firstText == secondText ? "yes" : "no");
    return ok;
}

// A character used to move only when it collided: move_and_collide returned
// before applying travel on a clear path, so open ground was impassable.
// Godot applies the travel outside its `if (collided)` for the same reason.
static bool testCharacterMovesThroughOpenSpace()
{
    k2d::Scene scene;
    makeBox(scene, "far_wall", Math::Vec2(200.0f, 0.0f), Math::Vec2(20.0f, 200.0f), k2d::BodyType::Static);

    k2d::GameObject* player =
        makeBox(scene, "walker", Math::Vec2(0.0f, 0.0f), Math::Vec2(16.0f, 16.0f), k2d::BodyType::Kinematic);
    k2d::CharacterBody2D* character = player->addComponent<k2d::CharacterBody2D>();

    scene.setGravity(Math::Vec2(0.0f, 0.0f));
    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    character->setVelocity(Math::Vec2(600.0f, 0.0f));
    const bool hit = character->moveAndSlide();
    const float afterOne = player->position().x;
    const bool movedFreely = !hit && nearEqual(afterOne, 600.0f * scene.fixedTimeStep(), 0.5f);

    for (int i = 0; i < 60; ++i)
    {
        character->setVelocity(Math::Vec2(600.0f, 0.0f));
        character->moveAndSlide();
    }
    const bool stopped = player->position().x < 190.0f && character->isOnWall();

    std::printf("  character_open: one_slide=%.1f final=%.1f wall=%d\n", afterOne, player->position().x,
                character->isOnWall() ? 1 : 0);
    return movedFreely && stopped;
}

int main()
{
    const bool falls = testBoxFallsAndRests();
    const bool parentTransform = testBodyUnderTransformedParent();
    const bool characterOpen = testCharacterMovesThroughOpenSpace();
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
    const bool distanceJoint = testDistanceJointKeepsDistance();
    const bool revoluteJoint = testRevoluteJointMotorRotates();
    const bool jointSerialized = testJointSerializerRoundTrip();
    const bool jointAuthoringFlow = testSceneJointAuthoringFlow();
    const bool maskContour = testMaskContourTrace();
    const bool noCollider = testBodyWithoutColliderGetsDefaultBox();
    const bool chainMinPoints = testChainColliderMinimumPoints();
    const bool jointSelf = testJointToItsOwnBodyStaysUnconnected();
    const bool jointDeadBody = testJointSurvivesDestroyedBody();
    const bool colliderZero = testColliderResizedToZero();
    const bool maskEdges = testMaskContourEdgeCases();
    const bool steeringSerialized = testSteeringSerializerRoundTrip();

    std::printf("physics2d: falls=%s parent_transform=%s character_open=%s tilemap=%s contacts=%s sensor=%s queries=%s character=%s static_follow=%s "
                "velocity=%s determinism=%s destroy=%s late_spawn=%s collider_change=%s "
                "live_type=%s collider_world=%s filters=%s point_query=%s circle=%s edge=%s polygon=%s "
                "chain=%s compound=%s serializer=%s distance_joint=%s revolute_joint=%s joint_serializer=%s "
                "joint_authoring_flow=%s mask_contour=%s no_collider=%s chain_min_points=%s joint_self=%s "
                "joint_dead_body=%s collider_zero=%s mask_edges=%s steering_serializer=%s\n",
                falls ? "pass" : "fail", parentTransform ? "pass" : "fail", characterOpen ? "pass" : "fail", tileMap ? "pass" : "fail", contacts ? "pass" : "fail",
                sensor ? "pass" : "fail", queries ? "pass" : "fail", character ? "pass" : "fail",
                staticFollow ? "pass" : "fail", velocity ? "pass" : "fail", deterministic ? "pass" : "fail",
                destroy ? "pass" : "fail", lateSpawn ? "pass" : "fail", colliderChange ? "pass" : "fail",
                liveType ? "pass" : "fail", colliderWorld ? "pass" : "fail", filters ? "pass" : "fail",
                pointQuery ? "pass" : "fail",
                circle ? "pass" : "fail", edge ? "pass" : "fail", polygon ? "pass" : "fail", chain ? "pass" : "fail",
                compound ? "pass" : "fail", serialized ? "pass" : "fail", distanceJoint ? "pass" : "fail",
                revoluteJoint ? "pass" : "fail", jointSerialized ? "pass" : "fail",
                jointAuthoringFlow ? "pass" : "fail", maskContour ? "pass" : "fail",
                noCollider ? "pass" : "fail", chainMinPoints ? "pass" : "fail", jointSelf ? "pass" : "fail",
                jointDeadBody ? "pass" : "fail", colliderZero ? "pass" : "fail", maskEdges ? "pass" : "fail",
                steeringSerialized ? "pass" : "fail");
    return falls && parentTransform && characterOpen && tileMap && contacts && sensor && queries && character && staticFollow && velocity &&
                   deterministic && destroy && lateSpawn && colliderChange && liveType && colliderWorld && filters && pointQuery &&
                   circle && edge && polygon && chain && compound && serialized && distanceJoint && revoluteJoint &&
                   jointSerialized && jointAuthoringFlow && maskContour && noCollider && chainMinPoints &&
                   jointSelf && jointDeadBody && colliderZero && maskEdges && steeringSerialized
               ? 0
               : 1;
}
