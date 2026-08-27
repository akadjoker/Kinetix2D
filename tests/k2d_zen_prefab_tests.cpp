#include <k2d/GameObject.h>
#include <k2d/Prefab.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>
#include <k2d/ZenRuntime.h>
#include <k2d/ZenScriptComponent.h>

#include "k2d_test_paths.h"

#include <cmath>
#include <cstdio>

static bool nearEqual(double a, double b, double tolerance = 0.001)
{
    return std::fabs(a - b) < tolerance;
}

static const std::string kBallScript = k2d_tests::tempPath("k2d_zen_ball.py");
static const std::string kThrowerScript = k2d_tests::tempPath("k2d_zen_thrower.py");
static const std::string kBallPrefab = k2d_tests::tempPath("k2d_zen_ball.k2dprefab");

static bool writeScripts()
{
    FILE *f = std::fopen(kBallScript.c_str(), "w");
    if (!f)
        return false;
    std::fputs("class Ball:\n"
               "    def __init__(self, node):\n"
               "        self.node = node\n"
               "        self.speed = 100\n"
               "\n"
               "    def on_start(self):\n"
               "        set_number(\"balls\", get_number(\"balls\", 0) + 1)\n"
               "\n"
               "    def on_update(self, dt):\n"
               "        self.node.translate(self.speed * dt, 0)\n",
               f);
    if (std::fclose(f) != 0)
        return false;

    f = std::fopen(kThrowerScript.c_str(), "w");
    if (!f)
        return false;
    std::fprintf(f,
                 "class Thrower:\n"
                 "    def __init__(self, node):\n"
                 "        self.node = node\n"
                 "        self.thrown = False\n"
                 "\n"
                 "    def on_update(self, dt):\n"
                 "        if self.thrown == False:\n"
                 "            self.thrown = True\n"
                 "            self.node.spawn(\"%s\", 50, 70)\n",
                 kBallPrefab.c_str());
    return std::fclose(f) == 0;
}

static bool writeBallPrefab(double speedOverride)
{
    k2d::Scene scene;
    k2d::GameObject *ball = scene.createObject("ball");
    k2d::ZenScriptComponent *script = ball->addComponent<k2d::ZenScriptComponent>();
    if (!script->loadFile(kBallScript.c_str()))
        return false;
    script->setNumberOverride("speed", speedOverride, true);

    k2d::Prefab prefab;
    return prefab.SaveToFile(kBallPrefab.c_str(), *ball);
}

static bool testPrefabInstantiateCarriesTheScript()
{
    if (!writeScripts() || !writeBallPrefab(400.0))
        return false;

    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    const std::size_t compilesBefore = k2d::ZenRuntime::instance().compileCount();

    k2d::Prefab prefab;
    bool ok = prefab.Load(kBallPrefab.c_str());

    k2d::Scene scene;
    k2d::GameObject *first = prefab.Instantiate(scene);
    k2d::GameObject *second = prefab.Instantiate(scene);
    ok = ok && first && second;

    k2d::ZenScriptComponent *firstScript =
        first ? first->getComponent<k2d::ZenScriptComponent>() : nullptr;
    ok = ok && firstScript && firstScript->loaded();
    ok = ok && firstScript && firstScript->overrideCount() == 1;

    if (second)
        if (k2d::ZenScriptComponent *secondScript = second->getComponent<k2d::ZenScriptComponent>())
            secondScript->setNumberOverride("speed", 50.0, true);

    scene.update(1.0f);

    ok = ok && first && nearEqual(first->position().x, 400.0);
    ok = ok && second && nearEqual(second->position().x, 50.0);
    ok = ok && nearEqual(k2d::ZenBlackboard::getNumber("balls"), 2.0);
    const std::size_t compiles = k2d::ZenRuntime::instance().compileCount() - compilesBefore;
    ok = ok && compiles == 1;

    std::printf("  instantiate: first=%.1f second=%.1f compiles=%d\n",
                first ? first->position().x : 0.0f, second ? second->position().x : 0.0f,
                (int)compiles);
    return ok;
}

static bool testScriptSpawnsPrefabWithItsOwnScript()
{
    if (!writeScripts() || !writeBallPrefab(300.0))
        return false;

    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    k2d::GameObject *thrower = scene.createObject("thrower");
    k2d::ZenScriptComponent *script = thrower->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadFile(kThrowerScript.c_str());

    scene.update(0.5f);

    k2d::GameObject *ball = scene.find("ball");
    ok = ok && ball != nullptr;

    k2d::ZenScriptComponent *ballScript =
        ball ? ball->getComponent<k2d::ZenScriptComponent>() : nullptr;
    ok = ok && ballScript && (ballScript->loaded() || ballScript->pendingLoad());
    ok = ok && ballScript && ballScript->overrideCount() == 1;
    ok = ok && ball && nearEqual(ball->position().x, 50.0) && nearEqual(ball->position().y, 70.0);

    scene.update(1.0f);

    ok = ok && ballScript && ballScript->loaded() && !ballScript->pendingLoad();
    ok = ok && ball && nearEqual(ball->position().x, 350.0);
    ok = ok && nearEqual(k2d::ZenBlackboard::getNumber("balls"), 1.0);

    std::printf("  spawn_from_script: pos=(%.1f, %.1f) balls=%g\n", ball ? ball->position().x : 0.0f,
                ball ? ball->position().y : 0.0f, k2d::ZenBlackboard::getNumber("balls"));
    return ok;
}

static bool testSpawnedPrefabsAreIndependent()
{
    if (!writeScripts() || !writeBallPrefab(200.0))
        return false;

    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    const std::size_t compilesBefore = k2d::ZenRuntime::instance().compileCount();

    k2d::Prefab prefab;
    bool ok = prefab.Load(kBallPrefab.c_str());

    k2d::Scene scene;
    for (int i = 0; i < 50; ++i)
    {
        k2d::GameObject *ball = prefab.Instantiate(scene);
        if (!ball)
        {
            ok = false;
            break;
        }
        if (k2d::ZenScriptComponent *ballScript = ball->getComponent<k2d::ZenScriptComponent>())
            ballScript->setNumberOverride("speed", (double)i, true);
    }

    scene.update(1.0f);

    for (size_t i = 0; i < scene.root().childCount() && ok; ++i)
        ok = nearEqual(scene.root().child(i)->position().x, (double)i);

    ok = ok && nearEqual(k2d::ZenBlackboard::getNumber("balls"), 50.0);
    const std::size_t compiles = k2d::ZenRuntime::instance().compileCount() - compilesBefore;
    ok = ok && compiles == 1;

    std::printf("  independent: objects=%d compiles=%d\n", (int)scene.root().childCount(),
                (int)compiles);
    return ok;
}

int main()
{
    k2d::SetZenScriptsEnabled(true);
    k2d::RegisterZenScriptSerializer();

    const bool instantiate = testPrefabInstantiateCarriesTheScript();
    const bool spawned = testScriptSpawnsPrefabWithItsOwnScript();
    const bool independent = testSpawnedPrefabsAreIndependent();

    std::remove(kBallScript.c_str());
    std::remove(kThrowerScript.c_str());
    std::remove(kBallPrefab.c_str());

    std::printf("zen prefab: instantiate=%s spawn_from_script=%s independent=%s\n",
                instantiate ? "pass" : "fail", spawned ? "pass" : "fail",
                independent ? "pass" : "fail");
    return instantiate && spawned && independent ? 0 : 1;
}
