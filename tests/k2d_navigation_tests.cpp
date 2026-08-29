#include <k2d/BoxCollider2D.h>
#include <k2d/Geometry2D.h>
#include <k2d/Navigation2D.h>
#include <k2d/NavigationAgent2D.h>
#include <k2d/NavigationRegion2D.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>
#include <k2d/Steering2D.h>

#include <cmath>
#include <limits>
#include <k2d/CircleCollider2D.h>
#include <k2d/Formation2D.h>

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

    // Godot snaps a query onto the map instead of refusing it. Refusing was
    // what stranded an agent for good: a character standing against a wall sits
    // outside a mesh that was never shrunk by its radius, so most requests
    // failed and nothing ever recovered.
    ct::Vector<Math::Vec2> path;
    const bool startOutside =
        k2d::Navigation2D::GetPath(scene, Math::Vec2(-50.0f, 50.0f), Math::Vec2(50.0f, 50.0f), path) &&
        region->containsPoint(path[0]);
    const bool endOutside =
        k2d::Navigation2D::GetPath(scene, Math::Vec2(50.0f, 50.0f), Math::Vec2(150.0f, 50.0f), path) &&
        region->containsPoint(path[path.size() - 1]) && std::fabs(path[path.size() - 1].x - 100.0f) < 1.0f;
    const bool bothOutside =
        k2d::Navigation2D::GetPath(scene, Math::Vec2(-50.0f, -50.0f), Math::Vec2(150.0f, 150.0f), path) &&
        region->containsPoint(path[0]) && region->containsPoint(path[path.size() - 1]);
    const bool inside = k2d::Navigation2D::GetPath(scene, Math::Vec2(10.0f, 10.0f), Math::Vec2(90.0f, 90.0f), path);

    const bool ok = startOutside && endOutside && bothOutside && inside;
    std::printf("navigation outside mesh snapped: start=%s end=%s both=%s inside_still_works=%s\n",
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
    k2d::GameObject* walker = scene.createObject("walker");
    walker->setPosition(Math::Vec2(0.0f, 0.0f));
    k2d::Steering2D* steering = walker->addComponent<k2d::Steering2D>();

    const float maxSpeed = 100.0f;
    const Math::Vec2 rest(0.0f, 0.0f);
    const Math::Vec2 goal(100.0f, 0.0f);

    // Every OpenSteer behaviour answers with the difference between the
    // velocity it wants and the velocity there is, so one already travelling
    // at the wanted velocity is asked for nothing at all.
    const Math::Vec2 seekAtRest = steering->seek(goal, rest, maxSpeed);
    const bool seeks = k2d::Distance(seekAtRest, Math::Vec2(100.0f, 0.0f)) < 0.01f;
    const Math::Vec2 seekUpToSpeed = steering->seek(goal, Math::Vec2(100.0f, 0.0f), maxSpeed);
    const bool seekSettles = seekUpToSpeed.Length() < 0.01f;

    const Math::Vec2 fleeAtRest = steering->flee(goal, rest, maxSpeed);
    const bool flees = k2d::Distance(fleeAtRest, Math::Vec2(-100.0f, 0.0f)) < 0.01f;

    steering->setSlowRadius(100.0f);
    const float far = steering->arrive(Math::Vec2(400.0f, 0.0f), rest, maxSpeed).Length();
    const float close = steering->arrive(Math::Vec2(25.0f, 0.0f), rest, maxSpeed).Length();
    // Arriving brakes: standing on the mark with speed on asks for all of it back.
    const Math::Vec2 onTheMark = steering->arrive(Math::Vec2(0.0f, 0.0f), Math::Vec2(60.0f, 0.0f), maxSpeed);
    const bool easesOff = far > 99.0f && close < far && close > 0.0f &&
                          k2d::Distance(onTheMark, Math::Vec2(-60.0f, 0.0f)) < 0.01f;

    bool wanderSpeed = true;
    bool wanderTurns = false;
    Math::Vec2 previous = steering->wander(1.0f / 60.0f, Math::Vec2(1.0f, 0.0f), maxSpeed);
    for (int i = 0; i < 120; ++i)
    {
        const Math::Vec2 next = steering->wander(1.0f / 60.0f, Math::Vec2(1.0f, 0.0f), maxSpeed);
        wanderSpeed = wanderSpeed && std::fabs(next.Length() - maxSpeed) < 0.1f;
        if (k2d::Distance(next, previous) > 0.0001f)
            wanderTurns = true;
        previous = next;
    }

    // Nothing switched on means nothing is added to the agent's own path
    // following, however much the behaviours would answer if asked directly.
    const bool quietByDefault = scene.steeringForce(*walker, Math::Vec2(50.0f, 0.0f), 1.0f / 60.0f).LengthSquared() ==
                                0.0f;

    const bool ok = seeks && seekSettles && flees && easesOff && wanderSpeed && wanderTurns && quietByDefault;
    std::printf("steering forces: seek=%s settles=%s flee=%s arrive=%s wander=%s quiet_until_enabled=%s\n",
               seeks ? "pass" : "fail", seekSettles ? "pass" : "fail", flees ? "pass" : "fail",
               easesOff ? "pass" : "fail", (wanderSpeed && wanderTurns) ? "pass" : "fail",
               quietByDefault ? "pass" : "fail");
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
    k2d::Steering2D* mute = inertWalker->addComponent<k2d::Steering2D>();
    mute->setTargetPosition(Math::Vec2(0.0f, 0.0f));
    mute->setWeight(0.0f);
    k2d::Steering2D* off = inertWalker->addComponent<k2d::Steering2D>();
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

bool TestDynamicAgentFacesItsPath()
{
    const float dt = 1.0f / 60.0f;
    k2d::Scene scene;
    makeField(scene, 400.0f);

    k2d::GameObject* runner = scene.createObject("facing_runner");
    runner->setPosition(Math::Vec2(200.0f, 20.0f));
    k2d::RigidBody2D* body = runner->addComponent<k2d::RigidBody2D>();
    body->setBodyType(k2d::BodyType::Dynamic);
    body->setFixedRotation(true);
    body->setLinearDamping(6.0f);
    runner->addComponent<k2d::CircleCollider2D>()->setRadius(10.0f);
    k2d::NavigationAgent2D* agent = runner->addComponent<k2d::NavigationAgent2D>();
    agent->setMaxSpeed(120.0f);
    agent->setAutoMove(true);
    agent->setOrientToPath(true);
    agent->setTargetPosition(Math::Vec2(200.0f, 380.0f));

    scene.setSimulationEnabled(true);
    for (int frame = 0; frame < 120; ++frame)
        scene.update(dt);

    // Walking due south, so it should be facing 90 degrees. Due east would
    // mean the facing was never written at all - which is what a body angle
    // pinned at zero looks like.
    const float facing = runner->rotationDegrees();
    const bool ok = std::fabs(facing - 90.0f) < 20.0f && runner->globalPosition().y > 100.0f;
    std::printf("navigation dynamic agent faces its path: y=%.1f facing=%.1f deg (want ~90) %s\n",
                runner->globalPosition().y, facing, ok ? "pass" : "fail");
    return ok;
}

bool TestSpawnedTogetherDoNotRepathTogether()
{
    const float dt = 1.0f / 60.0f;
    const int count = 40;
    k2d::Scene scene;
    makeField(scene, 800.0f);
    k2d::GameObject* quarry = makeWalker(scene, "quarry", Math::Vec2(400.0f, 400.0f), false);

    // A wave: every one of them created on the same frame, the way a door
    // opening spawns a room full of enemies.
    ct::Vector<k2d::NavigationAgent2D*> agents;
    for (int i = 0; i < count; ++i)
    {
        char name[24];
        std::snprintf(name, sizeof(name), "wave%d", i);
        k2d::GameObject* object = makeWalker(scene, name, Math::Vec2(40.0f + 4.0f * i, 40.0f), false);
        object->setTag("wave");
        k2d::NavigationAgent2D* agent = object->addComponent<k2d::NavigationAgent2D>();
        agent->setMaxSpeed(100.0f);
        agent->setAutoMove(true);
        k2d::Formation2D* formation = object->addComponent<k2d::Formation2D>();
        formation->setGroupTag("wave");
        formation->setAnchorName("quarry");
        formation->setSpacing(120.0f);
        agents.push_back(agent);
    }

    scene.setSimulationEnabled(true);
    int busiest = 0;
    uint32_t previous = 0;
    for (int frame = 0; frame < 240; ++frame)
    {
        // Keep the quarry moving so the places really do have to be recomputed.
        quarry->setPosition(Math::Vec2(400.0f + 60.0f * std::cos(frame * 0.05f),
                                       400.0f + 60.0f * std::sin(frame * 0.05f)));
        scene.update(dt);
        uint32_t total = 0;
        for (std::size_t i = 0; i < agents.size(); ++i)
            total += agents[i]->repathCount();
        const int thisFrame = (int)(total - previous);
        previous = total;
        if (frame > 30 && thisFrame > busiest)
            busiest = thisFrame;
    }

    // Spread over a quarter-second interval at 60fps, a fair share is about a
    // sixth of them per frame. Half the crowd in one frame is a stampede.
    const bool ok = busiest < count / 2;
    std::printf("navigation wave repaths spread out: busiest frame repathed %d of %d %s\n", busiest, count,
                ok ? "pass" : "fail");
    return ok;
}

bool TestSurroundIgnoresTheAnchorFacing()
{
    const float dt = 1.0f / 60.0f;
    k2d::Scene scene;
    makeField(scene, 600.0f);
    k2d::GameObject* quarry = makeWalker(scene, "spinner", Math::Vec2(300.0f, 300.0f), false);

    k2d::GameObject* members[3];
    for (int i = 0; i < 3; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "ring%d", i);
        members[i] = makeWalker(scene, name, Math::Vec2(260.0f + 20.0f * i, 300.0f), false);
        members[i]->setTag("ring");
        k2d::NavigationAgent2D* agent = members[i]->addComponent<k2d::NavigationAgent2D>();
        agent->setMaxSpeed(120.0f);
        agent->setAutoMove(true);
        k2d::Formation2D* formation = members[i]->addComponent<k2d::Formation2D>();
        formation->setGroupTag("ring");
        formation->setAnchorName("spinner");
        formation->setSpacing(70.0f);
    }

    scene.setSimulationEnabled(true);
    for (int frame = 0; frame < 400; ++frame)
        scene.update(dt);

    // Now spin the anchor on the spot, the way a player turning to aim does.
    // Places pinned to his facing would drag the whole ring round with him.
    Math::Vec2 previous[3];
    for (int i = 0; i < 3; ++i)
        previous[i] = members[i]->globalPosition();
    float travelled = 0.0f;
    for (int frame = 0; frame < 600; ++frame)
    {
        quarry->setRotationDegrees(quarry->rotationDegrees() + 3.0f);
        scene.update(dt);
        for (int i = 0; i < 3; ++i)
        {
            travelled += k2d::Distance(members[i]->globalPosition(), previous[i]);
            previous[i] = members[i]->globalPosition();
        }
    }

    const bool ok = travelled < 20.0f;
    std::printf("navigation surround ignores anchor facing: moved %.1f while it spun 1800 degrees %s\n", travelled,
                ok ? "pass" : "fail");
    return ok;
}

bool TestFormationSpreadsTheGroup()
{
    const float dt = 1.0f / 60.0f;

    // Three chasers converging on one target down the same open field. Without
    // a formation they all path to the same point and arrive in single file.
    const auto run = [&](bool withFormation, float& outClosest)
    {
        k2d::Scene scene;
        makeField(scene, 600.0f);
        k2d::GameObject* quarry = makeWalker(scene, "quarry", Math::Vec2(520.0f, 300.0f), false);

        k2d::GameObject* chasers[3];
        for (int i = 0; i < 3; ++i)
        {
            char name[16];
            std::snprintf(name, sizeof(name), "chaser%d", i);
            chasers[i] = makeWalker(scene, name, Math::Vec2(30.0f, 280.0f + 20.0f * i), false);
            chasers[i]->setTag("hunters");
            k2d::NavigationAgent2D* agent = chasers[i]->addComponent<k2d::NavigationAgent2D>();
            agent->setMaxSpeed(120.0f);
            agent->setAutoMove(true);
            if (withFormation)
            {
                k2d::Formation2D* formation = chasers[i]->addComponent<k2d::Formation2D>();
                formation->setGroupTag("hunters");
                formation->setAnchorName("quarry");
                formation->setShape(k2d::Formation2D::Shape::Surround);
                formation->setSpacing(70.0f);
            }
            else
            {
                agent->setFollowTargetName("quarry");
            }
        }

        scene.setSimulationEnabled(true);
        int samples = 0;
        int inLine = 0;
        outClosest = 1e9f;
        for (int frame = 0; frame < 600; ++frame)
        {
            scene.update(dt);
            const Math::Vec2 a = chasers[0]->globalPosition();
            const Math::Vec2 b = chasers[1]->globalPosition();
            const Math::Vec2 c = chasers[2]->globalPosition();
            const Math::Vec2 q = quarry->globalPosition();
            for (int i = 0; i < 3; ++i)
            {
                const float gap = k2d::Distance(chasers[i]->globalPosition(), q);
                if (gap < outClosest)
                    outClosest = gap;
            }
            // Judge where they settle, not the run-up: they start in a line
            // and share one corridor to get there.
            if (frame < 400 || k2d::Distance(a, q) > 200.0f || k2d::Distance(b, q) > 200.0f ||
                k2d::Distance(c, q) > 200.0f)
                continue;
            ++samples;
            // Twice the triangle area over its longest side: how far the odd one
            // out stands off the line through the other two. Zero is a queue.
            const float twiceArea = std::fabs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
            float longest = k2d::Distance(a, b);
            const float bc = k2d::Distance(b, c);
            const float ac = k2d::Distance(a, c);
            if (bc > longest)
                longest = bc;
            if (ac > longest)
                longest = ac;
            if (longest < 1.0f || twiceArea / longest < 12.0f)
                ++inLine;
        }
        return samples > 0 ? 100.0f * inLine / samples : 100.0f;
    };

    float plainClosest = 0.0f;
    float formedClosest = 0.0f;
    const float plain = run(false, plainClosest);
    const float formed = run(true, formedClosest);

    const bool ok = plain > 50.0f && formed < 5.0f && formedClosest < 90.0f;
    std::printf("navigation formation spreads the group: single file %.0f%% -> %.0f%%, closest %.0f %s\n", plain,
                formed, formedClosest, ok ? "pass" : "fail");
    return ok;
}

bool TestDynamicAgentsDoNotOverlap()
{
    const float dt = 1.0f / 60.0f;
    const float radius = 14.0f;

    // Three agents converging on one spot. A kinematic agent translates and
    // walks through its neighbours; a dynamic one hands the movement to the
    // solver, which is the only thing that can actually keep them apart.
    const auto run = [&](k2d::BodyType type)
    {
        k2d::Scene scene;
        makeField(scene, 400.0f);
        k2d::GameObject* agents[3];
        const Math::Vec2 spawns[3] = {Math::Vec2(20.0f, 20.0f), Math::Vec2(20.0f, 90.0f), Math::Vec2(90.0f, 20.0f)};
        for (int i = 0; i < 3; ++i)
        {
            char name[16];
            std::snprintf(name, sizeof(name), "runner%d", i);
            agents[i] = scene.createObject(name);
            agents[i]->setPosition(spawns[i]);
            k2d::RigidBody2D* body = agents[i]->addComponent<k2d::RigidBody2D>();
            body->setBodyType(type);
            body->setFixedRotation(true);
            body->setLinearDamping(6.0f);
            agents[i]->addComponent<k2d::CircleCollider2D>()->setRadius(radius);
            k2d::NavigationAgent2D* agent = agents[i]->addComponent<k2d::NavigationAgent2D>();
            agent->setMaxSpeed(120.0f);
            agent->setAutoMove(true);
            agent->setTargetPosition(Math::Vec2(340.0f, 340.0f));
        }

        scene.setSimulationEnabled(true);
        float deepest = 0.0f;
        for (int frame = 0; frame < 400; ++frame)
        {
            scene.update(dt);
            if (frame < 60)
                continue;
            for (int a = 0; a < 3; ++a)
                for (int b = a + 1; b < 3; ++b)
                {
                    const float overlap =
                        2.0f * radius - k2d::Distance(agents[a]->globalPosition(), agents[b]->globalPosition());
                    if (overlap > deepest)
                        deepest = overlap;
                }
        }
        return deepest;
    };

    const float kinematic = run(k2d::BodyType::Kinematic);
    const float dynamic = run(k2d::BodyType::Dynamic);
    // The solver settles contacts to within its linear slop rather than to
    // nothing, so the bar is "not visibly inside each other", not zero.
    const bool ok = dynamic < radius * 0.1f && kinematic > radius;
    std::printf("navigation dynamic agents keep apart: deepest overlap kinematic=%.1f dynamic=%.2f %s\n", kinematic,
                dynamic, ok ? "pass" : "fail");
    return ok;
}

bool TestFollowTargetSetAfterResolve()
{
    const float dt = 1.0f / 60.0f;
    k2d::Scene scene;
    makeField(scene, 400.0f);
    k2d::GameObject* prey = makeWalker(scene, "prey", Math::Vec2(360.0f, 300.0f), false);
    k2d::GameObject* hunter = makeWalker(scene, "hunter", Math::Vec2(20.0f, 20.0f), false);
    k2d::NavigationAgent2D* agent = hunter->addComponent<k2d::NavigationAgent2D>();
    agent->setAutoMove(true);
    agent->setMaxSpeed(120.0f);
    agent->setFollowTargetName("prey");

    // One frame so the agent resolves the name and stamps the topology
    // version, then set the same name again the way a script does from
    // on_start. The stamp must not survive, or the agent never paths again.
    scene.update(dt);
    agent->setFollowTargetName("prey");

    // The prey then runs elsewhere. Only a fresh path can reach it, so a stale
    // one carried over from before the name was set does not save the agent.
    prey->setPosition(Math::Vec2(40.0f, 360.0f));
    for (int frame = 0; frame < 400; ++frame)
        scene.update(dt);

    const float remaining = k2d::Distance(hunter->globalPosition(), prey->globalPosition());
    const bool ok = remaining < 20.0f && agent->repathCount() > 1;
    std::printf("navigation follow target after resolve: gap=%.1f repaths=%u %s\n", remaining,
                agent->repathCount(), ok ? "pass" : "fail");
    return ok;
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
                [&]{ k2d::Steering2D* st = walker->addComponent<k2d::Steering2D>();
            st->setSeparationEnabled(true);
            st->setSeparationRadius(60.0f);
            return st; }();
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
    k2d::Steering2D* separation = walker->addComponent<k2d::Steering2D>();
    separation->setSeparationEnabled(true);
    separation->setSeparationRadius(80.0f);

    k2d::GameObject* ghost = makeWalker(scene, "ghost", Math::Vec2(120.0f, 100.0f), false);
    [&]{ k2d::Steering2D* st = ghost->addComponent<k2d::Steering2D>();
            st->setSeparationEnabled(true);
            st->setSeparationRadius(80.0f);
            return st; }();

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
    k2d::Steering2D* avoidance = walker->addComponent<k2d::Steering2D>();
    avoidance->setAvoidanceEnabled(true);
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

    k2d::Steering2D* seek = walker->addComponent<k2d::Steering2D>();
    walker->addComponent<k2d::Steering2D>();
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

    // A target that never stops moving defeats the "has it moved enough"
    // shortcut, so only the interval is left holding the line. One second at a
    // quarter-second interval is a handful of paths, not sixty.
    k2d::GameObject* player = scene.createObject("restless_player");
    player->setPosition(Math::Vec2(180.0f, 180.0f));

    k2d::GameObject* agentObject = scene.createObject("chaser_throttled");
    agentObject->setPosition(Math::Vec2(10.0f, 10.0f));
    k2d::NavigationAgent2D* agent = agentObject->addComponent<k2d::NavigationAgent2D>();
    agent->setRepathInterval(0.25f);
    agent->setFollowTargetName("restless_player");

    for (int i = 0; i < 60; ++i)
    {
        const float wobble = (i % 2) ? 40.0f : -40.0f;
        player->setPosition(Math::Vec2(180.0f + wobble, 180.0f - wobble));
        scene.update(1.0f / 60.0f);
    }

    const uint32_t repaths = agent->repathCount();
    const bool ok = repaths >= 1 && repaths <= 6;
    std::printf("navigation repath throttle: repaths=%u over 60 frames %s\n", static_cast<unsigned>(repaths),
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

// A sensor is a trigger volume, not crowd. Raycast already skips sensors, so
// separation must agree or the two behaviours contradict on the same shape.
bool TestSeparationIgnoresSensors()
{
    k2d::Scene scene;
    k2d::GameObject* trigger = scene.createObject("trigger");
    trigger->setPosition(Math::Vec2(30.0f, 0.0f));
    trigger->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    k2d::BoxCollider2D* sensorShape = trigger->addComponent<k2d::BoxCollider2D>();
    sensorShape->setSize(Math::Vec2(20.0f, 20.0f));
    sensorShape->setSensor(true);

    k2d::GameObject* walker = scene.createObject("sensor_walker");
    walker->setPosition(Math::Vec2(0.0f, 0.0f));
    walker->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    walker->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(10.0f, 10.0f));
    k2d::Steering2D* separation = walker->addComponent<k2d::Steering2D>();
    separation->setSeparationEnabled(true);
    separation->setSeparationRadius(80.0f);

    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    const Math::Vec2 force = scene.steeringForce(*walker, Math::Vec2(0.0f, 0.0f), 1.0f / 60.0f);
    const bool ok = std::fabs(force.x) < 0.0001f && std::fabs(force.y) < 0.0001f;
    std::printf("navigation separation sensors: force=(%.3f, %.3f) %s\n", force.x, force.y, ok ? "pass" : "fail");
    return ok;
}

// A dynamic body used to swallow the agent's movement: pushTransforms skips it
// and pullTransforms writes the body's own position back over the object.
bool TestDynamicBodyAgentMoves()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("walkable_dynamic");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(-300.0f, -300.0f), Math::Vec2(300.0f, -300.0f),
                                  Math::Vec2(300.0f, 300.0f), Math::Vec2(-300.0f, 300.0f)};
    region->setPolygon(polygon, 4);

    k2d::GameObject* walker = scene.createObject("dynamic_walker");
    walker->setPosition(Math::Vec2(-200.0f, 0.0f));
    k2d::RigidBody2D* body = walker->addComponent<k2d::RigidBody2D>();
    body->setBodyType(k2d::BodyType::Dynamic);
    body->setGravityScale(0.0f);
    walker->addComponent<k2d::BoxCollider2D>()->setSize(Math::Vec2(10.0f, 10.0f));

    k2d::NavigationAgent2D* agent = walker->addComponent<k2d::NavigationAgent2D>();
    agent->setMaxSpeed(120.0f);
    agent->setAutoMove(true);
    agent->setTargetPosition(Math::Vec2(200.0f, 0.0f));

    scene.setGravity(Math::Vec2(0.0f, 0.0f));
    scene.setSimulationEnabled(true);
    const float startX = walker->globalPosition().x;
    for (int i = 0; i < 180; ++i)
        scene.update(1.0f / 60.0f);
    const float endX = walker->globalPosition().x;

    const bool ok = endX - startX > 100.0f;
    std::printf("navigation dynamic body: x %.1f -> %.1f %s\n", startX, endX, ok ? "pass" : "fail");
    return ok;
}

// A non-finite dt used to poison Wander2D's accumulated angle for good.
bool TestWanderSurvivesBadDeltaTime()
{
    k2d::Scene scene;
    k2d::GameObject* walker = scene.createObject("wanderer");
    k2d::Steering2D* steering = walker->addComponent<k2d::Steering2D>();

    // The wander angle is the one float that persists between calls, so a
    // single non-finite step would poison it for the rest of the session.
    const float bad = std::numeric_limits<float>::quiet_NaN();
    steering->wander(bad, Math::Vec2(1.0f, 0.0f), 100.0f);

    const Math::Vec2 after = steering->wander(1.0f / 60.0f, Math::Vec2(1.0f, 0.0f), 100.0f);
    const bool ok = std::isfinite(after.x) && std::isfinite(after.y) && after.Length() > 0.0001f;
    std::printf("navigation wander after NaN dt: velocity=(%.3f, %.3f) %s\n", after.x, after.y, ok ? "pass" : "fail");
    return ok;
}

// Triangle centroids are not a path: a straight run across an open region used
// to zigzag through the middle of every triangle it crossed. String pulling
// must collapse that to a single straight segment.
bool TestStraightPathHasNoZigzag()
{
    k2d::Scene scene;
    k2d::GameObject* regionObject = scene.createObject("open_field");
    k2d::NavigationRegion2D* region = regionObject->addComponent<k2d::NavigationRegion2D>();
    ct::Vector<Math::Vec2> outline;
    for (int i = 0; i <= 10; ++i)
        outline.push_back(Math::Vec2(i * 40.0f, 0.0f));
    for (int i = 10; i >= 0; --i)
        outline.push_back(Math::Vec2(i * 40.0f, 120.0f));
    region->setPolygon(outline.data(), (int)outline.size());

    ct::Vector<Math::Vec2> path;
    const bool found =
        k2d::Navigation2D::GetPath(scene, Math::Vec2(10.0f, 60.0f), Math::Vec2(390.0f, 60.0f), path);

    const bool straight = found && path.size() == 2;
    bool onLine = found;
    for (size_t i = 0; i < path.size(); ++i)
        onLine = onLine && std::fabs(path[i].y - 60.0f) < 1.0f;

    std::printf("navigation straight path: waypoints=%d triangles=%d %s\n", (int)path.size(),
                (int)(region->triangles().size() / 3), (straight && onLine) ? "pass" : "fail");
    return straight && onLine;
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
        k2d::Navigation2D::GetPath(scene, Math::Vec2(-1.0f, 0.0f), Math::Vec2(96.0f, 12.0f), path) &&
        region->containsPoint(path[0]);

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
    const bool followAfterResolve = TestFollowTargetSetAfterResolve();
    const bool crowdApart = TestDynamicAgentsDoNotOverlap();
    const bool formationSpread = TestFormationSpreadsTheGroup();
    const bool surroundStable = TestSurroundIgnoresTheAnchorFacing();
    const bool waveSpread = TestSpawnedTogetherDoNotRepathTogether();
    const bool dynamicFacing = TestDynamicAgentFacesItsPath();
    const bool separationSpread = TestSeparationSpreadsACrowd();
    const bool separationSources = TestSeparationOnlySeesBodies();
    const bool obstacleAside = TestObstacleAvoidanceSteersAside();
    const bool steeringList = TestSceneTracksSteeringComponents();

    std::printf("navigation: concave=%s outside_snapped=%s agent=%s triangles=%d waypoints=%d\n", concavePath ? "pass" : "fail",
               outsideRejected ? "pass" : "fail", agentPath ? "pass" : "fail",
               static_cast<int>(region->triangles().size() / 3), static_cast<int>(agent->path().size()));
    const bool straightPath = TestStraightPathHasNoZigzag();
    const bool sensorSeparation = TestSeparationIgnoresSensors();
    const bool dynamicBody = TestDynamicBodyAgentMoves();
    const bool wanderNaN = TestWanderSurvivesBadDeltaTime();
    const bool unreachableThrottle = TestUnreachableTargetIsStillThrottled();
    const bool missingTarget = TestMissingFollowTargetCostsNothing();
    const bool parentedAgent = TestAgentUnderRotatedParent();
    const bool agentRoundTrip = TestAgentTargetPresenceRoundTrip();

    return straightPath && sensorSeparation && dynamicBody && wanderNaN &&
           unreachableThrottle && missingTarget && parentedAgent && agentRoundTrip &&
           concavePath && outsideRejected && agentPath && holeRouting && holeRoundTrip && followTarget &&
                   repathThrottle && touchingHole && selfIntersecting && noRegion && followDestroyed &&
                   outsideMesh && steeringForces && steeringNeutral && followAfterResolve && crowdApart && formationSpread && surroundStable && waveSpread && dynamicFacing && separationSpread &&
           separationSources &&
                   obstacleAside && steeringList
               ? 0
               : 1;
}
