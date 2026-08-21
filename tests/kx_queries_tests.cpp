#include <kx/kx.h>

#include <cmath>
#include <cstdio>

static int failures = 0;

static void Check(bool condition, const char *name)
{
    if (!condition)
    {
        std::printf("[FAIL] %s\n", name);
        ++failures;
    }
    else
        std::printf("[PASS] %s\n", name);
}

int main()
{
    kx::World world(glm::vec2(0.0f));
    kx::Body *dynamic = world.CreateCircle(glm::vec2(10.0f, 0.0f), 5.0f, 1.0f);
    kx::Body *staticBody = world.CreateStaticBox(glm::vec2(100.0f, 0.0f), 5.0f, 5.0f);
    kx::Body *kinematic = world.CreateKinematicBox(glm::vec2(-100.0f, 0.0f), 5.0f, 5.0f);

    ct::Vector<kx::Body *> hits;
    kx::AABB box;
    box.lowerBound = glm::vec2(0.0f, -8.0f);
    box.upperBound = glm::vec2(20.0f, 8.0f);
    world.QueryAABB(box, hits);
    Check(hits.size() == 1 && hits[0] == dynamic, "QueryAABB returns overlapping body");

    world.QueryCircle(glm::vec2(0.0f), 20.0f, hits);
    Check(hits.size() == 1 && hits[0] == dynamic, "QueryCircle returns body in radius");

    kx::Explode(world, glm::vec2(0.0f), 20.0f, 100.0f, 1.0f);
    Check(dynamic->Velocity().x > 0.0f, "Explode applies impulse away from center");
    Check(std::fabs(staticBody->Velocity().x) < 0.0001f &&
              std::fabs(kinematic->Velocity().x) < 0.0001f,
          "Explode does not move static or kinematic bodies");

    std::printf("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
