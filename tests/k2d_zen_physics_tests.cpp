#include <k2d/BoxCollider2D.h>
#include <k2d/CircleCollider2D.h>
#include <k2d/CharacterBody2D.h>
#include <k2d/GameObject.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Scene.h>
#include <k2d/ZenRuntime.h>
#include <k2d/ZenScriptComponent.h>

#include <cmath>
#include <cstdio>

static bool nearEqual(double a, double b, double tolerance = 1.0)
{
    return std::fabs(a - b) < tolerance;
}

static k2d::GameObject* makeBox(k2d::Scene& scene, const char* name, const Math::Vec2& position, const Math::Vec2& size,
                                k2d::BodyType type)
{
    k2d::GameObject* object = scene.createObject(name);
    object->setPosition(position);
    object->addComponent<k2d::RigidBody2D>()->setBodyType(type);
    object->addComponent<k2d::BoxCollider2D>()->setSize(size);
    return object;
}

static bool testBodyHandleDrivesTheSimulation()
{
    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    k2d::GameObject* object =
        makeBox(scene, "rocket", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class Rocket:\n"
                                 "    def __init__(self, node):\n"
                                 "        self.node = node\n"
                                 "\n"
                                 "    def on_start(self):\n"
                                 "        body = self.node.get_component<RigidBody>()\n"
                                 "        set_flag(\"has_body\", body != None)\n"
                                 "        body.set_gravity_scale(0)\n"
                                 "        body.set_velocity(120, 0)\n"
                                 "\n"
                                 "    def on_update(self, dt):\n"
                                 "        body = self.node.get_component<RigidBody>()\n"
                                 "        vx, vy = body.get_velocity()\n"
                                 "        set_number(\"vx\", vx)\n",
                                 "rocket");

    scene.setSimulationEnabled(true);

    for (int i = 0; i < 60; ++i)
        scene.update(1.0f / 60.0f);

    ok = ok && k2d::ZenBlackboard::getBool("has_body", false);
    ok = ok && nearEqual(k2d::ZenBlackboard::getNumber("vx"), 120.0, 1.0);
    ok = ok && nearEqual(object->position().x, 120.0, 5.0);
    ok = ok && nearEqual(object->position().y, 0.0, 1.0);

    std::printf("  body_handle: x=%.1f vx=%g\n", object->position().x, k2d::ZenBlackboard::getNumber("vx"));
    return ok;
}

static bool testImpulseFromScript()
{
    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    k2d::GameObject* object =
        makeBox(scene, "jumper", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);
    object->addComponent<k2d::ZenScriptComponent>()->loadSource(
        "class Jumper:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "        self.fired = False\n"
        "\n"
        "    def on_update(self, dt):\n"
        "        if self.fired == False:\n"
        "            self.fired = True\n"
        "            self.node.get_body().apply_impulse(0, -8000)\n",
        "jumper");

    scene.setGravity(Math::Vec2(0.0f, 0.0f));
    scene.setSimulationEnabled(true);

    for (int i = 0; i < 30; ++i)
        scene.update(1.0f / 60.0f);

    const k2d::RigidBody2D* body = object->getComponent<k2d::RigidBody2D>();
    const float expected = -8000.0f / (40.0f * 40.0f * 1.0f);
    bool ok = body && nearEqual(body->velocity().y, expected, 0.01);
    ok = ok && object->position().y < -1.0f;

    std::printf("  impulse: vy=%.2f (impulse/mass = %.2f) y=%.1f\n", body ? body->velocity().y : 0.0f, expected,
                object->position().y);
    return ok;
}

static bool testCollisionCallbackReachesScripts()
{
    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    makeBox(scene, "floor", Math::Vec2(0.0f, 300.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);
    k2d::GameObject* faller =
        makeBox(scene, "faller", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    faller->addComponent<k2d::ZenScriptComponent>()->loadSource(
        "class Faller:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "\n"
        "    def on_collision(self, other, began):\n"
        "        if began:\n"
        "            set_number(\"hits\", get_number(\"hits\", 0) + 1)\n"
        "            set_string(\"hit_name\", other.get_name())\n",
        "faller");

    k2d::RouteZenScriptCollisions(scene);
    scene.setSimulationEnabled(true);

    for (int i = 0; i < 180; ++i)
        scene.update(1.0f / 60.0f);

    bool ok = k2d::ZenBlackboard::getNumber("hits", 0.0) >= 1.0;
    ok = ok && k2d::ZenBlackboard::getString("hit_name") == ct::String("floor");

    std::printf("  on_collision: hits=%g other=%s\n", k2d::ZenBlackboard::getNumber("hits", 0.0),
                k2d::ZenBlackboard::getString("hit_name").c_str());
    return ok;
}

static bool testSensorReportsToScript()
{
    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    k2d::GameObject* trigger = scene.createObject("trigger");
    trigger->setPosition(Math::Vec2(0.0f, 150.0f));
    trigger->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    k2d::BoxCollider2D* sensorShape = trigger->addComponent<k2d::BoxCollider2D>();
    sensorShape->setSize(Math::Vec2(400.0f, 20.0f));
    sensorShape->setSensor(true);

    trigger->addComponent<k2d::ZenScriptComponent>()->loadSource(
        "class Trigger:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "\n"
        "    def on_collision(self, other, began):\n"
        "        if began:\n"
        "            set_string(\"entered\", other.get_name())\n"
        "        else:\n"
        "            set_string(\"left\", other.get_name())\n",
        "trigger");

    makeBox(scene, "floor", Math::Vec2(0.0f, 500.0f), Math::Vec2(600.0f, 40.0f), k2d::BodyType::Static);
    makeBox(scene, "diver", Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 40.0f), k2d::BodyType::Dynamic);

    k2d::RouteZenScriptCollisions(scene);
    scene.setSimulationEnabled(true);

    for (int i = 0; i < 240; ++i)
        scene.update(1.0f / 60.0f);

    bool ok = k2d::ZenBlackboard::getString("entered") == ct::String("diver");
    ok = ok && k2d::ZenBlackboard::getString("left") == ct::String("diver");

    std::printf("  sensor_script: entered=%s left=%s\n", k2d::ZenBlackboard::getString("entered").c_str(),
                k2d::ZenBlackboard::getString("left").c_str());
    return ok;
}

static bool testRaycastAndGravityFromScript()
{
    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    makeBox(scene, "wall", Math::Vec2(200.0f, 0.0f), Math::Vec2(40.0f, 200.0f), k2d::BodyType::Static);

    k2d::GameObject* scanner = scene.createObject("scanner");
    scanner->addComponent<k2d::ZenScriptComponent>()->loadSource("class Scanner:\n"
                                                                 "    def __init__(self, node):\n"
                                                                 "        self.node = node\n"
                                                                 "\n"
                                                                 "    def on_start(self):\n"
                                                                 "        hit, hx, hy = raycast(0, 0, 1, 0, 500)\n"
                                                                 "        if hit != None:\n"
                                                                 "            set_string(\"seen\", hit.get_name())\n"
                                                                 "            set_number(\"hit_x\", hx)\n"
                                                                 "        set_gravity(0, 0)\n"
                                                                 "        gx, gy = get_gravity()\n"
                                                                 "        set_number(\"gy\", gy)\n",
                                                                 "scanner");

    scene.setSimulationEnabled(true);

    for (int i = 0; i < 3; ++i)
        scene.update(1.0f / 60.0f);

    bool ok = k2d::ZenBlackboard::getString("seen") == ct::String("wall");
    ok = ok && nearEqual(k2d::ZenBlackboard::getNumber("hit_x"), 180.0, 2.0);
    ok = ok && nearEqual(k2d::ZenBlackboard::getNumber("gy", -1.0), 0.0, 0.001);

    std::printf("  queries: seen=%s hit_x=%g gravity_y=%g\n", k2d::ZenBlackboard::getString("seen").c_str(),
                k2d::ZenBlackboard::getNumber("hit_x"), k2d::ZenBlackboard::getNumber("gy"));
    return ok;
}

static bool testBodyHandleIsNoneWithoutRigidBody()
{
    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("plain");
    object->addComponent<k2d::ZenScriptComponent>()->loadSource(
        "class Plain:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "\n"
        "    def on_start(self):\n"
        "        set_flag(\"none\", self.node.get_body() == None)\n",
        "plain");

    scene.update(1.0f / 60.0f);

    const bool ok = k2d::ZenBlackboard::getBool("none", false);
    std::printf("  no_body: get_body() returns %s\n", ok ? "None" : "something");
    return ok;
}

static bool testCharacterMovementBindings()
{
    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    makeBox(scene, "wall", Math::Vec2(0.0f, 0.0f), Math::Vec2(20.0f, 160.0f), k2d::BodyType::Static);
    k2d::GameObject* player =
        makeBox(scene, "player", Math::Vec2(-80.0f, 0.0f), Math::Vec2(20.0f, 20.0f), k2d::BodyType::Kinematic);
    player->addComponent<k2d::CharacterBody2D>();
    player->addComponent<k2d::ZenScriptComponent>()->loadSource(
        "class Player:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "\n"
        "    def on_start(self):\n"
        "        set_flag(\"free\", self.node.place_free(-80, 0))\n"
        "        meeting = self.node.place_meeting(0, 0)\n"
        "        set_flag(\"meeting\", meeting != None)\n"
        "        other, hx, hy, nx, ny = self.node.move_and_collide(160, 0)\n"
        "        set_flag(\"collision\", other != None)\n"
        "        set_number(\"normal_x\", nx)\n"
        "        hit, vx, vy, floor, wall, ceiling = self.node.move_and_slide(0, 0)\n"
        "        set_flag(\"slide_tuple\", floor == False and wall == False and ceiling == False)\n",
        "player");

    scene.setGravity(Math::Vec2(0.0f, 0.0f));
    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    const bool ok = k2d::ZenBlackboard::getBool("free", false) && k2d::ZenBlackboard::getBool("meeting", false) &&
                    k2d::ZenBlackboard::getBool("collision", false) &&
                    k2d::ZenBlackboard::getBool("slide_tuple", false) &&
                    k2d::ZenBlackboard::getNumber("normal_x") < -0.99;
    std::printf("  character_script: free=%d meeting=%d collision=%d normal_x=%g\n",
                k2d::ZenBlackboard::getBool("free", false) ? 1 : 0,
                k2d::ZenBlackboard::getBool("meeting", false) ? 1 : 0,
                k2d::ZenBlackboard::getBool("collision", false) ? 1 : 0, k2d::ZenBlackboard::getNumber("normal_x"));
    return ok;
}

int main()
{
    k2d::SetZenScriptsEnabled(true);

    const bool handle = testBodyHandleDrivesTheSimulation();
    const bool impulse = testImpulseFromScript();
    const bool collision = testCollisionCallbackReachesScripts();
    const bool sensor = testSensorReportsToScript();
    const bool queries = testRaycastAndGravityFromScript();
    const bool noBody = testBodyHandleIsNoneWithoutRigidBody();
    const bool character = testCharacterMovementBindings();

    std::printf("zen physics: body_handle=%s impulse=%s on_collision=%s sensor=%s queries=%s "
                "no_body=%s character=%s\n",
                handle ? "pass" : "fail", impulse ? "pass" : "fail", collision ? "pass" : "fail",
                sensor ? "pass" : "fail", queries ? "pass" : "fail", noBody ? "pass" : "fail",
                character ? "pass" : "fail");
    return handle && impulse && collision && sensor && queries && noBody && character ? 0 : 1;
}
