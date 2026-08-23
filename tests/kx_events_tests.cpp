#include <kx/kx.h>

#include <cmath>
#include <cstdio>

static int gFailures = 0;

#define CHECK(cond, name)                                      \
    do                                                         \
    {                                                          \
        if (cond)                                              \
            std::printf("[PASS] %s\n", name);                 \
        else                                                   \
        {                                                      \
            std::printf("[FAIL] %s (linha %d)\n", name, __LINE__); \
            ++gFailures;                                       \
        }                                                      \
    } while (0)

struct EventLog
{
    int begin;
    int persist;
    int end;
    bool sensor;
    bool userData;
};

static void CountEvent(const kx::ContactEvent &event, void *context)
{
    EventLog *log = static_cast<EventLog *>(context);
    if (event.phase == kx::ContactPhase::Begin)
        ++log->begin;
    else if (event.phase == kx::ContactPhase::Persist)
        ++log->persist;
    else
        ++log->end;

    log->sensor = log->sensor || event.sensor;
    log->userData = event.self->UserData() != nullptr &&
                    event.other->UserData() != nullptr &&
                    event.self->ShapeUserData(event.shapeIndexSelf) != nullptr &&
                    event.other->ShapeUserData(event.shapeIndexOther) != nullptr;
}

static void TestBeginPersistEnd()
{
    kx::World world(Math::Vec2(0.0f));
    kx::Body *wall = world.CreateStaticBox(Math::Vec2(0.0f), 20.0f, 20.0f);
    kx::Body *box = world.CreateBox(Math::Vec2(0.0f), 10.0f, 10.0f, 1.0f);
    EventLog log{0, 0, 0, false, false};
    wall->SetContactCallback(&CountEvent, &log);

    world.Step(1.0f / 60.0f);
    world.Step(1.0f / 60.0f);
    box->SetPosition(Math::Vec2(200.0f, 0.0f));
    world.Step(1.0f / 60.0f);

    CHECK(log.begin == 1, "callback Begin emitido uma vez");
    CHECK(log.persist >= 1, "callback Persist emitido enquanto sobreposto");
    CHECK(log.end == 1, "callback End emitido ao separar");
}

static void TestSensorPassThroughAndUserData()
{
    kx::World world(Math::Vec2(0.0f));
    kx::Body *sensor = world.CreateStaticBox(Math::Vec2(0.0f), 10.0f, 20.0f);
    kx::Body *ball = world.CreateCircle(Math::Vec2(-40.0f, 0.0f), 4.0f, 1.0f);
    int sensorBodyTag = 1;
    int ballBodyTag = 2;
    int sensorShapeTag = 3;
    int ballShapeTag = 4;
    sensor->SetUserData(&sensorBodyTag);
    ball->SetUserData(&ballBodyTag);
    sensor->SetShapeUserData(0, &sensorShapeTag);
    ball->SetShapeUserData(0, &ballShapeTag);
    sensor->SetSensor(0, true);

    EventLog log{0, 0, 0, false, false};
    sensor->SetContactCallback(&CountEvent, &log);
    ball->SetVelocity(Math::Vec2(60.0f, 0.0f));

    for (int i = 0; i < 100; ++i)
        world.Step(1.0f / 60.0f);

    CHECK(log.begin == 1 && log.persist > 0 && log.end == 1,
          "sensor emite Begin Persist End durante travessia");
    CHECK(log.sensor, "evento identifica contacto sensor");
    CHECK(log.userData, "callback acede a userData de bodies e shapes");
    CHECK(ball->Position().x > 50.0f && std::fabs(ball->Velocity().x - 60.0f) < 0.01f,
          "sensor nao produz resposta fisica");
}

static void TestSolidStillBlocks()
{
    kx::World world(Math::Vec2(0.0f));
    world.CreateStaticBox(Math::Vec2(0.0f), 10.0f, 20.0f);
    kx::Body *ball = world.CreateCircle(Math::Vec2(-40.0f, 0.0f), 4.0f, 1.0f);
    ball->SetVelocity(Math::Vec2(60.0f, 0.0f));

    for (int i = 0; i < 100; ++i)
        world.Step(1.0f / 60.0f);

    CHECK(ball->Position().x < -10.0f, "shape solida equivalente continua a bloquear");
}

static void TestDestroyEmitsEnd()
{
    kx::World world(Math::Vec2(0.0f));
    kx::Body *sensor = world.CreateStaticBox(Math::Vec2(0.0f), 20.0f, 20.0f);
    sensor->SetSensor(0, true);
    kx::Body *box = world.CreateBox(Math::Vec2(0.0f), 10.0f, 10.0f, 1.0f);
    EventLog log{0, 0, 0, false, false};
    sensor->SetContactCallback(&CountEvent, &log);

    world.Step(1.0f / 60.0f);
    world.Destroy(box);

    CHECK(log.begin == 1 && log.end == 1, "Destroy emite End para contacto ativo");
}

static void TestShapeIndicesRemainUnique()
{
    kx::World world(Math::Vec2(0.0f));
    kx::Body *sensor = world.CreateBody(kx::BodyType::Static, Math::Vec2(0.0f));
    kx::Body *body = world.CreateBody(kx::BodyType::Dynamic, Math::Vec2(0.0f));
    for (int i = 0; i < 9; ++i)
    {
        sensor->AddCircle(Math::Vec2(0.0f), 10.0f, 0.0f);
        sensor->SetSensor(i, true);
        body->AddCircle(Math::Vec2(0.0f), 5.0f, 1.0f);
    }

    EventLog log{0, 0, 0, false, false};
    sensor->SetContactCallback(&CountEvent, &log);
    world.Step(1.0f / 60.0f);

    CHECK(log.begin == 81, "chave de contacto distingue indices de shape acima de 7");
}

int main()
{
    TestBeginPersistEnd();
    TestSensorPassThroughAndUserData();
    TestSolidStillBlocks();
    TestDestroyEmitsEnd();
    TestShapeIndicesRemainUnique();

    std::printf("%d failures\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}