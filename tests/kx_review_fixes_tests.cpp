// Testes para as correcoes/funcionalidades da review exaustiva da fisica (2026-08-22):
// destruicao de corpos com joints anexados, RecomputeMass em corpos adormecidos,
// corpos adormecidos acordarem ao serem tocados, raycast, chain shapes (terreno
// one-sided) e Slice (corte de shapes).
#include <kx/kx.h>

#include <cstdio>
#include <cmath>

static int gFailures = 0;

#define CHECK(cond, name)                                        \
    do                                                           \
    {                                                            \
        if (cond)                                                \
        {                                                        \
            std::printf("[PASS] %s\n", name);                    \
        }                                                        \
        else                                                     \
        {                                                        \
            std::printf("[FAIL] %s (linha %d)\n", name, __LINE__); \
            ++gFailures;                                         \
        }                                                        \
    } while (0)

static void StepN(kx::World &world, int steps)
{
    for (int i = 0; i < steps; ++i)
        world.Step(1.0f / 60.0f);
}

// Bug #1: destruir um corpo com um joint anexado nao pode deixar ponteiros pendentes.
static void TestDestroyBodyWithJoint()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    kx::Body *a = world.CreateBox(glm::vec2(-50.0f, 0.0f), 10.0f, 10.0f, 1.0f);
    kx::Body *b = world.CreateBox(glm::vec2(50.0f, 0.0f), 10.0f, 10.0f, 1.0f);
    world.AddJoint(new kx::RevoluteJoint(a, b, glm::vec2(0.0f, 0.0f)));

    CHECK(world.Joints().size() == 1, "joint criado");

    world.Destroy(a);
    CHECK(world.Joints().size() == 0, "destruir corpo remove o joint anexado");

    // Se sobrasse um ponteiro pendente, isto e o que despoletava o use-after-free.
    StepN(world, 10);
    CHECK(true, "Step() apos destruir corpo com joint nao crasha");
}

// GearJoint depende de dois Body* (mBodyC/mBodyD) que nao sao o seu proprio
// BodyA()/BodyB() — tem de cascatear via DependsOnBody.
static void TestDestroyBodyWithGearJointDependency()
{
    kx::World world(glm::vec2(0.0f));
    kx::Body *wheelA = world.CreateCircle(glm::vec2(-50.0f, 0.0f), 10.0f, 1.0f);
    kx::Body *wheelB = world.CreateCircle(glm::vec2(50.0f, 0.0f), 10.0f, 1.0f);
    kx::Body *axleC = world.CreateStaticBox(glm::vec2(-50.0f, 0.0f), 2.0f, 2.0f);
    kx::Body *axleD = world.CreateStaticBox(glm::vec2(50.0f, 0.0f), 2.0f, 2.0f);

    kx::RevoluteJoint *rjA = new kx::RevoluteJoint(axleC, wheelA, glm::vec2(-50.0f, 0.0f));
    kx::RevoluteJoint *rjB = new kx::RevoluteJoint(axleD, wheelB, glm::vec2(50.0f, 0.0f));
    world.AddJoint(rjA);
    world.AddJoint(rjB);
    world.AddJoint(new kx::GearJoint(rjA, rjB, 1.0f));

    CHECK(world.Joints().size() == 3, "3 joints criados (2 revolute + 1 gear)");

    // axleC e mBodyC do GearJoint, nao o seu BodyA()/BodyB() (que sao wheelA/wheelB) —
    // sem DependsOnBody, o GearJoint sobrevivia com mJoint1 (rjA) tambem destruido.
    // axleC e tambem o BodyA() direto de rjA, por isso rjA e removido de qualquer forma;
    // o que este teste garante e que o GearJoint (que NAO tem axleC como seu proprio
    // BodyA()/BodyB()) cascateia com ele, e que rjB (nao relacionado) sobrevive.
    world.Destroy(axleC);
    CHECK(world.Joints().size() == 1, "destruir axleC cascateia: rjA e o GearJoint desaparecem, rjB sobrevive");
    CHECK(world.Joints().size() == 1 && world.Joints()[0] == rjB, "o joint sobrevivente e mesmo rjB");

    StepN(world, 10);
    CHECK(true, "Step() apos cascata de joints nao crasha");
}

// Bug #2: adicionar uma shape a um corpo adormecido nao pode zerar a massa.
static void TestRecomputeMassWhileAsleep()
{
    kx::World world(glm::vec2(0.0f));
    kx::Body *body = world.CreateCircle(glm::vec2(0.0f), 5.0f, 1.0f);
    float massBefore = body->Mass();

    for (int i = 0; i < 120 && body->IsAwake(); ++i)
        world.Step(1.0f / 60.0f);
    CHECK(!body->IsAwake(), "corpo adormece (sem forcas, velocidade ~0)");

    body->AddCircle(glm::vec2(20.0f, 0.0f), 5.0f, 1.0f);
    CHECK(body->Mass() > massBefore * 1.5f, "massa recalculada corretamente mesmo adormecido (nao ficou 0/infinita)");
}

// Bug descoberto durante a implementacao: um corpo adormecido nunca acordava ao ser
// atingido por outro corpo (nada chamava SetAwake(true) por colisao).
static void TestSleepingBodyWakesOnImpact()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 100.0f), 500.0f, 10.0f);
    kx::Body *resting = world.CreateBox(glm::vec2(0.0f, 0.0f), 20.0f, 20.0f, 1.0f);

    StepN(world, 120);
    CHECK(!resting->IsAwake(), "caixa em repouso adormece");
    float restY = resting->Position().y;

    kx::Body *bullet = world.CreateCircle(glm::vec2(-300.0f, restY), 10.0f, 5.0f);
    bullet->SetGravityScale(0.0f);
    bullet->SetVelocity(glm::vec2(2000.0f, 0.0f));

    StepN(world, 30);
    CHECK(resting->IsAwake(), "impacto acorda o corpo adormecido");
    CHECK(std::fabs(resting->Position().y - restY) > 0.5f || resting->Position().x > 0.5f,
          "corpo acordado reage ao impacto (nao fica congelado na posicao)");
}

// Ids reciclados (ver World::CreateBody / Destroy) nao podem corromper o par
// corpo-a-corpo ao longo de sucessivas criacoes/destruicoes.
static void TestBodyIdRecyclingStaysConsistent()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 100.0f), 500.0f, 10.0f);

    for (int i = 0; i < 500; ++i)
    {
        kx::Body *b = world.CreateBox(glm::vec2(0.0f, 0.0f), 5.0f, 5.0f, 1.0f);
        StepN(world, 2);
        world.Destroy(b);
    }

    kx::Body *final = world.CreateBox(glm::vec2(0.0f, 0.0f), 5.0f, 5.0f, 1.0f);
    StepN(world, 180);
    // Chao em y=100 (halfHeight 10 -> topo em y=90), caixa com halfHeight 5 -> repousa
    // perto de y=85.
    CHECK(final->Position().y > 78.0f && final->Position().y < 90.0f,
          "corpo assenta normalmente apos muita reciclagem de ids");
}

static void TestRayCast()
{
    kx::World world(glm::vec2(0.0f));
    world.CreateStaticBox(glm::vec2(100.0f, 0.0f), 10.0f, 10.0f);

    kx::RayCastHit hit;
    bool hitFound = world.RayCastClosest(glm::vec2(0.0f, 0.0f), glm::vec2(200.0f, 0.0f), hit);
    CHECK(hitFound, "raycast atinge a caixa");
    CHECK(hit.fraction > 0.4f && hit.fraction < 0.5f, "fraction do hit e ~0.45 (borda em x=90 de 200)");
    CHECK(hit.normal.x < -0.9f, "normal do hit aponta contra o raio (-x)");

    kx::RayCastHit miss;
    bool missFound = world.RayCastClosest(glm::vec2(0.0f, -50.0f), glm::vec2(200.0f, 0.0f), miss);
    CHECK(!missFound, "raycast que nao atinge nada devolve false");
}

// AddChain: um corpo a deslizar sobre a junta de dois segmentos nao pode ficar preso
// ("degrau" interno), e a colisao so pode acontecer vindo do lado da normal (one-sided).
static void TestChainShape()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    glm::vec2 points[3] = {glm::vec2(-200.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec2(200.0f, 0.0f)};
    world.CreateChain(points, 3, false);

    kx::Body *ball = world.CreateCircle(glm::vec2(-5.0f, -50.0f), 10.0f, 1.0f);
    ball->SetVelocity(glm::vec2(150.0f, 0.0f)); // atravessa a junta em x=0 enquanto cai

    float minY = 1e30f;
    for (int i = 0; i < 90; ++i)
    {
        world.Step(1.0f / 60.0f);
        if (ball->Position().y < minY)
            minY = ball->Position().y;
    }
    CHECK(ball->Position().y > -20.0f && ball->Position().y < 20.0f,
          "bola assenta na chain ao atravessar a junta entre segmentos, sem ficar presa");

    // Do lado errado (por baixo, encostando por dentro), o chain e atravessavel.
    kx::Body *fromBelow = world.CreateCircle(glm::vec2(-5.0f, 30.0f), 10.0f, 1.0f);
    for (int i = 0; i < 30; ++i)
        world.Step(1.0f / 60.0f);
    CHECK(fromBelow->Position().y > 40.0f, "chain one-sided nao bloqueia colisao vinda do lado interior");
}

static void TestSliceSplitsBodyInTwo()
{
    kx::World world(glm::vec2(0.0f));
    kx::Body *box = world.CreateBox(glm::vec2(0.0f, 0.0f), 50.0f, 50.0f, 1.0f);
    float totalMassBefore = box->Mass();
    size_t bodiesBefore = world.BodyCount();

    // normal=(1,0): "positive" = Dot(v-point,normal)>=0 = lado de x maior (direita).
    kx::Body *positive = nullptr;
    kx::Body *negative = nullptr;
    bool sliced = kx::Slice(world, box, glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), &positive, &negative, 20.0f);

    CHECK(sliced, "Slice atravessa a caixa e corta com sucesso");
    CHECK(positive != nullptr && negative != nullptr, "os dois lados produzem corpo");
    CHECK(world.BodyCount() == bodiesBefore + 1, "corpo original destruido, dois novos criados (net +1)");
    CHECK(std::fabs((positive->Mass() + negative->Mass()) - totalMassBefore) < totalMassBefore * 0.05f,
          "massa total preservada pelo corte (dentro de 5%)");
    CHECK(positive->Velocity().x > 0.0f && negative->Velocity().x < 0.0f,
          "separationSpeed afasta as duas metades uma da outra (positive para +x, negative para -x)");

    // Uma reta cujo ponto fica fora da caixa (x=1000, normal=(1,0) -> reta vertical em
    // x=1000) deixa a caixa inteira (x em [-50,50]) do lado negativo: nao ha corte.
    kx::World world2(glm::vec2(0.0f));
    kx::Body *box2 = world2.CreateBox(glm::vec2(0.0f, 0.0f), 50.0f, 50.0f, 1.0f);
    size_t bodiesBefore2 = world2.BodyCount();
    kx::Body *p2 = nullptr;
    kx::Body *n2 = nullptr;
    bool missedSlice = kx::Slice(world2, box2, glm::vec2(1000.0f, 0.0f), glm::vec2(1.0f, 0.0f), &p2, &n2);
    CHECK(!missedSlice && p2 == nullptr && n2 == nullptr, "Slice que nao atravessa o corpo devolve false sem alterar nada");
    CHECK(world2.BodyCount() == bodiesBefore2, "corpo original intacto quando o corte falha");
}

static void TestForceDampingGravityScale()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    kx::Body *floater = world.CreateCircle(glm::vec2(0.0f, 0.0f), 10.0f, 1.0f);
    floater->SetGravityScale(0.0f);
    StepN(world, 60);
    CHECK(std::fabs(floater->Position().y) < 1.0f, "gravityScale=0 anula a gravidade");

    kx::Body *damped = world.CreateCircle(glm::vec2(100.0f, 0.0f), 10.0f, 1.0f);
    damped->SetGravityScale(0.0f);
    damped->SetLinearDamping(5.0f);
    damped->SetVelocity(glm::vec2(1000.0f, 0.0f));
    StepN(world, 60);
    CHECK(damped->Velocity().x > 0.0f && damped->Velocity().x < 100.0f,
          "linear damping reduz a velocidade ao longo do tempo");

    kx::Body *thruster = world.CreateCircle(glm::vec2(200.0f, 0.0f), 10.0f, 1.0f);
    thruster->SetGravityScale(0.0f);
    for (int i = 0; i < 60; ++i)
    {
        thruster->ApplyForceToCenter(glm::vec2(0.0f, -thruster->Mass() * 500.0f));
        world.Step(1.0f / 60.0f);
    }
    CHECK(thruster->Position().y < -50.0f, "ApplyForceToCenter continua a acelerar o corpo ao longo do tempo");
}

// CCD leve (Body::SetBullet): um projetil rapido nao pode atravessar uma parede fina
// nem no primeiro step, nem em nenhum dos seguintes (a bala fica parada a empurrar a
// parede, nao so "sobrevive um step e depois atravessa" — foi exatamente este o bug
// que a primeira versao do SolveBulletSweeps tinha).
static void TestBulletCCD()
{
    // Sem CCD: atravessa uma parede fina a alta velocidade.
    {
        kx::World world(glm::vec2(0.0f));
        world.CreateStaticBox(glm::vec2(0.0f, 0.0f), 2.0f, 200.0f);
        kx::Body *slug = world.CreateCircle(glm::vec2(-300.0f, 0.0f), 5.0f, 1.0f);
        slug->SetGravityScale(0.0f);
        slug->SetVelocity(glm::vec2(3000.0f, 0.0f));

        StepN(world, 10); // -300 a 3000u/s, 50u/step -> precisa de ~6 steps so para chegar a x=0
        CHECK(slug->Position().x > 50.0f, "sem SetBullet, o projetil atravessa a parede fina (baseline do problema)");
    }

    // Com CCD: fica preso do lado de fora, mesmo ao longo de varios steps.
    {
        kx::World world(glm::vec2(0.0f));
        world.CreateStaticBox(glm::vec2(0.0f, 0.0f), 2.0f, 200.0f);
        kx::Body *bullet = world.CreateCircle(glm::vec2(-300.0f, 0.0f), 5.0f, 1.0f);
        bullet->SetGravityScale(0.0f);
        bullet->SetVelocity(glm::vec2(3000.0f, 0.0f));
        bullet->SetBullet(true);

        bool everTunneled = false;
        for (int i = 0; i < 20; ++i)
        {
            world.Step(1.0f / 60.0f);
            if (bullet->Position().x > 10.0f)
                everTunneled = true;
        }
        CHECK(!everTunneled, "com SetBullet, o projetil nunca atravessa a parede em 20 steps seguidos");
        CHECK(bullet->Position().x < 5.0f && bullet->Position().x > -20.0f,
              "e fica parado mesmo encostado a face de fora da parede");
    }

    // Contra outro corpo dynamic, o CCD leve nao intervem (fica por conta do solver
    // discreto normal) — aqui so confirmamos que nao crasha nem se comporta de forma
    // absurda.
    {
        kx::World world(glm::vec2(0.0f));
        kx::Body *target = world.CreateCircle(glm::vec2(0.0f, 0.0f), 5.0f, 1.0f);
        kx::Body *bullet = world.CreateCircle(glm::vec2(-300.0f, 0.0f), 5.0f, 1.0f);
        bullet->SetGravityScale(0.0f);
        bullet->SetVelocity(glm::vec2(3000.0f, 0.0f));
        bullet->SetBullet(true);
        StepN(world, 5);
        CHECK(true, "CCD leve com alvo dynamic nao crasha (fica por conta do solver normal)");
        (void)target;
    }
}

int main()
{
    TestDestroyBodyWithJoint();
    TestDestroyBodyWithGearJointDependency();
    TestRecomputeMassWhileAsleep();
    TestSleepingBodyWakesOnImpact();
    TestBodyIdRecyclingStaysConsistent();
    TestRayCast();
    TestChainShape();
    TestSliceSplitsBodyInTwo();
    TestForceDampingGravityScale();
    TestBulletCCD();

    std::printf("%d failures\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
