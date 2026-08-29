#include <k2d/RigidBody2D.h>
#include <k2d/ChainCollider2D.h>
#include <k2d/GameObject.h>
#include <k2d/Scene.h>

#include <cmath>
#include <cstdio>

namespace
{
    constexpr float kDegToRad = 0.01745329251f;

    bool Near(float a, float b, float epsilon)
    {
        return std::fabs(a - b) < epsilon;
    }

    struct ViewportTransform
    {
        Math::Vec2 pan;
        float zoom;

        Math::Vec2 worldToScreen(const Math::Vec2 &origin, float x, float y) const
        {
            return Math::Vec2(origin.x + pan.x + x * zoom, origin.y + pan.y + y * zoom);
        }

        Math::Vec2 screenToWorld(const Math::Vec2 &origin, const Math::Vec2 &screen) const
        {
            return Math::Vec2((screen.x - origin.x - pan.x) / zoom, (screen.y - origin.y - pan.y) / zoom);
        }
    };

    Math::Vec2 objectLocalToScreen(k2d::GameObject &object, const ViewportTransform &viewport,
                                   const Math::Vec2 &origin, const Math::Vec2 &local)
    {
        const Math::Vec2 world = object.globalPosition();
        const Math::Vec2 scale = object.scale();
        const float scaleX = std::fabs(scale.x) > 0.0001f ? std::fabs(scale.x) : 1.0f;
        const float scaleY = std::fabs(scale.y) > 0.0001f ? std::fabs(scale.y) : 1.0f;
        const float angle = object.rotationDegrees() * kDegToRad;
        const float cosA = std::cos(angle);
        const float sinA = std::sin(angle);
        const float sx = local.x * scaleX;
        const float sy = local.y * scaleY;
        return viewport.worldToScreen(origin, world.x + sx * cosA - sy * sinA, world.y + sx * sinA + sy * cosA);
    }

    Math::Vec2 screenToObjectLocal(k2d::GameObject &object, const ViewportTransform &viewport,
                                   const Math::Vec2 &origin, const Math::Vec2 &screen)
    {
        const Math::Vec2 world = viewport.screenToWorld(origin, screen);
        const Math::Vec2 objectWorld = object.globalPosition();
        const float dx = world.x - objectWorld.x;
        const float dy = world.y - objectWorld.y;
        const float angle = object.rotationDegrees() * kDegToRad;
        const float cosA = std::cos(angle);
        const float sinA = std::sin(angle);
        const float rx = dx * cosA + dy * sinA;
        const float ry = -dx * sinA + dy * cosA;
        const Math::Vec2 scale = object.scale();
        const float scaleX = std::fabs(scale.x) > 0.0001f ? std::fabs(scale.x) : 1.0f;
        const float scaleY = std::fabs(scale.y) > 0.0001f ? std::fabs(scale.y) : 1.0f;
        return Math::Vec2(rx / scaleX, ry / scaleY);
    }
}

static bool testBuildShapesWithoutSimulation()
{
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("shore");
    k2d::RigidBody2D* body = object->addComponent<k2d::RigidBody2D>();
    body->setBodyType(k2d::BodyType::Static);

    const Math::Vec2 points[4] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 0.0f), Math::Vec2(40.0f, 40.0f),
                                  Math::Vec2(0.0f, 40.0f)};
    k2d::ChainCollider2D* chain = object->addComponent<k2d::ChainCollider2D>();
    chain->setPoints(points, 4);
    chain->setLoop(true);

    const int before = body->ShapeCount();
    scene.buildBodyShapes(*body);
    const int after = body->ShapeCount();

    std::printf("  build_without_play: shapes %d -> %d (simulation off)\n", before, after);
    return before == 0 && after == 4 && !scene.simulationEnabled();
}

int main()
{
    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("collider_owner");
    object->setPosition(Math::Vec2(100.0f, -50.0f));
    object->setRotationDegrees(30.0f);
    object->setScale(Math::Vec2(2.0f, 3.0f));

    k2d::ChainCollider2D *chain = object->addComponent<k2d::ChainCollider2D>();
    chain->setOffset(Math::Vec2(1.5f, -2.0f));
    const Math::Vec2 points[4] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(12.5f, -6.25f), Math::Vec2(-4.0f, 8.0f),
                                  Math::Vec2(3.25f, 3.25f)};
    chain->setPoints(points, 4);
    chain->setLoop(true);

    const ViewportTransform viewport{Math::Vec2(40.0f, -15.0f), 1.35f};
    const Math::Vec2 origin(300.0f, 200.0f);

    bool allRoundTrip = true;
    for (size_t i = 0; i < chain->points().size(); ++i)
    {
        const Math::Vec2 originalPoint = chain->points()[i];
        const Math::Vec2 offset = chain->offset();
        const Math::Vec2 withOffset(originalPoint.x + offset.x, originalPoint.y + offset.y);

        const Math::Vec2 screen = objectLocalToScreen(*object, viewport, origin, withOffset);
        const Math::Vec2 recoveredWithOffset = screenToObjectLocal(*object, viewport, origin, screen);
        const Math::Vec2 recoveredPoint(recoveredWithOffset.x - offset.x, recoveredWithOffset.y - offset.y);

        const bool roundTrip = Near(recoveredPoint.x, originalPoint.x, 0.001f) &&
                               Near(recoveredPoint.y, originalPoint.y, 0.001f);
        if (!roundTrip)
        {
            std::printf("collider_points: point %zu failed round-trip: original=(%f,%f) recovered=(%f,%f)\n", i,
                       originalPoint.x, originalPoint.y, recoveredPoint.x, recoveredPoint.y);
        }
        allRoundTrip = allRoundTrip && roundTrip;
    }

    const bool built = testBuildShapesWithoutSimulation();

    std::printf("collider_points: round_trip=%s build_without_play=%s\n", allRoundTrip ? "pass" : "fail",
                built ? "pass" : "fail");
    return allRoundTrip && built ? 0 : 1;
}
