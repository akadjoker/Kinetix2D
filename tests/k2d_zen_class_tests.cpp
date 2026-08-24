#include <k2d/GameObject.h>
#include <k2d/Scene.h>
#include <k2d/ZenRuntime.h>
#include <k2d/ZenScriptComponent.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

static bool nearEqual(float a, float b, float tolerance = 0.001f)
{
    return std::fabs(a - b) < tolerance;
}

static const char *kSpinnerPath = "/tmp/k2d_zen_spinner.py";

static void writeSpinner(float base)
{
    FILE *f = std::fopen(kSpinnerPath, "w");
    std::fprintf(f,
                 "class Spinner:\n"
                 "    def __init__(self, node):\n"
                 "        self.node = node\n"
                 "        self.speed = %g\n"
                 "        self.ticks = 0\n"
                 "    def on_start(self):\n"
                 "        self.node.set_z_index(1)\n"
                 "    def on_update(self, dt):\n"
                 "        self.ticks = self.ticks + 1\n"
                 "        self.node.rotate(self.speed * dt)\n"
                 "    def set_speed(self, value):\n"
                 "        self.speed = value\n",
                 (double)base);
    std::fclose(f);
}

static bool testSharedClassPerInstanceState()
{
    writeSpinner(90.0f);
    k2d::ZenRuntime::instance().reset();
    const std::size_t compilesBefore = k2d::ZenRuntime::instance().compileCount();

    k2d::Scene scene;
    k2d::GameObject *a = scene.createObject("a");
    k2d::GameObject *b = scene.createObject("b");
    k2d::GameObject *c = scene.createObject("c");

    k2d::ZenScriptComponent *sa = a->addComponent<k2d::ZenScriptComponent>();
    k2d::ZenScriptComponent *sb = b->addComponent<k2d::ZenScriptComponent>();
    k2d::ZenScriptComponent *sc = c->addComponent<k2d::ZenScriptComponent>();

    bool ok = sa->loadFile(kSpinnerPath) && sb->loadFile(kSpinnerPath) && sc->loadFile(kSpinnerPath);

    scene.update(0.0f);
    ok = ok && sb->callFunction("set_speed", 180.0);
    ok = ok && sc->callFunction("set_speed", 270.0);

    scene.update(1.0f);

    ok = ok && nearEqual(a->rotationDegrees(), 90.0f, 0.01f);
    ok = ok && nearEqual(b->rotationDegrees(), 180.0f, 0.01f);
    ok = ok && nearEqual(c->rotationDegrees(), 270.0f, 0.01f);

    const std::size_t compiles = k2d::ZenRuntime::instance().compileCount() - compilesBefore;
    ok = ok && compiles == 1;
    ok = ok && k2d::ZenRuntime::instance().cachedClassCount() == 1;

    ok = ok && a->zIndex() == 1 && b->zIndex() == 1 && c->zIndex() == 1;

    std::printf("  shared_class: compiles=%d cached=%d rot=(%.1f, %.1f, %.1f)\n",
                (int)compiles, (int)k2d::ZenRuntime::instance().cachedClassCount(),
                a->rotationDegrees(), b->rotationDegrees(), c->rotationDegrees());
    return ok;
}

static bool testSpawnCostIsCheap()
{
    writeSpinner(45.0f);
    k2d::ZenRuntime::instance().reset();
    const std::size_t compilesBefore = k2d::ZenRuntime::instance().compileCount();

    k2d::Scene scene;
    const int count = 500;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < count; ++i)
    {
        k2d::GameObject *o = scene.createObject("bullet");
        o->addComponent<k2d::ZenScriptComponent>()->loadFile(kSpinnerPath);
    }
    auto t1 = std::chrono::steady_clock::now();
    const double spawnMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    scene.update(0.016f);

    const std::size_t compiles = k2d::ZenRuntime::instance().compileCount() - compilesBefore;
    const bool compiledOnce = compiles == 1;
    std::printf("  spawn_cost: %d objects in %.2fms (%.4fms each), compiles=%d\n",
                count, spawnMs, spawnMs / count, (int)compiles);
    return compiledOnce && spawnMs < 50.0;
}

static bool testReloadInvalidatesAllUsers()
{
    writeSpinner(90.0f);
    k2d::ZenRuntime::instance().reset();
    const std::size_t compilesBefore = k2d::ZenRuntime::instance().compileCount();

    k2d::Scene scene;
    k2d::GameObject *a = scene.createObject("a");
    k2d::GameObject *b = scene.createObject("b");
    k2d::ZenScriptComponent *sa = a->addComponent<k2d::ZenScriptComponent>();
    k2d::ZenScriptComponent *sb = b->addComponent<k2d::ZenScriptComponent>();
    bool ok = sa->loadFile(kSpinnerPath) && sb->loadFile(kSpinnerPath);

    scene.update(1.0f);
    ok = ok && nearEqual(a->rotationDegrees(), 90.0f, 0.01f);

    a->setRotationDegrees(0.0f);
    b->setRotationDegrees(0.0f);

    writeSpinner(10.0f);
    std::filesystem::last_write_time(
        kSpinnerPath, std::filesystem::last_write_time(kSpinnerPath) + std::chrono::seconds(3));

    const std::size_t reloaded = k2d::ReloadChangedZenScripts();
    ok = ok && reloaded == 1;
    const std::size_t compiles = k2d::ZenRuntime::instance().compileCount() - compilesBefore;
    ok = ok && compiles == 2;
    ok = ok && k2d::ZenRuntime::instance().cachedClassCount() == 1;

    scene.update(1.0f);
    ok = ok && nearEqual(a->rotationDegrees(), 10.0f, 0.01f);
    ok = ok && nearEqual(b->rotationDegrees(), 10.0f, 0.01f);

    std::printf("  reload: scripts_rebuilt=%d compiles=%d (1 initial + 1 recompile) rot=(%.1f, %.1f)\n",
                (int)reloaded, (int)compiles, a->rotationDegrees(), b->rotationDegrees());
    return ok;
}

static bool testResetInvalidatesStaleInstances()
{
    writeSpinner(90.0f);
    k2d::ZenRuntime::instance().reset();

    k2d::Scene scene;
    k2d::GameObject *a = scene.createObject("a");
    k2d::ZenScriptComponent *sa = a->addComponent<k2d::ZenScriptComponent>();
    bool ok = sa->loadFile(kSpinnerPath);
    scene.update(1.0f);
    ok = ok && nearEqual(a->rotationDegrees(), 90.0f, 0.01f);

    const unsigned int generationBefore = k2d::ZenRuntime::instance().generation();
    k2d::ZenRuntime::instance().reset();
    ok = ok && k2d::ZenRuntime::instance().generation() != generationBefore;
    ok = ok && k2d::ZenRuntime::instance().cachedClassCount() == 0;

    a->setRotationDegrees(0.0f);
    scene.update(1.0f);
    ok = ok && nearEqual(a->rotationDegrees(), 90.0f, 0.01f);
    ok = ok && k2d::ZenRuntime::instance().cachedClassCount() == 1;

    std::printf("  reset: recovered rot=%.1f cached=%d\n", a->rotationDegrees(),
                (int)k2d::ZenRuntime::instance().cachedClassCount());
    return ok;
}

static bool testDestroyHook()
{
    FILE *f = std::fopen("/tmp/k2d_zen_destroy.py", "w");
    std::fputs("class Dying:\n"
               "    def __init__(self, node):\n"
               "        self.node = node\n"
               "    def on_destroy(self):\n"
               "        set_number(\"destroyed\", get_number(\"destroyed\", 0) + 1)\n",
               f);
    std::fclose(f);

    k2d::ZenRuntime::instance().reset();
    k2d::ZenBlackboard::clear();

    {
        k2d::Scene scene;
        k2d::GameObject *o = scene.createObject("dying");
        o->addComponent<k2d::ZenScriptComponent>()->loadFile("/tmp/k2d_zen_destroy.py");
        scene.update(0.016f);
    }

    const bool ok = nearEqual((float)k2d::ZenBlackboard::getNumber("destroyed"), 1.0f);
    std::printf("  destroy_hook: count=%g\n", k2d::ZenBlackboard::getNumber("destroyed"));
    std::remove("/tmp/k2d_zen_destroy.py");
    return ok;
}

static const char *kOtherPath = "/tmp/k2d_zen_other.py";

static bool testReloadLeavesOtherScriptsAlone()
{
    writeSpinner(90.0f);
    FILE *f = std::fopen(kOtherPath, "w");
    std::fputs("class Counter:\n"
               "    def __init__(self, node):\n"
               "        self.node = node\n"
               "        self.ticks = 0\n"
               "    def on_update(self, dt):\n"
               "        self.ticks = self.ticks + 1\n"
               "        self.node.set_z_index(self.ticks)\n",
               f);
    std::fclose(f);

    k2d::ZenRuntime::instance().reset();

    k2d::Scene scene;
    k2d::GameObject *spinner = scene.createObject("spinner");
    k2d::GameObject *counter = scene.createObject("counter");
    spinner->addComponent<k2d::ZenScriptComponent>()->loadFile(kSpinnerPath);
    counter->addComponent<k2d::ZenScriptComponent>()->loadFile(kOtherPath);

    for (int i = 0; i < 5; ++i)
        scene.update(0.016f);
    bool ok = counter->zIndex() == 5;

    writeSpinner(10.0f);
    std::filesystem::last_write_time(
        kSpinnerPath, std::filesystem::last_write_time(kSpinnerPath) + std::chrono::seconds(3));
    ok = ok && k2d::ReloadChangedZenScripts() == 1;

    scene.update(0.016f);
    ok = ok && counter->zIndex() == 6;

    std::printf("  isolation: counter kept counting to %d across another script's reload\n",
                counter->zIndex());
    std::remove(kOtherPath);
    return ok;
}

static bool testBrokenReloadKeepsTheLastGoodClass()
{
    writeSpinner(90.0f);
    k2d::ZenRuntime::instance().reset();

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("spinner");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadFile(kSpinnerPath);

    scene.update(1.0f);
    ok = ok && nearEqual(object->rotationDegrees(), 90.0f, 0.01f);

    FILE *f = std::fopen(kSpinnerPath, "w");
    std::fputs("class Spinner:\n    def __init__(self, node)\n        broken\n", f);
    std::fclose(f);
    std::filesystem::last_write_time(
        kSpinnerPath, std::filesystem::last_write_time(kSpinnerPath) + std::chrono::seconds(3));

    ok = ok && k2d::ReloadChangedZenScripts() == 0;
    ok = ok && script->loaded();

    object->setRotationDegrees(0.0f);
    scene.update(1.0f);
    ok = ok && nearEqual(object->rotationDegrees(), 90.0f, 0.01f);

    std::printf("  broken_reload: still running the last good class, rot=%.1f\n",
                object->rotationDegrees());
    return ok;
}

int main()
{
    k2d::SetZenScriptsEnabled(true);

    const bool shared = testSharedClassPerInstanceState();
    const bool spawn = testSpawnCostIsCheap();
    const bool reload = testReloadInvalidatesAllUsers();
    const bool isolation = testReloadLeavesOtherScriptsAlone();
    const bool broken = testBrokenReloadKeepsTheLastGoodClass();
    const bool reset = testResetInvalidatesStaleInstances();
    const bool destroy = testDestroyHook();

    std::remove(kSpinnerPath);
    std::printf("zen class: shared=%s spawn=%s reload=%s isolation=%s broken_reload=%s reset=%s "
                "destroy=%s\n",
                shared ? "pass" : "fail", spawn ? "pass" : "fail", reload ? "pass" : "fail",
                isolation ? "pass" : "fail", broken ? "pass" : "fail", reset ? "pass" : "fail",
                destroy ? "pass" : "fail");
    return shared && spawn && reload && isolation && broken && reset && destroy ? 0 : 1;
}
