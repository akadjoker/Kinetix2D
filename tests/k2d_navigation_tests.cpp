#include <k2d/Geometry2D.h>
#include <k2d/Navigation2D.h>
#include <k2d/NavigationAgent2D.h>
#include <k2d/NavigationRegion2D.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>

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
} // namespace

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

    std::printf("navigation: concave=%s outside=%s agent=%s triangles=%d waypoints=%d\n", concavePath ? "pass" : "fail",
               outsideRejected ? "pass" : "fail", agentPath ? "pass" : "fail",
               static_cast<int>(region->triangles().size() / 3), static_cast<int>(agent->path().size()));
    return concavePath && outsideRejected && agentPath && holeRouting && holeRoundTrip && followTarget &&
                   repathThrottle
               ? 0
               : 1;
}
