#include <k2d/Arrive2D.h>
#include <k2d/BoxCollider2D.h>
#include <k2d/Flee2D.h>
#include <k2d/Geometry2D.h>
#include <k2d/Navigation2D.h>
#include <k2d/NavigationAgent2D.h>
#include <k2d/NavigationRegion2D.h>
#include <k2d/ObstacleAvoidance2D.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Seek2D.h>
#include <k2d/Separation2D.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>
#include <k2d/Steering2D.h>
#include <k2d/Wander2D.h>

#include <cmath>
#include <cstdio>

namespace
{
bool pointInsideBox(const Math::Vec2& p, float minX, float minY, float maxX, float maxY)
{
    return p.x > minX && p.x < maxX && p.y > minY && p.y < maxY;
}

bool TestHoleRouting()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable_with_hole");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();

    const Math::Vec2 outline[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(120.0f, 0.0f), Math::Vec2(120.0f, 120.0f),
                                  Math::Vec2(0.0f, 120.0f)};
    const Math::Vec2 hole[] = {Math::Vec2(40.0f, 40.0f), Math::Vec2(80.0f, 40.0f), Math::Vec2(80.0f, 80.0f),
                               Math::Vec2(40.0f, 80.0f)};
    const Math::Vec2* holes[] = {hole};
    const int holeCounts[] = {4};
    region->setPolygonWithHoles(outline, 4, holes, holeCounts, 1);

    ct::Vector<Math::Vec2> path;
    const bool found =
        region->valid() && k2d::Navigation2D::GetPath(scene, Math::Vec2(10.0f, 60.0f), Math::Vec2(110.0f, 60.0f), path);

    bool avoidsHole = found;
    for (size_t i = 0; i < path.size() && avoidsHole; ++i)
        avoidsHole = !pointInsideBox(path[i], 40.0f, 40.0f, 80.0f, 80.0f);

    std::printf("navigation hole: found=%s avoidsHole=%s waypoints=%d holes=%d\n", found ? "pass" : "fail",
               avoidsHole ? "pass" : "fail", static_cast<int>(path.size()), static_cast<int>(region->holes().size()));
    return found && avoidsHole;
}

bool TestHoleSerializerRoundTrip()
{
    k2d::Scene srcScene;
    k2d::GameObject* regionObject = srcScene.createObject("walkable_with_hole");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();

    const Math::Vec2 outline[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(120.0f, 0.0f), Math::Vec2(120.0f, 120.0f),
                                  Math::Vec2(0.0f, 120.0f)};
    const Math::Vec2 holeA[] = {Math::Vec2(10.0f, 10.0f), Math::Vec2(30.0f, 10.0f), Math::Vec2(30.0f, 30.0f),
                                Math::Vec2(10.0f, 30.0f)};
    const Math::Vec2 holeB[] = {Math::Vec2(60.0f, 60.0f), Math::Vec2(90.0f, 60.0f), Math::Vec2(90.0f, 90.0f)};
    const Math::Vec2* holes[] = {holeA, holeB};
    const int holeCounts[] = {4, 3};
    region->setPolygonWithHoles(outline, 4, holes, holeCounts, 2);

    const ct::Json written = k2d::Serializer::WriteObject(*regionObject);

    k2d::Scene dstScene;
    k2d::GameObject* copyObject = k2d::Serializer::ReadObject(dstScene, written);
    bool ok = copyObject != nullptr;

    k2d::NavigationRegion2D* copyRegion = ok ? copyObject->getComponent<k2d::NavigationRegion2D>() : nullptr;
    ok = ok && copyRegion != nullptr;

    if (ok)
    {
        ok = ok && copyRegion->polygon().size() == region->polygon().size();
        for (size_t i = 0; ok && i < region->polygon().size(); ++i)
            ok = ok && copyRegion->polygon()[i].x == region->polygon()[i].x &&
                 copyRegion->polygon()[i].y == region->polygon()[i].y;

        ok = ok && copyRegion->holes().size() == region->holes().size();
        for (size_t h = 0; ok && h < region->holes().size(); ++h)
        {
            ok = ok && copyRegion->holes()[h].size() == region->holes()[h].size();
            for (size_t i = 0; ok && i < region->holes()[h].size(); ++i)
                ok = ok && copyRegion->holes()[h][i].x == region->holes()[h][i].x &&
                     copyRegion->holes()[h][i].y == region->holes()[h][i].y;
        }
    }

    std::printf("navigation hole roundtrip: %s\n", ok ? "pass" : "fail");
    return ok;
}

bool TestFollowTarget()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable_follow");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(200.0f, 0.0f), Math::Vec2(200.0f, 200.0f),
                                  Math::Vec2(0.0f, 200.0f)};
    region->setPolygon(polygon, 4);

    k2d::GameObject* player = scene.createObject("player");
    player->setPosition(Math::Vec2(150.0f, 150.0f));

    k2d::GameObject* agentObject = scene.createObject("chaser");
    agentObject->setPosition(Math::Vec2(10.0f, 10.0f));
    k2d::NavigationAgent2D* agent = agentObject->addComponent<k2d::NavigationAgent2D>();
    agent->setFollowTargetName("player");

    const float dt = 1.0f / 60.0f;
    scene.update(dt);
    const bool initialOk = agent->hasPath() && !agent->path().empty() &&
        k2d::Distance(agent->path().back(), player->globalPosition()) < 0.5f;

    player->setPosition(Math::Vec2(20.0f, 20.0f));

    bool stillOldBeforeInterval = true;
    for (int i = 0; i < 10 && stillOldBeforeInterval; ++i)
    {
        scene.update(dt);
        stillOldBeforeInterval = agent->hasPath() && !agent->path().empty() &&
            k2d::Distance(agent->path().back(), Math::Vec2(150.0f, 150.0f)) < 0.5f;
    }

    bool repathedAfterInterval = false;
    for (int i = 0; i < 20 && !repathedAfterInterval; ++i)
    {
        scene.update(dt);
        repathedAfterInterval = agent->hasPath() && !agent->path().empty() &&
            k2d::Distance(agent->path().back(), player->globalPosition()) < 0.5f;
    }

    const bool ok = initialOk && stillOldBeforeInterval && repathedAfterInterval;
    std::printf("navigation follow: initial=%s throttled=%s repathed=%s\n", initialOk ? "pass" : "fail",
               stillOldBeforeInterval ? "pass" : "fail", repathedAfterInterval ? "pass" : "fail");
    return ok;
}

bool TestRepathThrottle()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable_throttle");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(200.0f, 0.0f), Math::Vec2(200.0f, 200.0f),
                                  Math::Vec2(0.0f, 200.0f)};
    region->setPolygon(polygon, 4);

    k2d::GameObject* player = scene.createObject("stationary_player");
    player->setPosition(Math::Vec2(150.0f, 150.0f));

    k2d::GameObject* agentObject = scene.createObject("chaser_throttle");
    agentObject->setPosition(Math::Vec2(10.0f, 10.0f));
    k2d::NavigationAgent2D* agent = agentObject->addComponent<k2d::NavigationAgent2D>();
    agent->setRepathInterval(0.25f);
    agent->setFollowTargetName("stationary_player");

    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i)
        scene.update(dt);

    const uint32_t repaths = agent->repathCount();
    const bool ok = repaths >= 1 && repaths <= 6;
    std::printf("navigation repath throttle: repaths=%u %s\n", static_cast<unsigned>(repaths), ok ? "pass" : "fail");
    return ok;
}

bool TestHoleTouchingTheOutline()
{
    // Holes have to be strictly interior: the triangulator bridges each hole to
    // the outline, and a hole already touching it leaves a zero-width bridge.
    // What matters is that such a region refuses to bake instead of producing a
    // mesh with holes in the wrong places.
    const Math::Vec2 outline[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(120.0f, 0.0f), Math::Vec2(120.0f, 120.0f),
                                  Math::Vec2(0.0f, 120.0f)};
    const Math::Vec2 edgeOnEdge[] = {Math::Vec2(0.0f, 40.0f), Math::Vec2(60.0f, 40.0f), Math::Vec2(60.0f, 80.0f),
                                     Math::Vec2(0.0f, 80.0f)};
    const Math::Vec2 vertexOnVertex[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(60.0f, 40.0f), Math::Vec2(40.0f, 60.0f)};
    const Math::Vec2 vertexOnEdge[] = {Math::Vec2(0.0f, 50.0f), Math::Vec2(60.0f, 40.0f), Math::Vec2(60.0f, 80.0f)};

    struct Case
    {
        const char* name;
        const Math::Vec2* points;
        int count;
    };
    const Case cases[] = {{"edge_on_edge", edgeOnEdge, 4},
                          {"vertex_on_vertex", vertexOnVertex, 3},
                          {"vertex_on_edge", vertexOnEdge, 3}};

    bool ok = true;
    for (const Case& testCase : cases)
    {
        k2d::Scene scene;
        k2d::GameObject* regionObject = scene.createObject("walkable_touching_hole");
        k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
        const Math::Vec2* holes[] = {testCase.points};
        const int holeCounts[] = {testCase.count};
        region->setPolygonWithHoles(outline, 4, holes, holeCounts, 1);

        ct::Vector<Math::Vec2> path;
        const bool refused =
            !region->valid() &&
            !k2d::Navigation2D::GetPath(scene, Math::Vec2(20.0f, 20.0f), Math::Vec2(100.0f, 100.0f), path) &&
            path.empty();
        ok = ok && refused;
        std::printf("navigation touching hole (%s): refused=%s triangles=%d\n", testCase.name,
                   refused ? "pass" : "fail", static_cast<int>(region->triangles().size() / 3));
    }

    // The same outline with the hole moved clear of it still bakes, so the
    // refusal above is about the contact, not about holes in general.
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable_clear_hole");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 clearHole[] = {Math::Vec2(20.0f, 40.0f), Math::Vec2(60.0f, 40.0f), Math::Vec2(60.0f, 80.0f),
                                    Math::Vec2(20.0f, 80.0f)};
    const Math::Vec2* holes[] = {clearHole};
    const int holeCounts[] = {4};
    region->setPolygonWithHoles(outline, 4, holes, holeCounts, 1);
    ct::Vector<Math::Vec2> path;
    const bool clearBakes =
        region->valid() && k2d::Navigation2D::GetPath(scene, Math::Vec2(10.0f, 20.0f), Math::Vec2(10.0f, 100.0f), path);
    ok = ok && clearBakes;
    std::printf("navigation touching hole (clear_of_outline): baked=%s triangles=%d\n", clearBakes ? "pass" : "fail",
               static_cast<int>(region->triangles().size() / 3));
    return ok;
}

bool TestSelfIntersectingOutline()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("bowtie");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();

    const Math::Vec2 bowtie[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(100.0f, 100.0f), Math::Vec2(100.0f, 0.0f),
                                 Math::Vec2(0.0f, 100.0f)};
    region->setPolygon(bowtie, 4);

    ct::Vector<Math::Vec2> path;
    const bool rejected = !k2d::Navigation2D::GetPath(scene, Math::Vec2(10.0f, 50.0f), Math::Vec2(90.0f, 50.0f), path);
    const bool empty = path.empty();

    std::printf("navigation self-intersecting: rejected=%s empty_path=%s triangles=%d\n", rejected ? "pass" : "fail",
               empty ? "pass" : "fail", static_cast<int>(region->triangles().size() / 3));
    return rejected && empty;
}

bool TestAgentWithoutRegion()
{
    k2d::Scene scene;
    k2d::GameObject* agentObject = scene.createObject("lost_agent");
    agentObject->setPosition(Math::Vec2(10.0f, 10.0f));
    k2d::NavigationAgent2D* agent = agentObject->addComponent<k2d::NavigationAgent2D>();
    agent->setAutoMove(true);

    const bool rejected = !agent->setTargetPosition(Math::Vec2(80.0f, 80.0f));

    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 30; ++i)
        scene.update(dt);

    const bool stayed = k2d::Distance(agentObject->globalPosition(), Math::Vec2(10.0f, 10.0f)) < 0.001f;
    const bool ok = rejected && !agent->hasPath() && agent->isNavigationFinished() && stayed;

    std::printf("navigation no region: rejected=%s has_path=%s stayed=%s\n", rejected ? "pass" : "fail",
               agent->hasPath() ? "fail" : "pass", stayed ? "pass" : "fail");
    return ok;
}

bool TestFollowTargetDestroyedMidPath()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable_destroy");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(200.0f, 0.0f), Math::Vec2(200.0f, 200.0f),
                                  Math::Vec2(0.0f, 200.0f)};
    region->setPolygon(polygon, 4);

    k2d::GameObject* prey = scene.createObject("prey");
    prey->setPosition(Math::Vec2(180.0f, 180.0f));

    k2d::GameObject* agentObject = scene.createObject("hunter");
    agentObject->setPosition(Math::Vec2(10.0f, 10.0f));
    k2d::NavigationAgent2D* agent = agentObject->addComponent<k2d::NavigationAgent2D>();
    agent->setFollowTargetName("prey");
    agent->setAutoMove(true);
    agent->setMaxSpeed(60.0f);

    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 20; ++i)
        scene.update(dt);

    Math::Vec2 followed(0.0f, 0.0f);
    const bool startedChasing = agent->hasPath() && agent->followPosition(followed);

    scene.destroy(prey);
    scene.update(dt);

    Math::Vec2 stale(0.0f, 0.0f);
    const bool followCleared = !agent->followPosition(stale);

    const Math::Vec2 before = agentObject->globalPosition();
    for (int i = 0; i < 400; ++i)
        scene.update(dt);
    const Math::Vec2 after = agentObject->globalPosition();

    // The last path stays valid, so the agent walks it out and then stops
    // instead of chasing freed memory.
    const bool finished = agent->isNavigationFinished();
    const bool onMesh = region->containsPoint(after);
    const bool ok = startedChasing && followCleared && finished && onMesh;

    std::printf("navigation follow destroyed: chasing=%s cleared=%s finished=%s moved=%.1f on_mesh=%s\n",
               startedChasing ? "pass" : "fail", followCleared ? "pass" : "fail", finished ? "pass" : "fail",
               k2d::Distance(before, after), onMesh ? "pass" : "fail");
    return ok;
}

bool TestPathEndpointsOutsideTheMesh()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable_bounds");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(100.0f, 0.0f), Math::Vec2(100.0f, 100.0f),
                                  Math::Vec2(0.0f, 100.0f)};
    region->setPolygon(polygon, 4);

    ct::Vector<Math::Vec2> path;
    const bool startOutside = !k2d::Navigation2D::GetPath(scene, Math::Vec2(-50.0f, 50.0f), Math::Vec2(50.0f, 50.0f),
                                                          path);
    const bool endOutside = !k2d::Navigation2D::GetPath(scene, Math::Vec2(50.0f, 50.0f), Math::Vec2(150.0f, 50.0f),
                                                        path);
    const bool bothOutside = !k2d::Navigation2D::GetPath(scene, Math::Vec2(-50.0f, -50.0f),
                                                         Math::Vec2(150.0f, 150.0f), path);
    const bool inside = k2d::Navigation2D::GetPath(scene, Math::Vec2(10.0f, 10.0f), Math::Vec2(90.0f, 90.0f), path);

    const bool ok = startOutside && endOutside && bothOutside && inside;
    std::printf("navigation outside mesh: start=%s end=%s both=%s inside_still_works=%s\n",
               startOutside ? "pass" : "fail", endOutside ? "pass" : "fail", bothOutside ? "pass" : "fail",
               inside ? "pass" : "fail");
    return ok;
}

k2d::NavigationRegion2D* makeField(k2d::Scene& scene, float size)
{
    k2d::GameObject* regionObject = scene.createObject("field");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(size, 0.0f), Math::Vec2(size, size),
                                  Math::Vec2(0.0f, size)};
    region->setPolygon(polygon, 4);
    return region;
}

k2d::GameObject* makeWalker(k2d::Scene& scene, const char* name, const Math::Vec2& position, bool withBody)
{
    k2d::GameObject* object = scene.createObject(name);
    object->setPosition(position);
    if (withBody)
    {
        object->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
        object->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(16.0f, 16.0f));
    }
    return object;
}

bool TestSteeringForceShapes()
{
    k2d::Scene scene;
    k2d::GameObject* prize = scene.createObject("prize");
    prize->setPosition(Math::Vec2(100.0f, 0.0f));

    k2d::GameObject* walker = scene.createObject("walker");
    walker->setPosition(Math::Vec2(0.0f, 0.0f));

    k2d::Seek2D* seek = walker->addComponent<k2d::Seek2D>();
    seek->setTargetName("prize");

    const Math::Vec2 rest(0.0f, 0.0f);
    Math::Vec2 force = scene.steeringForce(*walker, rest, 1.0f / 60.0f);
    bool ok = k2d::Distance(force, Math::Vec2(1.0f, 0.0f)) < 0.001f;

    seek->setWeight(0.25f);
    force = scene.steeringForce(*walker, rest, 1.0f / 60.0f);
    const bool weighted = k2d::Distance(force, Math::Vec2(0.25f, 0.0f)) < 0.001f;
    ok = ok && weighted;

    seek->setActive(false);
    const bool disabled = scene.steeringForce(*walker, rest, 1.0f / 60.0f).LengthSquared() == 0.0f;
    ok = ok && disabled;
    seek->setActive(true);
    seek->setWeight(1.0f);

    // A named target that no longer exists produces nothing rather than
    // steering at a stale position.
    scene.destroy(prize);
    scene.update(1.0f / 60.0f);
    const bool droppedTarget = scene.steeringForce(*walker, rest, 1.0f / 60.0f).LengthSquared() == 0.0f;
    ok = ok && droppedTarget;

    walker->removeComponent(seek);

    k2d::Flee2D* flee = walker->addComponent<k2d::Flee2D>();
    flee->setTargetPosition(Math::Vec2(0.0f, 40.0f));
    flee->setRadius(100.0f);
    force = scene.steeringForce(*walker, rest, 1.0f / 60.0f);
    const bool fleeing = force.y < -0.5f && force.Length() < 1.0f;
    flee->setRadius(10.0f);
    const bool fleeOutOfRange = scene.steeringForce(*walker, rest, 1.0f / 60.0f).LengthSquared() == 0.0f;
    ok = ok && fleeing && fleeOutOfRange;
    walker->removeComponent(flee);

    k2d::Arrive2D* arrive = walker->addComponent<k2d::Arrive2D>();
    arrive->setSlowRadius(100.0f);
    arrive->setStopRadius(10.0f);
    arrive->setTargetPosition(Math::Vec2(200.0f, 0.0f));
    const float farLength = scene.steeringForce(*walker, rest, 1.0f / 60.0f).Length();
    arrive->setTargetPosition(Math::Vec2(50.0f, 0.0f));
    const float nearLength = scene.steeringForce(*walker, rest, 1.0f / 60.0f).Length();
    arrive->setTargetPosition(Math::Vec2(5.0f, 0.0f));
    const float stopLength = scene.steeringForce(*walker, rest, 1.0f / 60.0f).Length();
    const bool easesOff = farLength > 0.99f && nearLength < farLength && nearLength > 0.0f && stopLength == 0.0f;
    ok = ok && easesOff;
    walker->removeComponent(arrive);

    k2d::Wander2D* wander = walker->addComponent<k2d::Wander2D>();
    bool wanderUnit = true;
    bool wanderTurns = false;
    Math::Vec2 previous = scene.steeringForce(*walker, Math::Vec2(1.0f, 0.0f), 1.0f / 60.0f);
    for (int i = 0; i < 120; ++i)
    {
        const Math::Vec2 next = scene.steeringForce(*walker, Math::Vec2(1.0f, 0.0f), 1.0f / 60.0f);
        wanderUnit = wanderUnit && std::fabs(next.Length() - 1.0f) < 0.001f;
        if (k2d::Distance(next, previous) > 0.0001f)
            wanderTurns = true;
        previous = next;
    }
    ok = ok && wanderUnit && wanderTurns;

    std::printf("steering forces: seek=%s weight=%s disabled=%s dead_target=%s flee=%s arrive=%s wander=%s\n",
               ok ? "pass" : "check", weighted ? "pass" : "fail", disabled ? "pass" : "fail",
               droppedTarget ? "pass" : "fail", (fleeing && fleeOutOfRange) ? "pass" : "fail",
               easesOff ? "pass" : "fail", (wanderUnit && wanderTurns) ? "pass" : "fail");
    return ok;
}

bool TestAgentWithoutSteeringIsUnchanged()
{
    const float dt = 1.0f / 60.0f;

    k2d::Scene plain;
    makeField(plain, 400.0f);
    k2d::GameObject* plainWalker = makeWalker(plain, "walker", Math::Vec2(20.0f, 20.0f), false);
    k2d::NavigationAgent2D* plainAgent = plainWalker->addComponent<k2d::NavigationAgent2D>();
    plainAgent->setAutoMove(true);
    plainAgent->setMaxSpeed(120.0f);
    plainAgent->setTargetPosition(Math::Vec2(360.0f, 300.0f));

    k2d::Scene inert;
    makeField(inert, 400.0f);
    k2d::GameObject* inertWalker = makeWalker(inert, "walker", Math::Vec2(20.0f, 20.0f), false);
    k2d::NavigationAgent2D* inertAgent = inertWalker->addComponent<k2d::NavigationAgent2D>();
    inertAgent->setAutoMove(true);
    inertAgent->setMaxSpeed(120.0f);
    k2d::Seek2D* mute = inertWalker->addComponent<k2d::Seek2D>();
    mute->setTargetPosition(Math::Vec2(0.0f, 0.0f));
    mute->setWeight(0.0f);
    k2d::Separation2D* off = inertWalker->addComponent<k2d::Separation2D>();
    off->setActive(false);
    inertAgent->setTargetPosition(Math::Vec2(360.0f, 300.0f));

    bool identical = true;
    for (int i = 0; i < 300; ++i)
    {
        plain.update(dt);
        inert.update(dt);
        identical = identical && plainWalker->position().x == inertWalker->position().x &&
                    plainWalker->position().y == inertWalker->position().y;
    }

    const bool arrived = plainAgent->isNavigationFinished() && inertAgent->isNavigationFinished();
    std::printf("steering neutral: identical=%s arrived=%s at=(%.3f, %.3f)\n", identical ? "pass" : "fail",
               arrived ? "pass" : "fail", plainWalker->position().x, plainWalker->position().y);
    return identical && arrived;
}

bool TestSeparationSpreadsACrowd()
{
    const float dt = 1.0f / 60.0f;
    const Math::Vec2 meetingPoint(300.0f, 200.0f);

    const auto run = [&](bool withSeparation, float& outDistance)
    {
        k2d::Scene scene;
        makeField(scene, 400.0f);
        k2d::GameObject* first = makeWalker(scene, "first", Math::Vec2(40.0f, 120.0f), true);
        k2d::GameObject* second = makeWalker(scene, "second", Math::Vec2(40.0f, 280.0f), true);

        k2d::GameObject* walkers[] = {first, second};
        for (k2d::GameObject* walker : walkers)
        {
            k2d::NavigationAgent2D* agent = walker->addComponent<k2d::NavigationAgent2D>();
            agent->setAutoMove(true);
            agent->setMaxSpeed(120.0f);
            if (withSeparation)
                walker->addComponent<k2d::Separation2D>()->setRadius(60.0f);
            agent->setTargetPosition(meetingPoint);
        }

        scene.setSimulationEnabled(true);
        for (int i = 0; i < 300; ++i)
            scene.update(dt);
        outDistance = k2d::Distance(first->globalPosition(), second->globalPosition());
    };

    float crowded = 0.0f;
    float spread = 0.0f;
    run(false, crowded);
    run(true, spread);

    const bool ok = crowded < 4.0f && spread > 40.0f;
    std::printf("steering separation: crowded_gap=%.2f separated_gap=%.2f %s\n", crowded, spread,
               ok ? "pass" : "fail");
    return ok;
}

bool TestSeparationOnlySeesBodies()
{
    k2d::Scene scene;
    makeField(scene, 400.0f);
    k2d::GameObject* walker = makeWalker(scene, "walker", Math::Vec2(100.0f, 100.0f), true);
    k2d::Separation2D* separation = walker->addComponent<k2d::Separation2D>();
    separation->setRadius(80.0f);

    k2d::GameObject* ghost = makeWalker(scene, "ghost", Math::Vec2(120.0f, 100.0f), false);
    ghost->addComponent<k2d::Separation2D>()->setRadius(80.0f);

    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    const bool ignoresBodiless = scene.steeringForce(*walker, Math::Vec2(0.0f, 0.0f), 1.0f / 60.0f).LengthSquared() == 0.0f;

    k2d::GameObject* solid = makeWalker(scene, "solid", Math::Vec2(120.0f, 100.0f), true);
    solid->getComponent<k2d::BoxCollider2D>()->setFilter(0x0002, 0xFFFF);
    scene.update(1.0f / 60.0f);

    const Math::Vec2 pushed = scene.steeringForce(*walker, Math::Vec2(0.0f, 0.0f), 1.0f / 60.0f);
    const bool pushesAway = pushed.x < -0.1f;

    separation->setMask(0x0001);
    scene.update(1.0f / 60.0f);
    const bool maskFiltersOut =
        scene.steeringForce(*walker, Math::Vec2(0.0f, 0.0f), 1.0f / 60.0f).LengthSquared() == 0.0f;

    (void)solid;
    const bool ok = ignoresBodiless && pushesAway && maskFiltersOut;
    std::printf("steering separation sources: bodiless_ignored=%s body_pushes=%s mask_filters=%s\n",
               ignoresBodiless ? "pass" : "fail", pushesAway ? "pass" : "fail", maskFiltersOut ? "pass" : "fail");
    return ok;
}

bool TestObstacleAvoidanceSteersAside()
{
    k2d::Scene scene;
    makeField(scene, 400.0f);

    k2d::GameObject* wall = scene.createObject("wall");
    wall->setPosition(Math::Vec2(200.0f, 200.0f));
    wall->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    wall->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(20.0f, 200.0f));

    k2d::GameObject* walker = makeWalker(scene, "walker", Math::Vec2(100.0f, 200.0f), true);
    k2d::ObstacleAvoidance2D* avoidance = walker->addComponent<k2d::ObstacleAvoidance2D>();
    avoidance->setLookAhead(1.0f);

    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    const Math::Vec2 heading(120.0f, 0.0f);
    const Math::Vec2 force = scene.steeringForce(*walker, heading, 1.0f / 60.0f);
    const bool lateral = std::fabs(force.y) > 0.1f && std::fabs(force.x) < 0.001f;

    const Math::Vec2 away = scene.steeringForce(*walker, Math::Vec2(-120.0f, 0.0f), 1.0f / 60.0f);
    const bool clearBehind = away.LengthSquared() == 0.0f;

    const Math::Vec2 still = scene.steeringForce(*walker, Math::Vec2(0.0f, 0.0f), 1.0f / 60.0f);
    const bool restIsQuiet = still.LengthSquared() == 0.0f;

    avoidance->setMask(0x0002);
    const Math::Vec2 masked = scene.steeringForce(*walker, heading, 1.0f / 60.0f);
    const bool maskFiltersOut = masked.LengthSquared() == 0.0f;

    const bool ok = lateral && clearBehind && restIsQuiet && maskFiltersOut;
    std::printf("steering obstacle: lateral=(%.3f, %.3f) behind_clear=%s at_rest=%s mask_filters=%s\n", force.x,
               force.y, clearBehind ? "pass" : "fail", restIsQuiet ? "pass" : "fail",
               maskFiltersOut ? "pass" : "fail");
    return ok;
}

bool TestSceneTracksSteeringComponents()
{
    k2d::Scene scene;
    k2d::GameObject* walker = scene.createObject("walker");
    bool ok = scene.steeringCount() == 0;

    k2d::Seek2D* seek = walker->addComponent<k2d::Seek2D>();
    walker->addComponent<k2d::Wander2D>();
    ok = ok && scene.steeringCount() == 2;
    ok = ok && scene.steeringAt(0) != nullptr && scene.steeringAt(1) != nullptr;
    ok = ok && scene.steeringAt(2) == nullptr;

    walker->removeComponent(seek);
    scene.update(1.0f / 60.0f);
    ok = ok && scene.steeringCount() == 1 && scene.steeringAt(0) != nullptr;

    scene.destroy(walker);
    scene.update(1.0f / 60.0f);
    ok = ok && scene.steeringCount() == 0;

    std::printf("steering scene list: %s\n", ok ? "pass" : "fail");
    return ok;
}
} // namespace

// The failure mode this pins: a target that can never be reached used to
// bypass the interval and run a full pathfind every single frame.
bool TestUnreachableTargetIsStillThrottled()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable_unreachable");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(200.0f, 0.0f), Math::Vec2(200.0f, 200.0f),
                                  Math::Vec2(0.0f, 200.0f)};
    region->setPolygon(polygon, 4);

    // Parked well outside the mesh: every path request must fail.
    k2d::GameObject* player = scene.createObject("offmesh_player");
    player->setPosition(Math::Vec2(900.0f, 900.0f));

    k2d::GameObject* agentObject = scene.createObject("chaser_unreachable");
    agentObject->setPosition(Math::Vec2(10.0f, 10.0f));
    k2d::NavigationAgent2D* agent = agentObject->addComponent<k2d::NavigationAgent2D>();
    agent->setRepathInterval(0.25f);
    agent->setFollowTargetName("offmesh_player");

    for (int i = 0; i < 60; ++i)
        scene.update(1.0f / 60.0f);

    const uint32_t repaths = agent->repathCount();
    const bool ok = repaths >= 1 && repaths <= 6 && !agent->hasPath();
    std::printf("navigation unreachable throttle: repaths=%u %s\n", static_cast<unsigned>(repaths),
                ok ? "pass" : "fail");
    return ok;
}

// A follow target that names nothing must not re-walk the tree every frame.
// There is no counter for find(), so this asserts the observable half: the
// agent stays put and costs no paths at all.
bool TestMissingFollowTargetCostsNothing()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable_missing");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(200.0f, 0.0f), Math::Vec2(200.0f, 200.0f),
                                  Math::Vec2(0.0f, 200.0f)};
    region->setPolygon(polygon, 4);

    k2d::GameObject* agentObject = scene.createObject("chaser_missing");
    agentObject->setPosition(Math::Vec2(10.0f, 10.0f));
    k2d::NavigationAgent2D* agent = agentObject->addComponent<k2d::NavigationAgent2D>();
    agent->setFollowTargetName("nobody_by_this_name");

    for (int i = 0; i < 60; ++i)
        scene.update(1.0f / 60.0f);

    const bool ok = agent->repathCount() == 0 && !agent->hasPath() &&
                    std::fabs(agentObject->position().x - 10.0f) < 0.001f &&
                    std::fabs(agentObject->position().y - 10.0f) < 0.001f;
    std::printf("navigation missing target: repaths=%u stayed=%s\n",
                static_cast<unsigned>(agent->repathCount()), ok ? "pass" : "fail");
    return ok;
}

// An agent under a rotated parent must still reach its waypoint: path deltas
// are global while translate() adds in the parent's frame.
bool TestAgentUnderRotatedParent()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable_parented");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(-400.0f, -400.0f), Math::Vec2(400.0f, -400.0f),
                                  Math::Vec2(400.0f, 400.0f), Math::Vec2(-400.0f, 400.0f)};
    region->setPolygon(polygon, 4);

    k2d::GameObject* pivot = scene.createObject("pivot");
    pivot->setPosition(Math::Vec2(50.0f, -20.0f));
    pivot->setRotationDegrees(90.0f);

    k2d::GameObject* agentObject = scene.createObject("parented_agent", pivot);
    agentObject->setPosition(Math::Vec2(0.0f, 0.0f));
    k2d::NavigationAgent2D* agent = agentObject->addComponent<k2d::NavigationAgent2D>();
    agent->setMaxSpeed(120.0f);
    agent->setAutoMove(true);

    const Math::Vec2 goal(200.0f, 150.0f);
    agent->setTargetPosition(goal);

    const Math::Vec2 start = agentObject->globalPosition();
    const float startDistance = k2d::Distance(start, goal);
    for (int i = 0; i < 240; ++i)
        scene.update(1.0f / 60.0f);

    const Math::Vec2 end = agentObject->globalPosition();
    const float endDistance = k2d::Distance(end, goal);
    // The agent stops within pathDesiredDistance (8 by default) of the final
    // waypoint, so "arrived" is that tolerance, not zero. Before the fix it
    // orbited and the distance never shrank at all.
    const bool ok = endDistance <= agent->pathDesiredDistance() + 1.0f && endDistance < startDistance * 0.2f;
    std::printf("navigation parented agent: %.1f -> %.1f from goal %s\n", startDistance, endDistance,
                ok ? "pass" : "fail");
    return ok;
}

// An agent saved with no target must not load pointing at the world origin.
bool TestAgentTargetPresenceRoundTrip()
{
    k2d::Scene source;
    k2d::GameObject* object = source.createObject("saved_agent");
    k2d::NavigationAgent2D* agent = object->addComponent<k2d::NavigationAgent2D>();
    agent->setMaxSpeed(77.0f);
    const bool untargetedBefore = !agent->hasTarget();

    const ct::Json json = k2d::Serializer::WriteObject(*object);
    k2d::Scene target;
    k2d::GameObject* loaded = k2d::Serializer::ReadObject(target, json);
    k2d::NavigationAgent2D* out = loaded ? loaded->getComponent<k2d::NavigationAgent2D>() : nullptr;
    const bool untargetedAfter = out && !out->hasTarget();

    k2d::Scene source2;
    k2d::GameObject* object2 = source2.createObject("saved_agent2");
    k2d::NavigationAgent2D* agent2 = object2->addComponent<k2d::NavigationAgent2D>();
    agent2->setTargetPosition(Math::Vec2(33.0f, -44.0f));
    const ct::Json json2 = k2d::Serializer::WriteObject(*object2);
    k2d::Scene target2;
    k2d::GameObject* loaded2 = k2d::Serializer::ReadObject(target2, json2);
    k2d::NavigationAgent2D* out2 = loaded2 ? loaded2->getComponent<k2d::NavigationAgent2D>() : nullptr;
    const bool targetedSurvives = out2 && out2->hasTarget() && std::fabs(out2->targetPosition().x - 33.0f) < 0.001f &&
                                  std::fabs(out2->targetPosition().y + 44.0f) < 0.001f;

    const bool ok = untargetedBefore && untargetedAfter && targetedSurvives;
    std::printf("navigation agent target round trip: untargeted=%s targeted=%s\n",
                untargetedAfter ? "pass" : "fail", targetedSurvives ? "pass" : "fail");
    return ok;
}

int main()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(0.0f, 0.0f),   Math::Vec2(120.0f, 0.0f),  Math::Vec2(120.0f, 36.0f),
                                  Math::Vec2(36.0f, 36.0f), Math::Vec2(36.0f, 120.0f), Math::Vec2(0.0f, 120.0f)};
    region->setPolygon(polygon, 6);

    ct::Vector<Math::Vec2> path;
    const bool concavePath =
        region->valid() &&
        k2d::Navigation2D::GetPath(scene, Math::Vec2(12.0f, 96.0f), Math::Vec2(96.0f, 12.0f), path) && path.size() >= 2;
    const bool outsideRejected =
        !k2d::Navigation2D::GetPath(scene, Math::Vec2(-1.0f, 0.0f), Math::Vec2(96.0f, 12.0f), path);

    k2d::GameObject* agentObject = scene.createObject("agent");
    agentObject->setPosition(Math::Vec2(12.0f, 96.0f));
    k2d::NavigationAgent2D* agent = agentObject->addComponent<k2d::NavigationAgent2D>();
    const bool agentPath =
        agent->setTargetPosition(Math::Vec2(96.0f, 12.0f)) && agent->hasPath() && !agent->isNavigationFinished();

    const bool holeRouting = TestHoleRouting();
    const bool holeRoundTrip = TestHoleSerializerRoundTrip();
    const bool followTarget = TestFollowTarget();
    const bool repathThrottle = TestRepathThrottle();
    const bool touchingHole = TestHoleTouchingTheOutline();
    const bool selfIntersecting = TestSelfIntersectingOutline();
    const bool noRegion = TestAgentWithoutRegion();
    const bool followDestroyed = TestFollowTargetDestroyedMidPath();
    const bool outsideMesh = TestPathEndpointsOutsideTheMesh();
    const bool steeringForces = TestSteeringForceShapes();
    const bool steeringNeutral = TestAgentWithoutSteeringIsUnchanged();
    const bool separationSpread = TestSeparationSpreadsACrowd();
    const bool separationSources = TestSeparationOnlySeesBodies();
    const bool obstacleAside = TestObstacleAvoidanceSteersAside();
    const bool steeringList = TestSceneTracksSteeringComponents();

    std::printf("navigation: concave=%s outside=%s agent=%s triangles=%d waypoints=%d\n", concavePath ? "pass" : "fail",
               outsideRejected ? "pass" : "fail", agentPath ? "pass" : "fail",
               static_cast<int>(region->triangles().size() / 3), static_cast<int>(agent->path().size()));
    const bool unreachableThrottle = TestUnreachableTargetIsStillThrottled();
    const bool missingTarget = TestMissingFollowTargetCostsNothing();
    const bool parentedAgent = TestAgentUnderRotatedParent();
    const bool agentRoundTrip = TestAgentTargetPresenceRoundTrip();

    return unreachableThrottle && missingTarget && parentedAgent && agentRoundTrip &&
           concavePath && outsideRejected && agentPath && holeRouting && holeRoundTrip && followTarget &&
                   repathThrottle && touchingHole && selfIntersecting && noRegion && followDestroyed &&
                   outsideMesh && steeringForces && steeringNeutral && separationSpread && separationSources &&
                   obstacleAside && steeringList
               ? 0
               : 1;
}
