
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

static kx::Body *MakeCharacter(kx::World &world, const glm::vec2 &pos)
{
    kx::Body *character = world.CreateBox(pos, 3.0f, 5.0f, 1.0f);
    character->SetFixedRotation(true);
    character->SetFriction(1.0f);
    return character;
}

static void TestLateralPlatformCarriesCharacter()
{
    kx::World world(glm::vec2(0.0f, -500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, -60.0f), 300.0f, 5.0f);

    kx::Body *platform = world.CreateKinematicBox(glm::vec2(-100.0f, -45.0f), 30.0f, 3.0f);
    platform->SetFriction(1.0f);

    kx::Body *character = MakeCharacter(world, glm::vec2(-100.0f, -35.0f));

    for (int i = 0; i < 120; ++i)
    {
        platform->SetVelocity(glm::vec2(50.0f, 0.0f));
        world.Step(1.0f / 60.0f);
    }
    Check(std::fabs(character->Position().x - 0.0f) < 12.0f,
          "personagem acompanha plataforma lateral (ida)");
    Check(character->Position().y > -40.0f, "personagem continua em cima da plataforma");
    Check(std::fabs(character->Angle()) < 1.0e-5f, "personagem nao roda ao ser carregado");

    for (int i = 0; i < 120; ++i)
    {
        platform->SetVelocity(glm::vec2(-50.0f, 0.0f));
        world.Step(1.0f / 60.0f);
    }
    Check(std::fabs(character->Position().x - (-100.0f)) < 12.0f,
          "personagem acompanha plataforma lateral (volta)");
    Check(character->Position().y > -40.0f, "personagem continua em cima na volta");
}

static void TestVerticalPlatformCarriesCharacter()
{
    kx::World world(glm::vec2(0.0f, -500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, -60.0f), 300.0f, 5.0f);

    kx::Body *platform = world.CreateKinematicBox(glm::vec2(0.0f, 10.0f), 30.0f, 3.0f);
    platform->SetFriction(1.0f);

    kx::Body *character = MakeCharacter(world, glm::vec2(0.0f, 22.0f));

    for (int i = 0; i < 90; ++i)
    {
        platform->SetVelocity(glm::vec2(0.0f, 60.0f));
        world.Step(1.0f / 60.0f);
    }

    float expectedY = 10.0f + 60.0f * 1.5f; 

    Check(std::fabs(character->Position().y - (expectedY + 8.0f)) < 8.0f,
          "personagem e levado para cima pela plataforma vertical");
    Check(std::fabs(character->Angle()) < 1.0e-5f, "personagem nao roda ao subir");

    for (int i = 0; i < 90; ++i)
    {
        platform->SetVelocity(glm::vec2(0.0f, -60.0f));
        world.Step(1.0f / 60.0f);
    }
    Check(std::fabs(character->Position().y - (10.0f + 8.0f)) < 8.0f,
          "personagem acompanha a plataforma vertical na descida");
}

static void TestCharacterFallsUpright()
{
    kx::World world(glm::vec2(0.0f, -500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, -80.0f), 300.0f, 5.0f);
    kx::Body *character = MakeCharacter(world, glm::vec2(0.0f, 200.0f));

    for (int i = 0; i < 240; ++i)
        world.Step(1.0f / 60.0f);

    Check(character->Position().y < -60.0f, "personagem cai ao chao");
    Check(std::fabs(character->Angle()) < 1.0e-5f, "personagem aterra de pe (sem tombar)");
}

static void TestBoxLaunchedIntoStaticCeiling()
{
    kx::World world(glm::vec2(0.0f, -500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 60.0f), 200.0f, 5.0f); 

    kx::Body *box = world.CreateBox(glm::vec2(0.0f, 0.0f), 5.0f, 8.0f, 1.0f);
    box->SetVelocity(glm::vec2(0.0f, 300.0f));

    for (int i = 0; i < 120; ++i)
        world.Step(1.0f / 60.0f);

    Check(box->Position().y < 54.0f,
          "caixa NAO atravessa o teto estatico (sem plataforma)");
}

static void TestSqueezedBetweenPlatformAndCeiling()
{
    kx::World world(glm::vec2(0.0f, -500.0f));

    world.CreateStaticBox(glm::vec2(0.0f, 60.0f), 200.0f, 5.0f);

    world.CreateStaticBox(glm::vec2(0.0f, -80.0f), 300.0f, 5.0f);

    kx::Body *platform = world.CreateKinematicBox(glm::vec2(0.0f, 10.0f), 30.0f, 3.0f);
    platform->SetFriction(1.0f);

    kx::Body *character = MakeCharacter(world, glm::vec2(0.0f, 22.0f));

    float maxY = -1.0e30f;

    for (int i = 0; i < 40; ++i)
    {
        platform->SetVelocity(glm::vec2(0.0f, 60.0f));
        world.Step(1.0f / 60.0f);
        maxY = glm::max(maxY, character->Position().y);
    }

    const float squeezedY = character->Position().y;
    const bool awakeWhileSqueezed = character->IsAwake();

    for (int i = 0; i < 15; ++i)
    {
        platform->SetVelocity(glm::vec2(0.0f, -60.0f));
        world.Step(1.0f / 60.0f);
    }
    const float releasedY = character->Position().y;

    for (int i = 15; i < 40; ++i)
    {
        platform->SetVelocity(glm::vec2(0.0f, -60.0f));
        world.Step(1.0f / 60.0f);
    }

    platform->SetVelocity(glm::vec2(0.0f));
    for (int i = 0; i < 80; ++i)
        world.Step(1.0f / 60.0f);

    Check(maxY < 58.0f, "personagem NAO atravessa o teto (max y < fundo do teto)");
    Check(awakeWhileSqueezed, "personagem comprimido permanece acordado");
    Check(releasedY < squeezedY - 1.0f,
          "personagem separa-se do teto quando a plataforma desce");
    Check(std::fabs(character->Position().x) < 0.01f,
          "solver nao inventa movimento lateral ao resolver a compressao");

    Check(character->Position().y < 40.0f, "personagem NAO fica colado ao teto");

    Check(character->Position().y > 12.0f && character->Position().y < 25.0f,
          "personagem volta a assentar sobre a plataforma sem sobreposicao");
    Check(std::fabs(character->Angle()) < 1.0e-5f, "personagem nao roda ao ser esmagado");
}

int main()
{
    TestLateralPlatformCarriesCharacter();
    TestVerticalPlatformCarriesCharacter();
    TestCharacterFallsUpright();
    TestBoxLaunchedIntoStaticCeiling();
    TestSqueezedBetweenPlatformAndCeiling();
    std::printf("%d failures\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}