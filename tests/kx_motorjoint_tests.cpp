#include <kx/kx.h>

#include <cmath>
#include <cstdio>

static int gFailures = 0;

static void Check(bool condition, const char *name)
{
    if (condition)
        std::printf("[PASS] %s\n", name);
    else
    {
        std::printf("[FAIL] %s\n", name);
        ++gFailures;
    }
}

static void TestReachesLinearAndAngularOffset()
{
    kx::World world(Math::Vec2(0.0f));
    kx::Body *ground = world.CreateBody(kx::BodyType::Static, Math::Vec2(0.0f));
    kx::Body *body = world.CreateBox(Math::Vec2(0.0f), 10.0f, 10.0f, 1.0f);
    kx::MotorJoint *joint = new kx::MotorJoint(ground, body);
    joint->SetLinearOffset(Math::Vec2(100.0f, -50.0f));
    joint->SetAngularOffset(0.75f);
    joint->SetMaxForce(1.0e8f);
    joint->SetMaxTorque(1.0e10f);
    world.AddJoint(joint);

    for (int i = 0; i < 180; ++i)
        world.Step(1.0f / 60.0f);

    Check((body->Position() - Math::Vec2(100.0f, -50.0f)).Length() < 1.0f,
          "MotorJoint converge para o offset linear");
    Check(std::fabs(body->Angle() - 0.75f) < 0.02f,
          "MotorJoint converge para o offset angular");
}

static void TestTracksMovingTarget()
{
    kx::World world(Math::Vec2(0.0f));
    kx::Body *ground = world.CreateBody(kx::BodyType::Static, Math::Vec2(0.0f));
    kx::Body *body = world.CreateBox(Math::Vec2(0.0f), 10.0f, 10.0f, 1.0f);
    kx::MotorJoint *joint = new kx::MotorJoint(ground, body);
    joint->SetMaxForce(1.0e8f);
    joint->SetMaxTorque(1.0e10f);
    world.AddJoint(joint);

    Math::Vec2 target(0.0f);
    float angle = 0.0f;
    for (int i = 0; i < 240; ++i)
    {
        float time = i / 60.0f;
        target = Math::Vec2(80.0f * std::sin(time), 40.0f * std::cos(0.5f * time));
        angle = 0.5f * std::sin(0.75f * time);
        joint->SetLinearOffset(target);
        joint->SetAngularOffset(angle);
        world.Step(1.0f / 60.0f);
    }

    Check((body->Position() - target).Length() < 5.0f,
          "MotorJoint acompanha alvo linear em movimento");
    Check(std::fabs(body->Angle() - angle) < 0.08f,
          "MotorJoint acompanha alvo angular em movimento");
}

static void TestMaxForceLimitsAcceleration()
{
    kx::World world(Math::Vec2(0.0f));
    kx::Body *ground = world.CreateBody(kx::BodyType::Static, Math::Vec2(0.0f));
    kx::Body *body = world.CreateBox(Math::Vec2(0.0f), 10.0f, 10.0f, 1.0f);
    kx::MotorJoint *joint = new kx::MotorJoint(ground, body);
    joint->SetLinearOffset(Math::Vec2(1000.0f, 0.0f));
    joint->SetMaxForce(100.0f);
    joint->SetMaxTorque(0.0f);
    world.AddJoint(joint);

    for (int i = 0; i < 60; ++i)
        world.Step(1.0f / 60.0f);

    Check(body->Velocity().x > 0.0f && body->Velocity().x < 0.35f,
          "MotorJoint respeita o limite de forca");
}

int main()
{
    TestReachesLinearAndAngularOffset();
    TestTracksMovingTarget();
    TestMaxForceLimitsAcceleration();
    std::printf("%d failures\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}