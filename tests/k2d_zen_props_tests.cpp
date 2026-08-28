#include <k2d/GameObject.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>
#include <k2d/ZenRuntime.h>
#include <k2d/ZenScriptComponent.h>

#include "k2d_test_paths.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static bool nearEqual(double a, double b, double tolerance = 0.001)
{
    return std::fabs(a - b) < tolerance;
}

static const std::string kShipPath = k2d_tests::tempPath("k2d_zen_ship.py");

static const char *kShipSource =
    "SPEED = 200\n"
    "\n"
    "class Ship:\n"
    "    def __init__(self, node):\n"
    "        self.node = node\n"
    "        self.speed = SPEED\n"
    "        self.turn = 1.5\n"
    "        self.label = \"alpha\"\n"
    "        self.armed = True\n"
    "        self._timer = 0.0\n"
    "        self.lives = 3  # trailing comment\n"
    "        self.computed = self.turn * 2\n"
    "\n"
    "    def on_start(self):\n"
    "        set_number(\"starts\", get_number(\"starts\", 0) + 1)\n"
    "\n"
    "    def on_update(self, dt):\n"
    "        self.node.translate(self.speed * dt, 0)\n"
    "        self.node.rotate(self.turn * dt)\n";

static bool writeShip()
{
    FILE *f = std::fopen(kShipPath.c_str(), "w");
    if (!f)
        return false;
    std::fputs(kShipSource, f);
    return std::fclose(f) == 0;
}

static bool testScanFindsLiterals()
{
    ct::Vector<k2d::ZenScriptProperty> props;
    const std::size_t count = k2d::ScanZenScriptProperties(kShipSource, props);

    bool ok = count == 5;
    ok = ok && props[0].name == "speed" && props[0].kind == k2d::ZenScriptProperty::Kind::Number &&
         nearEqual(props[0].number, 200.0) && props[0].integer;
    ok = ok && props[1].name == "turn" && !props[1].integer && nearEqual(props[1].number, 1.5);
    ok = ok && props[2].name == "label" && props[2].kind == k2d::ZenScriptProperty::Kind::String &&
         props[2].text == "alpha";
    ok = ok && props[3].name == "armed" && props[3].kind == k2d::ZenScriptProperty::Kind::Bool &&
         props[3].flag;
    ok = ok && props[4].name == "lives" && props[4].integer && nearEqual(props[4].number, 3.0);

    std::printf("  scan: found=%d", (int)count);
    for (size_t i = 0; i < props.size(); ++i)
        std::printf(" %s", props[i].name.c_str());
    std::printf("\n");
    return ok;
}

static bool testDeclaredPropertiesComeFromTheClass()
{
    if (!writeShip())
        return false;
    k2d::ZenRuntime::instance().reset();

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("ship");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();

    bool ok = script->loadFile(kShipPath.c_str());
    ok = ok && script->declaredPropertyCount() == 5;
    ok = ok && script->declaredProperty("speed") != nullptr;
    ok = ok && script->declaredProperty("_timer") == nullptr;
    ok = ok && script->declaredProperty("node") == nullptr;
    ok = ok && script->declaredProperty("computed") == nullptr;

    k2d::GameObject *second = scene.createObject("ship2");
    k2d::ZenScriptComponent *cached = second->addComponent<k2d::ZenScriptComponent>();
    ok = ok && cached->loadFile(kShipPath.c_str());
    ok = ok && cached->declaredPropertyCount() == 5;

    std::printf("  declared: count=%d cached_count=%d\n", (int)script->declaredPropertyCount(),
                (int)cached->declaredPropertyCount());
    return ok;
}

static bool testOverridesReachTheInstance()
{
    if (!writeShip())
        return false;
    k2d::ZenRuntime::instance().reset();

    k2d::Scene scene;
    k2d::GameObject *plain = scene.createObject("plain");
    k2d::GameObject *fast = scene.createObject("fast");

    k2d::ZenScriptComponent *a = plain->addComponent<k2d::ZenScriptComponent>();
    k2d::ZenScriptComponent *b = fast->addComponent<k2d::ZenScriptComponent>();

    bool ok = a->loadFile(kShipPath.c_str()) && b->loadFile(kShipPath.c_str());
    b->setNumberOverride("speed", 500.0, true);
    b->setNumberOverride("turn", 90.0);

    scene.update(1.0f);

    ok = ok && nearEqual(plain->position().x, 200.0) && nearEqual(plain->rotationDegrees(), 1.5);
    ok = ok && nearEqual(fast->position().x, 500.0) && nearEqual(fast->rotationDegrees(), 90.0);
    ok = ok && k2d::ZenRuntime::instance().compileCount() >= 1;

    std::printf("  overrides: plain=(%.1f, %.1f) fast=(%.1f, %.1f)\n", plain->position().x,
                plain->rotationDegrees(), fast->position().x, fast->rotationDegrees());
    return ok;
}

static bool testLiveOverrideRetunesRunningInstance()
{
    if (!writeShip())
        return false;
    k2d::ZenRuntime::instance().reset();

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("ship");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();

    bool ok = script->loadFile(kShipPath.c_str());
    scene.update(1.0f);
    ok = ok && nearEqual(object->position().x, 200.0);

    script->setNumberOverride("speed", 10.0, true);
    scene.update(1.0f);
    ok = ok && nearEqual(object->position().x, 210.0);

    script->clearOverride("speed");
    scene.update(1.0f);
    ok = ok && nearEqual(object->position().x, 410.0);

    std::printf("  live_tuning: x=%.1f\n", object->position().x);
    return ok;
}

static bool testClearingAnOverrideKeepsTheInstanceAlive()
{
    if (!writeShip())
        return false;
    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("ship");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();

    bool ok = script->loadFile(kShipPath.c_str());
    script->setNumberOverride("speed", 40.0, true);
    scene.update(0.0f);
    ok = ok && nearEqual(k2d::ZenBlackboard::getNumber("starts"), 1.0);

    script->clearOverride("speed");
    scene.update(0.0f);
    ok = ok && nearEqual(k2d::ZenBlackboard::getNumber("starts"), 1.0);

    std::printf("  keep_alive: starts=%g\n", k2d::ZenBlackboard::getNumber("starts"));
    return ok;
}

static bool testOverridesRoundTripThroughTheSerializer()
{
    if (!writeShip())
        return false;
    k2d::ZenRuntime::instance().reset();
    k2d::RegisterZenScriptSerializer();

    k2d::Scene source;
    k2d::GameObject *object = source.createObject("ship");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();

    bool ok = script->loadFile(kShipPath.c_str());
    script->setNumberOverride("speed", 320.0, true);
    script->setNumberOverride("turn", 2.25);
    script->setStringOverride("label", "beta");
    script->setBoolOverride("armed", false);

    const ct::Json json = k2d::Serializer::WriteObject(*object);

    k2d::Scene target;
    k2d::GameObject *loaded = k2d::Serializer::ReadObject(target, json);
    ok = ok && loaded != nullptr;

    k2d::ZenScriptComponent *restored =
        loaded ? loaded->getComponent<k2d::ZenScriptComponent>() : nullptr;
    ok = ok && restored != nullptr && restored->overrideCount() == 4;

    if (restored)
    {
        const k2d::ZenScriptProperty *speed = restored->findOverride("speed");
        const k2d::ZenScriptProperty *turn = restored->findOverride("turn");
        const k2d::ZenScriptProperty *label = restored->findOverride("label");
        const k2d::ZenScriptProperty *armed = restored->findOverride("armed");
        ok = ok && speed && nearEqual(speed->number, 320.0) && speed->integer;
        ok = ok && turn && nearEqual(turn->number, 2.25) && !turn->integer;
        ok = ok && label && label->text == "beta";
        ok = ok && armed && !armed->flag;

        target.update(1.0f);
        ok = ok && nearEqual(loaded->position().x, 320.0) &&
             nearEqual(loaded->rotationDegrees(), 2.25);
    }

    std::printf("  serializer: overrides=%d x=%.1f\n",
                restored ? (int)restored->overrideCount() : -1,
                loaded ? loaded->position().x : 0.0f);
    return ok;
}

static bool testOverridesSurviveReload()
{
    if (!writeShip())
        return false;
    k2d::ZenRuntime::instance().reset();

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("ship");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();

    bool ok = script->loadFile(kShipPath.c_str());
    script->setNumberOverride("speed", 50.0, true);
    scene.update(1.0f);
    ok = ok && nearEqual(object->position().x, 50.0);

    k2d::ZenRuntime::instance().invalidate(kShipPath.c_str());
    ok = ok && script->loadFile(kShipPath.c_str());
    ok = ok && script->overrideCount() == 1;
    scene.update(1.0f);
    ok = ok && nearEqual(object->position().x, 100.0);

    std::printf("  reload: overrides=%d x=%.1f\n", (int)script->overrideCount(),
                object->position().x);
    return ok;
}

static bool testClassBodyFieldsBecomeProperties()
{
    k2d::ZenRuntime::instance().reset();

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("spinner");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();

    bool ok = script->loadSource("class Spinner:\n"
                                 "    speed = 90.0\n"
                                 "    lives = 3\n"
                                 "    label = \"spin\"\n"
                                 "    armed = True\n"
                                 "    _phase = 0.0\n"
                                 "\n"
                                 "    def on_update(self, dt):\n"
                                 "        self.node.rotate(self.speed * dt)\n"
                                 "\n"
                                 "    def __init__(self, node):\n"
                                 "        self.node = node\n",
                                 "spinner");

    ok = ok && script->declaredPropertyCount() == 4;
    ok = ok && script->declaredProperty("_phase") == nullptr;
    ok = ok && script->declaredProperty("node") == nullptr;

    const k2d::ZenScriptProperty *speed = script->declaredProperty("speed");
    const k2d::ZenScriptProperty *lives = script->declaredProperty("lives");
    const k2d::ZenScriptProperty *label = script->declaredProperty("label");
    const k2d::ZenScriptProperty *armed = script->declaredProperty("armed");
    ok = ok && speed && nearEqual(speed->number, 90.0) && !speed->integer;
    ok = ok && lives && nearEqual(lives->number, 3.0) && lives->integer;
    ok = ok && label && label->text == "spin";
    ok = ok && armed && armed->flag;

    scene.update(1.0f);
    ok = ok && nearEqual(object->rotationDegrees(), 90.0, 0.1);

    script->setNumberOverride("speed", 10.0);
    scene.update(1.0f);
    ok = ok && nearEqual(object->rotationDegrees(), 100.0, 0.1);

    std::printf("  class_body: %d properties, rot=%.1f after override\n",
                (int)script->declaredPropertyCount(), object->rotationDegrees());
    return ok;
}

static bool testClassBodyWinsOverInit()
{
    k2d::ZenRuntime::instance().reset();

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("mixed");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();

    bool ok = script->loadSource("class Mixed:\n"
                                 "    speed = 50.0\n"
                                 "\n"
                                 "    def __init__(self, node):\n"
                                 "        self.node = node\n"
                                 "        self.speed = 999.0\n"
                                 "        self.extra = 7\n",
                                 "mixed");

    ok = ok && script->declaredPropertyCount() == 2;

    const k2d::ZenScriptProperty *speed = script->declaredProperty("speed");
    const k2d::ZenScriptProperty *extra = script->declaredProperty("extra");
    ok = ok && speed && nearEqual(speed->number, 50.0);
    ok = ok && extra && nearEqual(extra->number, 7.0) && extra->integer;

    std::printf("  precedence: speed=%.0f (class body, not the 999 in __init__), extra=%.0f\n",
                speed ? speed->number : -1.0, extra ? extra->number : -1.0);
    return ok;
}

int main()
{
    k2d::SetZenScriptsEnabled(true);

    const bool scan = testScanFindsLiterals();
    const bool classBody = testClassBodyFieldsBecomeProperties();
    const bool precedence = testClassBodyWinsOverInit();
    const bool declared = testDeclaredPropertiesComeFromTheClass();
    const bool overrides = testOverridesReachTheInstance();
    const bool live = testLiveOverrideRetunesRunningInstance();
    const bool alive = testClearingAnOverrideKeepsTheInstanceAlive();
    const bool serialized = testOverridesRoundTripThroughTheSerializer();
    const bool reload = testOverridesSurviveReload();

    std::remove(kShipPath.c_str());
    std::printf("zen props: scan=%s class_body=%s precedence=%s declared=%s overrides=%s live=%s keep_alive=%s serializer=%s reload=%s\n",
                scan ? "pass" : "fail", classBody ? "pass" : "fail",
                precedence ? "pass" : "fail", declared ? "pass" : "fail", overrides ? "pass" : "fail",
                live ? "pass" : "fail", alive ? "pass" : "fail", serialized ? "pass" : "fail",
                reload ? "pass" : "fail");
    return scan && classBody && precedence && declared && overrides && live && alive && serialized && reload ? 0 : 1;
}
