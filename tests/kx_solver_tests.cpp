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

static void TestBoxRestsOnGround()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 100.0f), 500.0f, 10.0f);
    kx::Body *box = world.CreateBox(glm::vec2(0.0f, 0.0f), 20.0f, 20.0f, 1.0f);

    StepN(world, 240);

    float restY = box->Position().y;
    CHECK(restY > 60.0f && restY < 75.0f, "caixa assenta no chao (y ~70)");
    CHECK(fabsf(box->Velocity().y) < 5.0f, "caixa em repouso (vel ~0)");

    StepN(world, 120);
    CHECK(fabsf(box->Position().y - restY) < 1.0f, "repouso estavel sem afundar");
}

static void TestCircleRestsOnEdge()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateEdge(glm::vec2(-200.0f, 100.0f), glm::vec2(200.0f, 100.0f));
    kx::Body *ball = world.CreateCircle(glm::vec2(0.0f, 0.0f), 15.0f, 1.0f);

    StepN(world, 240);

    CHECK(ball->Position().y > 80.0f && ball->Position().y < 90.0f, "circulo assenta na edge (y ~85)");
    CHECK(fabsf(ball->Velocity().y) < 5.0f, "circulo em repouso na edge");
}

static void TestStackOfBoxes()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 200.0f), 500.0f, 10.0f);

    kx::Body *stack[5];
    for (int i = 0; i < 5; ++i)
        stack[i] = world.CreateBox(glm::vec2(0.0f, 160.0f - i * 45.0f), 20.0f, 20.0f, 1.0f);

    StepN(world, 480);

    bool ordered = true;
    bool calm = true;
    for (int i = 0; i < 5; ++i)
    {
        if (i > 0 && stack[i]->Position().y > stack[i - 1]->Position().y - 20.0f)
            ordered = false;
        if (glm::length(stack[i]->Velocity()) > 10.0f)
            calm = false;
        if (fabsf(stack[i]->Position().x) > 30.0f)
            ordered = false;
    }
    CHECK(ordered, "pilha de 5 caixas mantem a ordem e alinhamento");
    CHECK(calm, "pilha estabiliza");
}

static void TestRestitutionBounce()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 200.0f), 500.0f, 10.0f);
    kx::Body *ball = world.CreateCircle(glm::vec2(0.0f, 0.0f), 10.0f, 1.0f);
    ball->SetRestitution(0.8f);

    float minY = 1000.0f;
    bool touched = false;
    for (int i = 0; i < 300; ++i)
    {
        world.Step(1.0f / 60.0f);
        if (ball->Position().y > 175.0f)
            touched = true;
        if (touched && ball->Position().y < minY)
            minY = ball->Position().y;
    }
    CHECK(touched, "bola chega ao chao");
    CHECK(minY < 120.0f, "bola ressalta com restituicao alta");

    kx::World dead(glm::vec2(0.0f, 500.0f));
    dead.CreateStaticBox(glm::vec2(0.0f, 200.0f), 500.0f, 10.0f);
    kx::Body *clay = dead.CreateCircle(glm::vec2(0.0f, 0.0f), 10.0f, 1.0f);
    StepN(dead, 300);
    CHECK(clay->Position().y > 170.0f, "sem restituicao nao ressalta");
}

static void TestKinematicPushesDynamic()
{
    kx::World world(glm::vec2(0.0f, 0.0f));
    kx::Body *plate = world.CreateKinematicBox(glm::vec2(-100.0f, 0.0f), 30.0f, 30.0f);
    plate->SetVelocity(glm::vec2(120.0f, 0.0f));
    kx::Body *box = world.CreateBox(glm::vec2(0.0f, 0.0f), 15.0f, 15.0f, 1.0f);

    StepN(world, 120);

    CHECK(box->Position().x > 60.0f, "kinematic empurra dynamic");
    CHECK(fabsf(plate->Velocity().x - 120.0f) < 0.001f, "kinematic nunca e travado");
    CHECK(plate->Position().x > 100.0f, "kinematic avanca sempre");
}

static void TestFrictionStopsSlide()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 100.0f), 800.0f, 10.0f);
    kx::Body *box = world.CreateBox(glm::vec2(-300.0f, 60.0f), 15.0f, 15.0f, 1.0f);
    box->SetFriction(0.6f);
    box->SetVelocity(glm::vec2(300.0f, 0.0f));

    StepN(world, 300);
    float x1 = box->Position().x;
    StepN(world, 60);
    CHECK(fabsf(box->Position().x - x1) < 2.0f, "friccao trava o deslize");
    CHECK(x1 > -280.0f && x1 < 400.0f, "deslizou uma distancia razoavel");

    kx::World ice(glm::vec2(0.0f, 500.0f));
    kx::Body *ground = ice.CreateStaticBox(glm::vec2(0.0f, 100.0f), 800.0f, 10.0f);
    ground->SetFriction(0.0f);
    kx::Body *puck = ice.CreateBox(glm::vec2(-300.0f, 60.0f), 15.0f, 15.0f, 1.0f);
    puck->SetFriction(0.0f);
    puck->SetVelocity(glm::vec2(300.0f, 0.0f));
    StepN(ice, 300);
    CHECK(puck->Position().x > x1 + 100.0f, "sem friccao desliza muito mais");
}

static void TestMultiShapeCart()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 200.0f), 500.0f, 10.0f);
    kx::Body *cart = world.CreateBody(kx::BodyType::Dynamic, glm::vec2(0.0f, 0.0f));
    cart->AddBox(30.0f, 10.0f, 1.0f);
    cart->AddCircle(glm::vec2(-20.0f, 12.0f), 8.0f, 1.0f);
    cart->AddCircle(glm::vec2(20.0f, 12.0f), 8.0f, 1.0f);

    StepN(world, 300);

    CHECK(cart->Position().y > 150.0f && cart->Position().y < 185.0f, "carrinho multi-shape assenta");
    CHECK(fabsf(cart->Position().x) < 40.0f, "carrinho nao foge para o lado");
    float tilt = fabsf(fmodf(cart->GetTransform().b, 1.0f));
    CHECK(tilt < 0.35f, "carrinho fica aproximadamente nivelado");
}

static void TestWarmStartingConverges()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 200.0f), 500.0f, 10.0f);
    for (int i = 0; i < 8; ++i)
        world.CreateBox(glm::vec2((i % 2) * 4.0f, 160.0f - i * 42.0f), 18.0f, 18.0f, 1.0f);

    StepN(world, 600);

    float maxSpeed = 0.0f;
    const ct::Vector<kx::Body *> &bodies = world.Bodies();
    for (size_t i = 0; i < bodies.size(); ++i)
    {
        float s = glm::length(bodies[i]->Velocity());
        if (s > maxSpeed)
            maxSpeed = s;
    }
    CHECK(maxSpeed < 12.0f, "pilha de 8 com warm starting converge para repouso");
}

static void TestMouseJointPullsBody()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    kx::Body *box = world.CreateBox(glm::vec2(0.0f, 0.0f), 15.0f, 15.0f, 1.0f);

    kx::MouseJoint *grab = new kx::MouseJoint(box, box->Position(), 5000.0f * box->Mass());
    world.AddJoint(grab);
    grab->SetTarget(glm::vec2(200.0f, -100.0f));

    StepN(world, 180);

    glm::vec2 d = box->Position() - glm::vec2(200.0f, -100.0f);
    CHECK(glm::length(d) < 20.0f, "mouse joint leva o corpo ao alvo contra a gravidade");

    world.DestroyJoint(grab);
    StepN(world, 60);
    CHECK(box->Position().y > -80.0f, "depois de largar volta a cair");
}

int main()
{
    TestBoxRestsOnGround();
    TestCircleRestsOnEdge();
    TestStackOfBoxes();
    TestRestitutionBounce();
    TestKinematicPushesDynamic();
    TestFrictionStopsSlide();
    TestMultiShapeCart();
    TestWarmStartingConverges();
    TestMouseJointPullsBody();

    if (gFailures)
    {
        std::printf("RESULTADO: %d FALHAS\n", gFailures);
        return 1;
    }
    std::printf("RESULTADO: solver limpo\n");
    return 0;
}
