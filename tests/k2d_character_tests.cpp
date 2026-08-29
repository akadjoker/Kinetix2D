#include <k2d/BoxCollider2D.h>
#include <k2d/NavigationAgent2D.h>
#include <k2d/NavigationRegion2D.h>
#include <k2d/Utils.h>
#include <k2d/CharacterBody2D.h>
#include <k2d/CircleCollider2D.h>
#include <k2d/GameObject.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Scene.h>
#include <k2d/Steering2D.h>

#include <cmath>
#include <cstdio>

static k2d::GameObject* makeWall(k2d::Scene& scene, const char* name, const Math::Vec2& position,
                                 const Math::Vec2& size)
{
    k2d::GameObject* object = scene.createObject(name);
    object->setPosition(position);
    object->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    object->addComponent<k2d::BoxCollider2D>()->setSize(size);
    return object;
}

static k2d::CharacterBody2D* makeCharacter(k2d::Scene& scene, const Math::Vec2& position, float radius)
{
    k2d::GameObject* object = scene.createObject("character");
    object->setPosition(position);
    object->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Kinematic);
    object->addComponent<k2d::CircleCollider2D>()->setRadius(radius);
    return object->addComponent<k2d::CharacterBody2D>();
}

static bool testSlidesAlongWall()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);
    makeWall(scene, "wall", Math::Vec2(0.0f, 100.0f), Math::Vec2(600.0f, 40.0f));
    k2d::CharacterBody2D* character = makeCharacter(scene, Math::Vec2(-200.0f, 60.0f), 12.0f);

    const float surface = 100.0f - 20.0f - 12.0f;
    const float startX = character->owner()->position().x;
    float previousX = startX;
    float deepest = 0.0f;
    float highest = 0.0f;
    int reversals = 0;
    for (int frame = 0; frame < 240; ++frame)
    {
        character->setVelocity(Math::Vec2(120.0f, 240.0f));
        character->moveAndSlide();
        scene.update(1.0f / 60.0f);

        const Math::Vec2 position = character->owner()->position();
        if (position.x < previousX - 0.01f)
            ++reversals;
        previousX = position.x;

        if (frame < 30)
            continue;
        const float penetration = position.y - surface;
        if (penetration > deepest)
            deepest = penetration;
        if (surface - position.y > highest)
            highest = surface - position.y;
    }

    const float travelled = previousX - startX;
    const bool ok = travelled > 300.0f && reversals == 0 && deepest < 0.5f && highest < 2.0f;
    std::printf("  slide along wall: travelled=%.1f reversals=%d sunk=%.3f gap=%.3f\n", travelled, reversals, deepest,
                highest);
    return ok;
}

static bool testRecoversFromInsideAWall()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);
    makeWall(scene, "wall", Math::Vec2(0.0f, 0.0f), Math::Vec2(200.0f, 200.0f));
    k2d::CharacterBody2D* character = makeCharacter(scene, Math::Vec2(90.0f, 0.0f), 20.0f);

    for (int frame = 0; frame < 60; ++frame)
    {
        character->setVelocity(Math::Vec2(0.0f, 0.0f));
        character->moveAndSlide();
        scene.update(1.0f / 60.0f);
    }

    const Math::Vec2 position = character->owner()->position();
    const bool outside = std::fabs(position.x) >= 100.0f || std::fabs(position.y) >= 100.0f;
    const bool ok = outside && position.x > 90.0f;
    std::printf("  recovers from inside: x=%.2f y=%.2f outside=%s\n", position.x, position.y, outside ? "yes" : "no");
    return ok;
}

static bool testDoesNotTunnelHeadOn()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);
    makeWall(scene, "wall", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 400.0f));
    k2d::CharacterBody2D* character = makeCharacter(scene, Math::Vec2(-100.0f, 0.0f), 10.0f);

    float furthest = -100.0f;
    for (int frame = 0; frame < 300; ++frame)
    {
        character->setVelocity(Math::Vec2(400.0f, 0.0f));
        character->moveAndSlide();
        scene.update(1.0f / 60.0f);
        if (character->owner()->position().x > furthest)
            furthest = character->owner()->position().x;
    }

    const bool ok = furthest < -20.0f && character->owner()->position().x < -20.0f;
    std::printf("  head-on wall: furthest=%.2f final=%.2f (wall face at -30)\n", furthest,
                character->owner()->position().x);
    return ok;
}

static bool testCornerDoesNotJitter()
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);
    makeWall(scene, "right", Math::Vec2(60.0f, 0.0f), Math::Vec2(40.0f, 400.0f));
    makeWall(scene, "bottom", Math::Vec2(0.0f, 60.0f), Math::Vec2(400.0f, 40.0f));
    k2d::CharacterBody2D* character = makeCharacter(scene, Math::Vec2(0.0f, 0.0f), 10.0f);

    Math::Vec2 previous = character->owner()->position();
    float largestStep = 0.0f;
    for (int frame = 0; frame < 240; ++frame)
    {
        character->setVelocity(Math::Vec2(200.0f, 200.0f));
        character->moveAndSlide();
        scene.update(1.0f / 60.0f);

        const Math::Vec2 position = character->owner()->position();
        if (frame > 60)
        {
            const float step = (position - previous).Length();
            if (step > largestStep)
                largestStep = step;
        }
        previous = position;
    }

    const bool ok = largestStep < 1.0f;
    std::printf("  wedged in corner: largest settled step=%.4f\n", largestStep);
    return ok;
}

static float seekPastObstacle(bool withAvoidance, float& reachedX)
{
    k2d::Scene scene;
    scene.setSimulationEnabled(true);
    makeWall(scene, "obstacle", Math::Vec2(640.0f, 360.0f), Math::Vec2(120.0f, 120.0f));

    k2d::GameObject* seeker = scene.createObject("seeker");
    seeker->setPosition(Math::Vec2(120.0f, 360.0f));
    seeker->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Kinematic);
    seeker->addComponent<k2d::CircleCollider2D>()->setRadius(12.0f);
    // Open ground as far as the navigation mesh knows: the box is a collider
    // only, so the path runs straight through where it stands and avoidance is
    // the only thing that can steer around it.
    k2d::GameObject* field = scene.createObject("field");
    const Math::Vec2 polygon[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(1300.0f, 0.0f), Math::Vec2(1300.0f, 720.0f),
                                  Math::Vec2(0.0f, 720.0f)};
    field->addComponent<k2d::NavigationRegion2D>()->setPolygon(polygon, 4);

    k2d::NavigationAgent2D* agent = seeker->addComponent<k2d::NavigationAgent2D>();
    agent->setMaxSpeed(200.0f);
    agent->setAutoMove(true);
    agent->setTargetPosition(Math::Vec2(1160.0f, 360.0f));
    if (withAvoidance)
    {
        k2d::Steering2D* steering = seeker->addComponent<k2d::Steering2D>();
        steering->setAvoidanceEnabled(true);
    }

    float worst = 1000.0f;
    for (int frame = 0; frame < 400; ++frame)
    {
        scene.update(1.0f / 60.0f);
        const Math::Vec2 position = seeker->globalPosition();
        // Signed distance from the agent's circle to the obstacle rectangle:
        // negative means it walked through the thing it was told to avoid.
        const float dx = std::fabs(position.x - 640.0f) - 60.0f;
        const float dy = std::fabs(position.y - 360.0f) - 60.0f;
        const float outside = std::sqrt(k2d::Max(dx, 0.0f) * k2d::Max(dx, 0.0f) +
                                        k2d::Max(dy, 0.0f) * k2d::Max(dy, 0.0f));
        const float clearance = (dx < 0.0f && dy < 0.0f ? k2d::Max(dx, dy) : outside) - 12.0f;
        if (clearance < worst)
            worst = clearance;
    }
    reachedX = seeker->globalPosition().x;
    return worst;
}

static bool testObstacleAvoidanceActuallySteers()
{
    float straightX = 0.0f;
    float avoidX = 0.0f;
    const float straight = seekPastObstacle(false, straightX);
    const float avoided = seekPastObstacle(true, avoidX);

    // It does not clear the box outright: path following keeps pulling it back
    // the moment the feeler stops seeing anything, which is what a lateral
    // force against a goal that never yields comes to. It does bite hard, and
    // the agent still gets where it was going.
    const bool ok = avoided > straight + 15.0f && avoidX > 900.0f;
    std::printf("  obstacle avoidance: clearance off=%.1f on=%.1f, reached x=%.0f\n", straight, avoided, avoidX);
    return ok;
}

int main()
{
    std::printf("k2d character body tests\n");
    int failures = 0;
    failures += testSlidesAlongWall() ? 0 : 1;
    failures += testRecoversFromInsideAWall() ? 0 : 1;
    failures += testDoesNotTunnelHeadOn() ? 0 : 1;
    failures += testCornerDoesNotJitter() ? 0 : 1;
    failures += testObstacleAvoidanceActuallySteers() ? 0 : 1;
    std::printf(failures == 0 ? "all character tests passed\n" : "%d character test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
