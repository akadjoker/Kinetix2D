#include <kx/kx.h>

#include <cstdio>

int main()
{
    kx::World world(Math::Vec2(0.0f));
    kx::Body *body = world.CreateCircle(Math::Vec2(0.0f), 5.0f, 1.0f);

    for (int i = 0; i < 30; ++i)
        world.Step(1.0f / 60.0f);
    bool slept = !body->IsAwake();

    body->ApplyImpulse(Math::Vec2(10.0f, 0.0f), body->WorldCenter());
    bool woke = body->IsAwake() && body->Velocity().x > 0.0f;

    for (int i = 0; i < 30; ++i)
        world.Step(1.0f / 60.0f);
    bool sleptAgain = !body->IsAwake();

    std::printf("slept=%s woke=%s slept_again=%s\n",
                slept ? "pass" : "fail", woke ? "pass" : "fail",
                sleptAgain ? "pass" : "fail");
    return slept && woke && sleptAgain ? 0 : 1;
}