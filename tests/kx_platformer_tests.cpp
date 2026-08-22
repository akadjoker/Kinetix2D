// Platformer mechanics integration test.
// - fixed-rotation character (never topples)
// - kinematic sliding platforms (lateral + vertical) that carry the character
// Port of the Box2D "Moving Platforms" pattern: kinematic body with velocity
// pushes the dynamic body resting on it through contacts (no new algorithm).
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

// Plataforma deslizante LATERAL (esquerda-direita) leva o personagem.
static void TestLateralPlatformCarriesCharacter()
{
    kx::World world(glm::vec2(0.0f, -500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, -60.0f), 300.0f, 5.0f);

    kx::Body *platform = world.CreateKinematicBox(glm::vec2(-100.0f, -45.0f), 30.0f, 3.0f);
    platform->SetFriction(1.0f);

    kx::Body *character = MakeCharacter(world, glm::vec2(-100.0f, -35.0f));

    // Fase 1: plataforma vai para a direita 2s (v=50 => +100px)
    for (int i = 0; i < 120; ++i)
    {
        platform->SetVelocity(glm::vec2(50.0f, 0.0f));
        world.Step(1.0f / 60.0f);
    }
    Check(std::fabs(character->Position().x - 0.0f) < 12.0f,
          "personagem acompanha plataforma lateral (ida)");
    Check(character->Position().y > -40.0f, "personagem continua em cima da plataforma");
    Check(std::fabs(character->Angle()) < 1.0e-5f, "personagem nao roda ao ser carregado");

    // Fase 2: plataforma volta para a esquerda 2s (volta a -100)
    for (int i = 0; i < 120; ++i)
    {
        platform->SetVelocity(glm::vec2(-50.0f, 0.0f));
        world.Step(1.0f / 60.0f);
    }
    Check(std::fabs(character->Position().x - (-100.0f)) < 12.0f,
          "personagem acompanha plataforma lateral (volta)");
    Check(character->Position().y > -40.0f, "personagem continua em cima na volta");
}

// Plataforma deslizante VERTICAL (cima-baixo) leva o personagem para cima.
static void TestVerticalPlatformCarriesCharacter()
{
    kx::World world(glm::vec2(0.0f, -500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, -60.0f), 300.0f, 5.0f);

    kx::Body *platform = world.CreateKinematicBox(glm::vec2(0.0f, 10.0f), 30.0f, 3.0f);
    platform->SetFriction(1.0f);

    kx::Body *character = MakeCharacter(world, glm::vec2(0.0f, 22.0f));

    // Plataforma sobe 1.5s: y = 10 + 60*1.5 = 100
    for (int i = 0; i < 90; ++i)
    {
        platform->SetVelocity(glm::vec2(0.0f, 60.0f));
        world.Step(1.0f / 60.0f);
    }

    float expectedY = 10.0f + 60.0f * 1.5f; // 100
    // personagem assenta em cima da plataforma: centro ~ plataforma.y + 3 + 5
    Check(std::fabs(character->Position().y - (expectedY + 8.0f)) < 8.0f,
          "personagem e levado para cima pela plataforma vertical");
    Check(std::fabs(character->Angle()) < 1.0e-5f, "personagem nao roda ao subir");

    // Plataforma desce de volta: y = 100 - 60*1.5 = 10
    for (int i = 0; i < 90; ++i)
    {
        platform->SetVelocity(glm::vec2(0.0f, -60.0f));
        world.Step(1.0f / 60.0f);
    }
    Check(std::fabs(character->Position().y - (10.0f + 8.0f)) < 8.0f,
          "personagem acompanha a plataforma vertical na descida");
}

// Personagem fixed-rotation sobrevive a uma queda sem tombar.
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

// Isolado: uma caixa lançada para cima contra um teto estático (sem
// plataforma) tem de parar no teto, NÃO atravessar.
static void TestBoxLaunchedIntoStaticCeiling()
{
    kx::World world(glm::vec2(0.0f, -500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 60.0f), 200.0f, 5.0f); // fundo em y=55

    kx::Body *box = world.CreateBox(glm::vec2(0.0f, 0.0f), 5.0f, 8.0f, 1.0f);
    box->SetVelocity(glm::vec2(0.0f, 300.0f));

    for (int i = 0; i < 120; ++i)
        world.Step(1.0f / 60.0f);

    // Se nao tunelar, a caixa fica junto ao teto (centro ~47) ou cai.
    Check(box->Position().y < 54.0f,
          "caixa NAO atravessa o teto estatico (sem plataforma)");
}

// Comprimido entre a plataforma vertical e um teto estatico: o personagem nao
// atravessa o teto nem adormece no contacto. Quando a plataforma inverte, a
// gravidade liberta-o do teto sem teleportes ou velocidades artificiais.
static void TestSqueezedBetweenPlatformAndCeiling()
{
    kx::World world(glm::vec2(0.0f, -500.0f));

    // teto estatico (fundo em y=55)
    world.CreateStaticBox(glm::vec2(0.0f, 60.0f), 200.0f, 5.0f);
    // chao estatico (topo em y=-75)
    world.CreateStaticBox(glm::vec2(0.0f, -80.0f), 300.0f, 5.0f);

    kx::Body *platform = world.CreateKinematicBox(glm::vec2(0.0f, 10.0f), 30.0f, 3.0f);
    platform->SetFriction(1.0f);

    // personagem assenta na plataforma: centro y = plataforma + 11
    kx::Body *character = MakeCharacter(world, glm::vec2(0.0f, 22.0f));

    float maxY = -1.0e30f;
    // A plataforma sobe ate comprimir o personagem, mas inverte antes de
    // atravessar completamente o teto.
    for (int i = 0; i < 40; ++i)
    {
        platform->SetVelocity(glm::vec2(0.0f, 60.0f));
        world.Step(1.0f / 60.0f);
        maxY = glm::max(maxY, character->Position().y);
    }

    const float squeezedY = character->Position().y;
    const bool awakeWhileSqueezed = character->IsAwake();

    // Nos primeiros 0.25 s da descida ja se deve separar do teto.
    for (int i = 0; i < 15; ++i)
    {
        platform->SetVelocity(glm::vec2(0.0f, -60.0f));
        world.Step(1.0f / 60.0f);
    }
    const float releasedY = character->Position().y;

    // Completa a descida ate a plataforma voltar a y=10.
    for (int i = 15; i < 40; ++i)
    {
        platform->SetVelocity(glm::vec2(0.0f, -60.0f));
        world.Step(1.0f / 60.0f);
    }

    platform->SetVelocity(glm::vec2(0.0f));
    for (int i = 0; i < 80; ++i)
        world.Step(1.0f / 60.0f);

    // centro do personagem nunca passa do fundo do teto (55): nao tunelou
    Check(maxY < 58.0f, "personagem NAO atravessa o teto (max y < fundo do teto)");
    Check(awakeWhileSqueezed, "personagem comprimido permanece acordado");
    Check(releasedY < squeezedY - 1.0f,
          "personagem separa-se do teto quando a plataforma desce");
    Check(std::fabs(character->Position().x) < 0.01f,
          "solver nao inventa movimento lateral ao resolver a compressao");
    // nao ficou colado ao teto (fundo 55 -> centro ~47)
    Check(character->Position().y < 40.0f, "personagem NAO fica colado ao teto");
    // Depois de libertado pode voltar a assentar naturalmente na plataforma;
    // isso e suporte fisico, nao uma ligacao/teleporte artificial.
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
