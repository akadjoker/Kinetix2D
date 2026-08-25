#include <kx/body.h>
#include <kx/shapecast.h>
#include <kx/world.h>

#include <cmath>
#include <cstdio>

namespace
{
bool near(float a, float b, float epsilon = 0.01f)
{
    return std::fabs(a - b) <= epsilon;
}
} // namespace

int main()
{
    kx::Shape wall;
    wall.type = kx::ShapeType::Polygon;
    wall.polygon.SetAsBox(10.0f, 30.0f);

    kx::Shape mover;
    mover.type = kx::ShapeType::Polygon;
    mover.polygon.SetAsBox(5.0f, 5.0f);

    kx::ShapeCastInput input;
    input.shapeA = &wall;
    input.transformA = kx::MakeTransform(Math::Vec2(0.0f, 0.0f), 0.0f);
    input.shapeB = &mover;
    input.transformB = kx::MakeTransform(Math::Vec2(-50.0f, 0.0f), 0.0f);
    input.translationB = Math::Vec2(100.0f, 0.0f);

    kx::ShapeCastOutput hit;
    const bool wallHit = kx::ShapeCast(input, hit);
    const bool wallCorrect = wallHit && near(hit.fraction, 0.345f, 0.015f) && hit.normal.x < -0.99f;

    input.transformB = kx::MakeTransform(Math::Vec2(-50.0f, 50.0f), 0.0f);
    kx::ShapeCastOutput miss;
    const bool missesWall = !kx::ShapeCast(input, miss);

    kx::Shape circle;
    circle.type = kx::ShapeType::Circle;
    circle.circle.center = Math::Vec2(0.0f, 0.0f);
    circle.circle.radius = 4.0f;
    input.shapeB = &circle;
    input.transformB = kx::MakeTransform(Math::Vec2(-50.0f, 0.0f), 0.0f);
    kx::ShapeCastOutput circleHit;
    const bool circleCorrect =
        kx::ShapeCast(input, circleHit) && near(circleHit.fraction, 0.355f, 0.015f) && circleHit.normal.x < -0.99f;

    kx::World world(Math::Vec2(0.0f, 0.0f));
    kx::Body* staticWall = world.CreateStaticBox(Math::Vec2(0.0f, 0.0f), 10.0f, 30.0f);
    kx::Body* character = world.CreateKinematicBox(Math::Vec2(-50.0f, 0.0f), 5.0f, 5.0f);
    (void)staticWall;
    kx::MotionResult motion;
    const bool motionHit = world.TestMotion(*character, Math::Vec2(100.0f, 0.0f), motion);
    const bool motionCorrect = motionHit && motion.body == staticWall && motion.normal.x < -0.99f &&
                               motion.travel.x > 33.0f && motion.travel.x < 35.0f && motion.remainder.x > 65.0f;

    kx::MotionResult overlap;
    const bool positionBlocked =
        world.TestPosition(*character, Math::Vec2(0.0f, 0.0f), overlap) && overlap.body == staticWall;
    const bool positionFree = !world.TestPosition(*character, Math::Vec2(-50.0f, 0.0f), overlap);

    std::printf("shapecast: wall=%s miss=%s circle=%s motion=%s position=%s\n", wallCorrect ? "pass" : "fail",
                missesWall ? "pass" : "fail", circleCorrect ? "pass" : "fail", motionCorrect ? "pass" : "fail",
                (positionBlocked && positionFree) ? "pass" : "fail");
    return wallCorrect && missesWall && circleCorrect && motionCorrect && positionBlocked && positionFree ? 0 : 1;
}
