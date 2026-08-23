#include <kx/kx.h>

#include <cstdio>
#include <cmath>

static int gFailures = 0;

#define CHECK(cond, name)                                          \
    do                                                             \
    {                                                              \
        if (cond)                                                  \
        {                                                          \
            std::printf("[PASS] %s\n", name);                      \
        }                                                          \
        else                                                       \
        {                                                          \
            std::printf("[FAIL] %s (linha %d)\n", name, __LINE__); \
            ++gFailures;                                           \
        }                                                          \
    } while (0)

static void BuildScene(kx::World &world, kx::Body *out[12])
{
    world.CreateStaticBox(glm::vec2(0.0f, 300.0f), 600.0f, 10.0f);
    int n = 0;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 3; ++row)
            out[n++] = world.CreateBox(glm::vec2(-150.0f + col * 100.0f, 260.0f - row * 45.0f),
                                       18.0f, 18.0f, 1.0f);
}

static void TestTreeMatchesBrute()
{
    kx::World treeWorld(glm::vec2(0.0f, 500.0f));
    kx::World bruteWorld(glm::vec2(0.0f, 500.0f));
    treeWorld.SetTreeBroadphase(true);
    bruteWorld.SetTreeBroadphase(false);

    kx::Body *ta[12];
    kx::Body *ba[12];
    BuildScene(treeWorld, ta);
    BuildScene(bruteWorld, ba);

    bool contactsMatch = true;
    for (int s = 0; s < 300; ++s)
    {
        treeWorld.Step(1.0f / 60.0f);
        bruteWorld.Step(1.0f / 60.0f);
        if (treeWorld.ContactCount() != bruteWorld.ContactCount())
            contactsMatch = false;
    }
    CHECK(contactsMatch, "tree e brute produzem o mesmo numero de contactos em 300 steps");

    bool positionsMatch = true;
    for (int i = 0; i < 12; ++i)
    {
        glm::vec2 d = ta[i]->Position() - ba[i]->Position();
        if (glm::length(d) > 2.0f)
            positionsMatch = false;
    }
    CHECK(positionsMatch, "posicoes finais equivalentes tree vs brute");
}

static void TestRestingStackKeepsContacts()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 300.0f), 600.0f, 10.0f);
    for (int i = 0; i < 5; ++i)
        world.CreateBox(glm::vec2(0.0f, 260.0f - i * 45.0f), 18.0f, 18.0f, 1.0f);

    for (int s = 0; s < 600; ++s)
        world.Step(1.0f / 60.0f);

    CHECK(world.ContactCount() >= 5, "pilha em repouso continua com contactos (bug do wip)");

    float maxSpeed = 0.0f;
    for (size_t i = 0; i < world.Bodies().size(); ++i)
    {
        float sp = glm::length(world.Bodies()[i]->Velocity());
        if (sp > maxSpeed)
            maxSpeed = sp;
    }
    CHECK(maxSpeed < 12.0f, "pilha em repouso estavel com tree");
}

static void TestTeleportLosesAndRegainsContacts()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 300.0f), 600.0f, 10.0f);
    kx::Body *box = world.CreateBox(glm::vec2(0.0f, 260.0f), 18.0f, 18.0f, 1.0f);

    for (int s = 0; s < 180; ++s)
        world.Step(1.0f / 60.0f);
    CHECK(world.ContactCount() > 0, "assente tem contacto");

    box->SetPosition(glm::vec2(5000.0f, 0.0f));
    box->SetVelocity(glm::vec2(0.0f, 0.0f));
    world.Step(1.0f / 60.0f);
    world.Step(1.0f / 60.0f);
    CHECK(world.ContactCount() == 0, "teleportado para longe perde contactos");

    box->SetPosition(glm::vec2(0.0f, 260.0f));
    box->SetVelocity(glm::vec2(0.0f, 0.0f));
    for (int s = 0; s < 120; ++s)
        world.Step(1.0f / 60.0f);
    CHECK(world.ContactCount() > 0, "teleportado de volta recupera contactos");
}

static void TestDestroyPurgesPairs()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 300.0f), 600.0f, 10.0f);
    kx::Body *doomed = world.CreateBox(glm::vec2(0.0f, 260.0f), 18.0f, 18.0f, 1.0f);
    kx::Body *keeper = world.CreateBox(glm::vec2(0.0f, 215.0f), 18.0f, 18.0f, 1.0f);

    for (int s = 0; s < 180; ++s)
        world.Step(1.0f / 60.0f);
    size_t before = world.ContactCount();
    CHECK(before >= 2, "dois corpos assentes geram contactos");

    world.Destroy(doomed);
    for (int s = 0; s < 180; ++s)
        world.Step(1.0f / 60.0f);
    CHECK(world.BodyCount() == 2, "destroy remove o corpo");
    CHECK(world.ContactCount() >= 1, "keeper cai e assenta no chao sem contactos fantasma");
    CHECK(keeper->Position().y > 260.0f, "keeper desceu para o lugar do destruido");
}

static void TestBigGridStepsClean()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 3000.0f), 8000.0f, 10.0f);
    for (int i = 0; i < 2000; ++i)
        world.CreateBox(glm::vec2((float)(i % 50) * 45.0f, 2950.0f - (float)(i / 50) * 45.0f),
                        15.0f, 15.0f, 1.0f);
    for (int s = 0; s < 120; ++s)
        world.Step(1.0f / 60.0f);
    CHECK(world.BodyCount() == 2001, "grelha de 2000 corpos processa sem erro");
    CHECK(world.ContactCount() > 1500, "grelha assenta com contactos em massa");
}

int main()
{
    TestTreeMatchesBrute();
    TestRestingStackKeepsContacts();
    TestTeleportLosesAndRegainsContacts();
    TestDestroyPurgesPairs();
    TestBigGridStepsClean();

    if (gFailures)
    {
        std::printf("RESULTADO: %d FALHAS\n", gFailures);
        return 1;
    }
    std::printf("RESULTADO: broadphase limpo\n");
    return 0;
}