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
    cart->AddBox(30.0f, 10.0f, glm::vec2(0.0f, 0.0f), 1.0f);
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

static void TestDeepOverlapNoExplosion()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 200.0f), 500.0f, 10.0f);
    kx::Body *buried = world.CreateBox(glm::vec2(0.0f, 193.0f), 20.0f, 20.0f, 1.0f);
    kx::Body *ontop = world.CreateBox(glm::vec2(5.0f, 170.0f), 15.0f, 15.0f, 1.0f);

    float maxSpeed = 0.0f;
    for (int i = 0; i < 240; ++i)
    {
        world.Step(1.0f / 60.0f);
        float s1 = glm::length(buried->Velocity());
        float s2 = glm::length(ontop->Velocity());
        if (s1 > maxSpeed)
            maxSpeed = s1;
        if (s2 > maxSpeed)
            maxSpeed = s2;
    }
    CHECK(maxSpeed < 450.0f, "spawn enterrado sai sem explosao (vel limitada)");
    CHECK(buried->Position().y < 190.0f, "corpo enterrado e expelido para cima");
    CHECK(fabsf(glm::length(buried->Velocity())) < 15.0f, "estabiliza depois de sair");
}

static void TestWheelJointHoldsWheel()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 300.0f), 500.0f, 10.0f);

    kx::Body *chassis = world.CreateBox(glm::vec2(0.0f, 0.0f), 40.0f, 12.0f, 1.0f);
    kx::Body *wheel = world.CreateCircle(glm::vec2(-25.0f, 40.0f), 12.0f, 1.0f);

    kx::WheelJoint *joint = new kx::WheelJoint(chassis, wheel, glm::vec2(-25.0f, 40.0f), glm::vec2(0.0f, -1.0f));
    world.AddJoint(joint);

    StepN(world, 240);

    float dist = glm::length(wheel->Position() - joint->AnchorA());
    CHECK(dist < 40.0f, "wheel joint mantem a roda perto do chassis");
    CHECK(glm::length(chassis->Velocity()) < 400.0f && glm::length(wheel->Velocity()) < 400.0f,
          "wheel joint nao explode");
}

static void TestWheelJointMotorSpinsWheel()
{
    kx::World world(glm::vec2(0.0f, 0.0f));

    kx::Body *chassis = world.CreateBox(glm::vec2(0.0f, 0.0f), 40.0f, 12.0f, 1.0f);
    kx::Body *wheel = world.CreateCircle(glm::vec2(-25.0f, 40.0f), 12.0f, 1.0f);

    kx::WheelJoint *joint = new kx::WheelJoint(chassis, wheel, glm::vec2(-25.0f, 40.0f), glm::vec2(0.0f, -1.0f));
    joint->SetMotor(true, 15.0f, 2000000.0f);
    world.AddJoint(joint);

    StepN(world, 60);

    CHECK(wheel->AngularVelocity() > 10.0f, "motor do wheel joint acelera a roda ate perto da velocidade alvo");
}

static void TestAddPolygonTriangleRests()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 300.0f), 500.0f, 10.0f);

    glm::vec2 tri[3] = {glm::vec2(0.0f, -20.0f), glm::vec2(20.0f, 15.0f), glm::vec2(-20.0f, 15.0f)};
    kx::Body *body = world.CreatePolygon(glm::vec2(0.0f, 0.0f), tri, 3, 1.0f);

    StepN(world, 240);

    CHECK(body->Mass() > 0.0f, "AddPolygon triangulo tem massa positiva");
    CHECK(body->Position().y > 200.0f && body->Position().y < 320.0f, "triangulo assenta no chao");
    CHECK(glm::length(body->Velocity()) < 50.0f, "triangulo em repouso");
}

static void TestConcaveMeshLShapeRests()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 300.0f), 500.0f, 10.0f);

    glm::vec2 outline[6] = {
        glm::vec2(-30.0f, -30.0f), glm::vec2(30.0f, -30.0f), glm::vec2(30.0f, -10.0f),
        glm::vec2(-10.0f, -10.0f), glm::vec2(-10.0f, 30.0f), glm::vec2(-30.0f, 30.0f)};

    kx::Body *body = world.CreateMesh(glm::vec2(0.0f, 0.0f), outline, 6, 1.0f);

    CHECK(body->ShapeCount() > 0, "CreateMesh L-shape triangula pelo menos uma forma");
    CHECK(body->Mass() > 0.0f, "CreateMesh L-shape tem massa positiva");

    StepN(world, 240);

    CHECK(std::isfinite(body->Position().x) && std::isfinite(body->Position().y), "L-shape nao diverge");
    CHECK(body->Position().y < 320.0f, "L-shape nao atravessa o chao");
}

static void TestConcaveMeshStarSettles()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 300.0f), 500.0f, 10.0f);

    const int kPoints = 10;
    glm::vec2 outline[kPoints];
    for (int i = 0; i < kPoints; ++i)
    {
        float angle = -1.57079633f + (float)i * (6.28318531f / (float)kPoints);
        float radius = (i % 2 == 0) ? 30.0f : 12.0f;
        outline[i] = glm::vec2(radius * cosf(angle), radius * sinf(angle));
    }

    kx::Body *body = world.CreateMesh(glm::vec2(0.0f, 0.0f), outline, kPoints, 1.0f);

    CHECK(body->ShapeCount() > 0, "CreateMesh estrela triangula pelo menos uma forma");
    CHECK(body->Mass() > 0.0f, "CreateMesh estrela tem massa positiva");

    float maxSpeed = 0.0f;
    for (int i = 0; i < 240; ++i)
    {
        world.Step(1.0f / 60.0f);
        float s = glm::length(body->Velocity());
        if (s > maxSpeed)
            maxSpeed = s;
    }

    CHECK(maxSpeed < 2000.0f, "estrela concava nao explode");
    CHECK(std::isfinite(body->Position().x) && std::isfinite(body->Position().y), "estrela nao diverge");
}

static void TestFiltersAndCollideConnected()
{
    kx::World world(glm::vec2(0.0f, 500.0f));
    world.CreateStaticBox(glm::vec2(0.0f, 200.0f), 500.0f, 10.0f);

    kx::Body *ghostA = world.CreateBox(glm::vec2(-100.0f, 150.0f), 15.0f, 15.0f, 1.0f);
    kx::Body *ghostB = world.CreateBox(glm::vec2(-100.0f, 150.0f), 15.0f, 15.0f, 1.0f);
    ghostA->SetFilter(2, 0xFFFF & ~2u);
    ghostB->SetFilter(2, 0xFFFF & ~2u);

    for (int i = 0; i < 240; ++i)
        world.Step(1.0f / 60.0f);
    CHECK(fabsf(ghostA->Position().x - ghostB->Position().x) < 5.0f,
          "mesma categoria mascarada: atravessam-se e caem juntos");
    CHECK(ghostA->Position().y > 150.0f, "mas ambos colidem com o chao (categoria 1)");

    kx::World grouped(glm::vec2(0.0f, 500.0f));
    grouped.CreateStaticBox(glm::vec2(0.0f, 200.0f), 500.0f, 10.0f);
    kx::Body *na = grouped.CreateBox(glm::vec2(0.0f, 120.0f), 15.0f, 15.0f, 1.0f);
    kx::Body *nb = grouped.CreateBox(glm::vec2(0.0f, 158.0f), 15.0f, 15.0f, 1.0f);
    na->SetFilter(1, 0xFFFF, -7);
    nb->SetFilter(1, 0xFFFF, -7);
    for (int i = 0; i < 240; ++i)
        grouped.Step(1.0f / 60.0f);
    CHECK(fabsf(na->Position().y - nb->Position().y) < 8.0f,
          "grupo negativo nunca colide entre si (empilham no mesmo sitio)");

    kx::World jointed(glm::vec2(0.0f, 500.0f));
    jointed.CreateEdge(glm::vec2(-1000.0f, 400.0f), glm::vec2(1000.0f, 400.0f));
    glm::vec2 pos(0.0f, 300.0f);
    kx::Body *chassis = jointed.CreateBox(pos, 40.0f, 10.0f, 1.0f);
    kx::Body *wheel = jointed.CreateCircle(pos + glm::vec2(0.0f, 8.0f), 14.0f, 1.0f);
    kx::WheelJoint *j = new kx::WheelJoint(chassis, wheel, pos + glm::vec2(0.0f, 8.0f), glm::vec2(0.0f, -1.0f));
    jointed.AddJoint(j);

    for (int i = 0; i < 120; ++i)
        jointed.Step(1.0f / 60.0f);
    j->SetMotor(true, 15.0f, 5000000.0f);
    float x0 = chassis->Position().x;
    for (int i = 0; i < 180; ++i)
        jointed.Step(1.0f / 60.0f);
    CHECK(fabsf(wheel->AngularVelocity() - 15.0f) < 2.0f,
          "roda sobreposta ao chassis gira na mesma (collideConnected=false)");
    CHECK(chassis->Position().x - x0 > 150.0f, "carro com roda sobreposta anda");
}

static void TestAddReturnValuesAndDefaultFilter()
{
    kx::World world(glm::vec2(0.0f, 500.0f));

    kx::Body *body = world.CreateBody(kx::BodyType::Dynamic, glm::vec2(0.0f, 0.0f));
    CHECK(body->AddCircle(glm::vec2(0.0f, 0.0f), 5.0f, 1.0f) == 1, "AddCircle devolve 1 quando ha espaco");

    glm::vec2 degenerate[2] = {glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f)};
    CHECK(body->AddPolygon(degenerate, 2, 1.0f) == 0, "AddPolygon com menos de 3 pontos devolve 0");

    int added = 0;
    for (int i = 0; i < kx::Body::kMaxShapes + 5; ++i)
        added += body->AddCircle(glm::vec2(0.0f, 0.0f), 1.0f, 1.0f);
    CHECK(added == kx::Body::kMaxShapes - 1, "soma dos valores devolvidos por AddCircle para no limite de formas");
    CHECK(body->ShapeCount() == kx::Body::kMaxShapes, "corpo satura em kMaxShapes formas");
    CHECK(body->AddCircle(glm::vec2(0.0f, 0.0f), 1.0f, 1.0f) == 0, "AddCircle devolve 0 quando o corpo esta cheio");

    kx::Body *filtered = world.CreateBody(kx::BodyType::Dynamic, glm::vec2(0.0f, 0.0f));
    filtered->SetFilter(4, 0xFFFF, 0);
    filtered->AddCircle(glm::vec2(0.0f, 0.0f), 5.0f, 1.0f);
    filtered->AddBox(5.0f, 5.0f, glm::vec2(10.0f, 0.0f), 1.0f);
    bool allFiltered = true;
    for (int i = 0; i < filtered->ShapeCount(); ++i)
        if (filtered->Shapes()[i].filter.category != 4)
            allFiltered = false;
    CHECK(allFiltered, "SetFilter antes de Add* aplica-se tambem as formas adicionadas depois");
}

static void TestRevolutePendulumLimit()
{
    kx::World world(glm::vec2(0.0f, 500.0f));

    kx::Body *anchor = world.CreateBody(kx::BodyType::Static, glm::vec2(0.0f, 0.0f));
    kx::Body *bob = world.CreateBox(glm::vec2(100.0f, 0.0f), 20.0f, 10.0f, 1.0f);

    kx::RevoluteJoint *pin = new kx::RevoluteJoint(anchor, bob, glm::vec2(0.0f, 0.0f));
    pin->SetLimits(true, -0.5f, 0.5f);
    world.AddJoint(pin);

    glm::vec2 startPos = bob->Position();

    for (int i = 0; i < 300; ++i)
        world.Step(1.0f / 60.0f);

    CHECK(glm::length(bob->Position() - startPos) > 20.0f, "pendulo revolute oscila (posicao mudou)");
    CHECK(bob->Angle() <= 0.5f + 0.1f && bob->Angle() >= -0.5f - 0.1f, "limite do revolute mantem o angulo dentro do intervalo");
}

static void TestRevoluteMotorSpinsWheel()
{
    kx::World world(glm::vec2(0.0f, 0.0f));

    kx::Body *anchor = world.CreateBody(kx::BodyType::Static, glm::vec2(50.0f, 0.0f));
    kx::Body *wheel = world.CreateCircle(glm::vec2(50.0f, 0.0f), 15.0f, 1.0f);

    kx::RevoluteJoint *pin = new kx::RevoluteJoint(anchor, wheel, glm::vec2(50.0f, 0.0f));
    pin->SetMotor(true, 10.0f, 1.0e7f);
    world.AddJoint(pin);

    for (int i = 0; i < 120; ++i)
        world.Step(1.0f / 60.0f);

    CHECK(fabsf(wheel->AngularVelocity() - 10.0f) < 1.0f, "motor do revolute acelera a roda ate a velocidade alvo");
}

static void TestDistanceRigidKeepsLength()
{
    kx::World world(glm::vec2(0.0f, 500.0f));

    kx::Body *anchor = world.CreateBody(kx::BodyType::Static, glm::vec2(0.0f, 0.0f));
    kx::Body *bob = world.CreateCircle(glm::vec2(0.0f, 150.0f), 10.0f, 1.0f);

    kx::DistanceJoint *rope = new kx::DistanceJoint(anchor, bob, glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 150.0f));
    world.AddJoint(rope);

    for (int i = 0; i < 180; ++i)
        world.Step(1.0f / 60.0f);

    float dist = glm::length(bob->Position() - anchor->Position());
    CHECK(fabsf(dist - 150.0f) < 3.0f, "distance rigida mantem o comprimento sob gravidade");
}

static void TestDistanceSpringOscillatesThenDamps()
{
    kx::World world(glm::vec2(0.0f, 500.0f));

    kx::Body *anchor = world.CreateBody(kx::BodyType::Static, glm::vec2(0.0f, 0.0f));
    kx::Body *bob = world.CreateCircle(glm::vec2(0.0f, 150.0f), 10.0f, 1.0f);

    kx::DistanceJoint *spring = new kx::DistanceJoint(anchor, bob, glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 150.0f));
    spring->SetSpring(2.0f, 0.05f);
    spring->SetLengthRange(0.0f, 1000.0f);
    world.AddJoint(spring);

    float maxDist = 0.0f;
    for (int i = 0; i < 600; ++i)
    {
        world.Step(1.0f / 60.0f);
        float dist = glm::length(bob->Position() - anchor->Position());
        if (dist > maxDist)
            maxDist = dist;
    }

    CHECK(maxDist > 154.0f, "distance em modo mola estica alem do comprimento de repouso (overshoot)");
    CHECK(glm::length(bob->Velocity()) < 5.0f, "distance em modo mola amortece ate quase parar");
}

static void TestGearJointCounterRotates()
{
    kx::World world(glm::vec2(0.0f, 0.0f));

    kx::Body *anchorA = world.CreateBody(kx::BodyType::Static, glm::vec2(-60.0f, 0.0f));
    kx::Body *wheelA = world.CreateCircle(glm::vec2(-60.0f, 0.0f), 20.0f, 1.0f);
    kx::RevoluteJoint *jointA = new kx::RevoluteJoint(anchorA, wheelA, glm::vec2(-60.0f, 0.0f));
    jointA->SetMotor(true, 6.0f, 1.0e7f);
    world.AddJoint(jointA);

    kx::Body *anchorB = world.CreateBody(kx::BodyType::Static, glm::vec2(60.0f, 0.0f));
    kx::Body *wheelB = world.CreateCircle(glm::vec2(60.0f, 0.0f), 20.0f, 1.0f);
    kx::RevoluteJoint *jointB = new kx::RevoluteJoint(anchorB, wheelB, glm::vec2(60.0f, 0.0f));
    world.AddJoint(jointB);

    kx::GearJoint *gear = new kx::GearJoint(jointA, jointB, 2.0f);
    world.AddJoint(gear);

    for (int i = 0; i < 180; ++i)
        world.Step(1.0f / 60.0f);

    float wA = wheelA->AngularVelocity();
    float wB = wheelB->AngularVelocity();
    float expectedB = -wA / 2.0f;

    CHECK(wA > 4.0f, "engrenagem: roda motora acelera");
    CHECK(fabsf(wB - expectedB) < 1.0f, "engrenagem: roda B segue wB = -wA/ratio (formula do b2GearJoint)");
    CHECK(wB < 0.0f, "engrenagem: roda B gira em sentido contrario a roda A");
}

static void TestWheelSpringChangesBounceDepth()
{
    float stiffDrop, softDrop;

    {
        kx::World world(glm::vec2(0.0f, 500.0f));
        world.CreateStaticBox(glm::vec2(0.0f, 300.0f), 500.0f, 10.0f);

        kx::Body *chassis = world.CreateBox(glm::vec2(0.0f, 0.0f), 40.0f, 12.0f, 1.0f);
        kx::Body *wheel = world.CreateCircle(glm::vec2(0.0f, 40.0f), 15.0f, 1.0f);
        kx::WheelJoint *joint = new kx::WheelJoint(chassis, wheel, glm::vec2(0.0f, 40.0f), glm::vec2(0.0f, -1.0f));
        joint->SetSpring(10.0f, 0.1f);
        world.AddJoint(joint);

        float maxY = chassis->Position().y;
        for (int i = 0; i < 90; ++i)
        {
            world.Step(1.0f / 60.0f);
            if (chassis->Position().y > maxY)
                maxY = chassis->Position().y;
        }
        stiffDrop = maxY;
    }

    {
        kx::World world(glm::vec2(0.0f, 500.0f));
        world.CreateStaticBox(glm::vec2(0.0f, 300.0f), 500.0f, 10.0f);

        kx::Body *chassis = world.CreateBox(glm::vec2(0.0f, 0.0f), 40.0f, 12.0f, 1.0f);
        kx::Body *wheel = world.CreateCircle(glm::vec2(0.0f, 40.0f), 15.0f, 1.0f);
        kx::WheelJoint *joint = new kx::WheelJoint(chassis, wheel, glm::vec2(0.0f, 40.0f), glm::vec2(0.0f, -1.0f));
        joint->SetSpring(0.7f, 0.1f);
        world.AddJoint(joint);

        float maxY = chassis->Position().y;
        for (int i = 0; i < 90; ++i)
        {
            world.Step(1.0f / 60.0f);
            if (chassis->Position().y > maxY)
                maxY = chassis->Position().y;
        }
        softDrop = maxY;
    }

    CHECK(fabsf(stiffDrop - softDrop) > 3.0f, "WheelJoint::SetSpring muda a profundidade do ressalto de forma mensuravel");
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
    TestDeepOverlapNoExplosion();
    TestFiltersAndCollideConnected();
    TestWheelJointHoldsWheel();
    TestWheelJointMotorSpinsWheel();
    TestAddPolygonTriangleRests();
    TestConcaveMeshLShapeRests();
    TestConcaveMeshStarSettles();
    TestAddReturnValuesAndDefaultFilter();
    TestRevolutePendulumLimit();
    TestRevoluteMotorSpinsWheel();
    TestDistanceRigidKeepsLength();
    TestDistanceSpringOscillatesThenDamps();
    TestGearJointCounterRotates();
    TestWheelSpringChangesBounceDepth();

    if (gFailures)
    {
        std::printf("RESULTADO: %d FALHAS\n", gFailures);
        return 1;
    }
    std::printf("RESULTADO: solver limpo\n");
    return 0;
}
