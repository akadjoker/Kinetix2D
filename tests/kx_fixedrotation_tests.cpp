
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

static void TestFixedRotationZerosInvI()
{
    kx::World world(glm::vec2(0.0f, -500.0f));
    kx::Body *body = world.CreateBox(glm::vec2(0.0f, 0.0f), 10.0f, 10.0f, 1.0f);
    Check(body->InvI() > 0.0f, "dinamico normal tem mInvI > 0");

    body->SetFixedRotation(true);
    Check(body->FixedRotation(), "FixedRotation() reflete o flag");
    Check(std::fabs(body->InvI()) < 1.0e-6f, "fixed rotation => mInvI = 0");
    Check(body->InvMass() > 0.0f, "massa linear mantem-se (mInvMass > 0)");
}

static void TestOffCenterImpulseDoesNotSpin()
{
    kx::World world(glm::vec2(0.0f));
    kx::Body *body = world.CreateBox(glm::vec2(0.0f, 0.0f), 10.0f, 10.0f, 1.0f);
    body->SetFixedRotation(true);

    body->ApplyImpulse(glm::vec2(0.0f, 10000.0f), body->WorldCenter() + glm::vec2(10.0f, 0.0f));
    Check(std::fabs(body->AngularVelocity()) < 1.0e-6f,
          "impulso descentrado nao gira corpo fixed-rotation");
    Check(body->Velocity().y > 0.0f, "impulso descentrado ainda da velocidade linear");
}

static void TestFallsAndLandsUpright()
{
    kx::World world(glm::vec2(0.0f, -500.0f));
    kx::Body *ground = world.CreateStaticBox(glm::vec2(0.0f, -60.0f), 200.0f, 5.0f);
    kx::Body *body = world.CreateBox(glm::vec2(0.0f, 0.0f), 5.0f, 5.0f, 1.0f);
    body->SetFixedRotation(true);

    for (int i = 0; i < 180; ++i)
        world.Step(1.0f / 60.0f);

    Check(body->Position().y < -40.0f, "corpo fixed-rotation cai e assenta no chao");
    Check(std::fabs(body->Angle()) < 1.0e-5f, "angulo permanece 0");
    Check(std::fabs(body->AngularVelocity()) < 1.0e-6f, "velocidade angular permanece 0");
}

int main()
{
    TestFixedRotationZerosInvI();
    TestOffCenterImpulseDoesNotSpin();
    TestFallsAndLandsUpright();
    std::printf("%d failures\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}