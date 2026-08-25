#include <k2d/Navigation2D.h>
#include <k2d/NavigationAgent2D.h>
#include <k2d/NavigationRegion2D.h>
#include <k2d/Scene.h>

#include <cstdio>

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

    std::printf("navigation: concave=%s outside=%s agent=%s triangles=%d waypoints=%d\n", concavePath ? "pass" : "fail",
                outsideRejected ? "pass" : "fail", agentPath ? "pass" : "fail",
                static_cast<int>(region->triangles().size() / 3), static_cast<int>(agent->path().size()));
    return concavePath && outsideRejected && agentPath ? 0 : 1;
}
