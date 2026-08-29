#include <k2d/RigidBody2D.h>
#include <k2d/ChainCollider2D.h>
#include <k2d/GameObject.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>
#include <k2d/SpriteBatch.h>

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

static bool testSpriteBatchEntryEditing()
{
    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("scatter_owner");
    object->setPosition(Math::Vec2(100.0f, -50.0f));
    object->setRotationDegrees(30.0f);
    object->setScale(Math::Vec2(2.0f, 3.0f));

    k2d::SpriteBatch *batch = object->addComponent<k2d::SpriteBatch>();
    const int first = batch->add(nullptr, Math::Vec2(4.0f, -6.0f), Math::Vec2(32.0f, 48.0f), 0xFFEECC11u);
    batch->setSource(first, Math::Vec4(0.0f, 0.0f, 32.0f, 48.0f));
    const int second = batch->add(nullptr, Math::Vec2(-12.5f, 20.0f), Math::Vec2(16.0f, 16.0f));
    batch->setSource(second, Math::Vec4(16.0f, 0.0f, 16.0f, 16.0f));

    const ViewportTransform viewport{Math::Vec2(-25.0f, 60.0f), 0.8f};
    const Math::Vec2 origin(320.0f, 180.0f);

    bool allRoundTrip = true;
    for (int i = 0; i < batch->count(); ++i)
    {
        const k2d::SpriteBatch::Entry *entry = batch->entry(i);
        const Math::Vec2 centre(entry->position.x + entry->size.x * 0.5f, entry->position.y + entry->size.y * 0.5f);

        const Math::Vec2 screen = objectLocalToScreen(*object, viewport, origin, centre);
        const Math::Vec2 recovered = screenToObjectLocal(*object, viewport, origin, screen);

        const bool roundTrip = Near(recovered.x, centre.x, 0.001f) && Near(recovered.y, centre.y, 0.001f);
        if (!roundTrip)
        {
            std::printf("spritebatch_entries: entry %d failed round-trip: centre=(%f,%f) recovered=(%f,%f)\n", i,
                       centre.x, centre.y, recovered.x, recovered.y);
        }
        allRoundTrip = allRoundTrip && roundTrip;
    }

    const ct::Json written = k2d::Serializer::WriteObject(*object);
    k2d::Scene copyScene;
    k2d::GameObject *copy = k2d::Serializer::ReadObject(copyScene, written);
    k2d::SpriteBatch *copyBatch = copy ? copy->getComponent<k2d::SpriteBatch>() : nullptr;

    bool serialized = copyBatch != nullptr && copyBatch->count() == batch->count();
    for (int i = 0; serialized && i < batch->count(); ++i)
    {
        const k2d::SpriteBatch::Entry *src = batch->entry(i);
        const k2d::SpriteBatch::Entry *dst = copyBatch->entry(i);
        serialized = serialized && dst != nullptr &&
                     Near(src->position.x, dst->position.x, 0.001f) && Near(src->position.y, dst->position.y, 0.001f) &&
                     Near(src->size.x, dst->size.x, 0.001f) && Near(src->size.y, dst->size.y, 0.001f) &&
                     Near(src->source.x, dst->source.x, 0.001f) && Near(src->source.y, dst->source.y, 0.001f) &&
                     Near(src->source.z, dst->source.z, 0.001f) && Near(src->source.w, dst->source.w, 0.001f) &&
                     src->color.Packed() == dst->color.Packed();
    }
    if (!serialized)
        std::printf("spritebatch_entries: serializer round-trip failed\n");

    std::printf("spritebatch_entries: round_trip=%s serialize=%s\n", allRoundTrip ? "pass" : "fail",
               serialized ? "pass" : "fail");
    return allRoundTrip && serialized;
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
    const bool spriteBatch = testSpriteBatchEntryEditing();

    std::printf("collider_points: round_trip=%s build_without_play=%s\n", allRoundTrip ? "pass" : "fail",
                built ? "pass" : "fail");
    return allRoundTrip && built && spriteBatch ? 0 : 1;
}
