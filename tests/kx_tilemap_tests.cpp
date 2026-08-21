#include <kx/kx.h>

#include <cstdio>

int main()
{
    kx::World world(glm::vec2(0.0f, 100.0f));
    kx::TileMapCollider collider(world);
    collider.SetMapSize(8, 4);
    collider.SetCellSize(glm::vec2(10.0f, 10.0f));
    for (int x = 0; x < 8; ++x)
        collider.SetSolid(x, 3, true);
    collider.Rebuild();

    bool merged = collider.Bodies().size() == 1 &&
                  collider.Bodies()[0]->ShapeCount() == 1;

    kx::Body *body = world.CreateCircle(glm::vec2(35.0f, 10.0f), 4.0f, 1.0f);
    for (int i = 0; i < 120; ++i)
        world.Step(1.0f / 60.0f);
    bool collision = body->Position().y < 30.0f && world.ContactCount() > 0;

    collider.SetSolid(0, 0, true);
    collider.Rebuild();
    bool rebuild = collider.Bodies().size() == 1 &&
                   collider.Bodies()[0]->ShapeCount() == 2;

    std::printf("tilemap: merged=%s collision=%s rebuild=%s\n",
                merged ? "pass" : "fail", collision ? "pass" : "fail",
                rebuild ? "pass" : "fail");
    return merged && collision && rebuild ? 0 : 1;
}
