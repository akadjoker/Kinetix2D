#include <k2d/Animation2D.h>
#include <k2d/GameObject.h>
#include <k2d/Input.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>
#include <k2d/SpriteComponent.h>
#include <k2d/ZenScriptComponent.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

static bool nearEqual(float a, float b)
{
    return std::fabs(a - b) < 0.001f;
}

static bool testBasics()
{
    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("player");
    object->setPosition(Math::Vec2(5.0f, 6.0f));

    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource(
        "class Basics:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "    def on_start(self):\n"
        "        self.node.set_z_index(7)\n"
        "    def on_update(self, dt):\n"
        "        self.node.translate(10 * dt, 0)\n"
        "        self.node.set_rotation(self.node.get_rotation() + 90 * dt)\n",
        "basics");
    ok = ok && script->loaded();

    scene.update(0.5f);
    ok = ok && object->zIndex() == 7;
    ok = ok && nearEqual(object->position().x, 10.0f) && nearEqual(object->rotationDegrees(), 45.0f);
    scene.update(0.5f);
    ok = ok && nearEqual(object->position().x, 15.0f) && nearEqual(object->rotationDegrees(), 90.0f);

    k2d::ZenScriptComponent probe;
    ok = ok && !probe.loadSource("def broken(:\n", "bad") && !probe.loaded();
    return ok;
}

static bool testHierarchy()
{
    k2d::Scene scene;
    k2d::GameObject *parent = scene.createObject("parent");
    k2d::GameObject *child = scene.createObject("child", parent);
    scene.createObject("target")->setPosition(Math::Vec2(50.0f, 60.0f));
    (void)child;

    k2d::ZenScriptComponent *script = parent->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource(
        "class Hier:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "    def on_start(self):\n"
        "        first = self.node.get_child(0)\n"
        "        set_string(\"child_name\", first.get_name())\n"
        "        set_string(\"parent_of_child\", first.get_parent().get_name())\n"
        "        set_number(\"child_total\", self.node.child_count())\n"
        "        set_number(\"target_x\", self.node.find(\"target\").get_x())\n"
        "        made = self.node.create_child(\"spawned\")\n"
        "        made.set_position(1, 2)\n",
        "hierarchy");
    scene.update(0.016f);

    ok = ok && scene.find("spawned") != nullptr;
    ok = ok && scene.find("spawned")->parent() == parent;
    ok = ok && nearEqual(scene.find("spawned")->position().x, 1.0f);
    return ok;
}

static bool testComponents()
{
    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("holder");
    k2d::SpriteComponent *sprite = object->addComponent<k2d::SpriteComponent>();
    k2d::Animation2D *animation = object->addComponent<k2d::Animation2D>();
    animation->addClip("run", nullptr, 8, 8, 4, 10.0f, k2d::AnimationMode::Loop);
    animation->addClip("idle", nullptr, 8, 8, 2, 5.0f, k2d::AnimationMode::Loop);
    k2d::ParticleComponent *particle = object->addComponent<k2d::ParticleComponent>();
    particle->system().SetCapacity(64);

    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource(
        "class Comp:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "    def on_start(self):\n"
        "        s = self.node.get_sprite()\n"
        "        s.set_color(255, 0, 0, 255)\n"
        "        s.set_flip(True, False)\n"
        "        s.set_size(32, 16)\n"
        "        self.node.get_animation().play(\"idle\")\n"
        "        self.node.get_particle().burst(5)\n",
        "components");
    scene.update(0.016f);

    ok = ok && sprite->flipX() && !sprite->flipY();
    ok = ok && nearEqual(sprite->size().x, 32.0f) && nearEqual(sprite->size().y, 16.0f);
    ok = ok && std::strcmp(animation->currentClip(), "idle") == 0;
    ok = ok && particle->system().ActiveCount() == 5;
    return ok;
}

static bool testInput()
{
    k2d::Input input;
    k2d::SetZenScriptInput(&input);
    input.NewFrame();
    input.OnKey(44, true, false);
    input.OnMouseMove(120.0f, 240.0f);
    input.OnMouseButton(0, true);

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("listener");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource(
        "class In:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "    def on_update(self, dt):\n"
        "        if key_down(\"space\") and mouse_down(0):\n"
        "            self.node.set_position(mouse_x(), mouse_y())\n"
        "        if key_down(\"escape\"):\n"
        "            self.node.set_position(-1, -1)\n",
        "input");
    scene.update(0.016f);

    ok = ok && nearEqual(object->position().x, 120.0f) && nearEqual(object->position().y, 240.0f);
    k2d::SetZenScriptInput(nullptr);
    return ok;
}

static bool testDestroy()
{
    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("mortal");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource(
        "class Mortal:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "    def on_update(self, dt):\n"
        "        self.node.queue_destroy()\n",
        "destroy");
    ok = ok && scene.find("mortal") != nullptr;
    scene.update(0.016f);
    scene.update(0.016f);
    ok = ok && scene.find("mortal") == nullptr;
    return ok;
}

static bool testSerialization()
{
    k2d::RegisterZenScriptSerializer();

    const char *scriptPath = "/tmp/k2d_zen_test_script.py";
    FILE *file = std::fopen(scriptPath, "w");
    if (!file)
        return false;
    std::fputs("class S:\n    def __init__(self, node):\n        self.node = node\n    def on_update(self, dt):\n        self.node.set_position(33, 44)\n", file);
    std::fclose(file);

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("scripted");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadFile(scriptPath);

    const ct::Json json = k2d::Serializer::WriteObject(*object);
    const ct::Json &components = json["components"];
    bool foundEntry = false;
    for (size_t i = 0; i < components.size(); ++i)
        if (std::strcmp(components[i]["type"].as_cstr(""), "ZenScript") == 0 &&
            std::strcmp(components[i]["data"]["path"].as_cstr(""), scriptPath) == 0)
            foundEntry = true;
    ok = ok && foundEntry;

    k2d::Scene loadedScene;
    k2d::GameObject *loaded = k2d::Serializer::ReadObject(loadedScene, json);
    ok = ok && loaded != nullptr;
    k2d::ZenScriptComponent *loadedScript = loaded ? loaded->getComponent<k2d::ZenScriptComponent>() : nullptr;
    ok = ok && loadedScript && loadedScript->loaded() &&
         loadedScript->scriptPath() == ct::String(scriptPath);

    loadedScene.update(0.016f);
    ok = ok && loaded && nearEqual(loaded->position().x, 33.0f) && nearEqual(loaded->position().y, 44.0f);

    std::remove(scriptPath);
    return ok;
}

static ct::String gCapturedOutput;
static bool gCapturedError = false;

static bool testSpawnAndMath()
{
    const char *prefabPath = "/tmp/k2d_zen_test_prefab.k2dprefab";
    FILE *file = std::fopen(prefabPath, "w");
    if (!file)
        return false;
    std::fputs("{\"name\":\"bullet\",\"components\":[],\"children\":[]}", file);
    std::fclose(file);

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("shooter");
    object->setPosition(Math::Vec2(0.0f, 0.0f));
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource(
        "class Shooter:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "    def on_start(self):\n"
        "        b = self.node.spawn(\"/tmp/k2d_zen_test_prefab.k2dprefab\", 30, 40)\n"
        "        print(\"spawned\", b.get_name(), self.node.distance_to(30, 40))\n"
        "        self.node.look_at(0, 100)\n"
        "    def on_update(self, dt):\n"
        "        self.node.move_toward(60, 0, 25)\n",
        "spawner");

    k2d::SetZenScriptOutput([](const char *text, bool isError, void *)
    {
        gCapturedOutput += text;
        if (isError)
            gCapturedError = true;
    }, nullptr);
    scene.update(0.016f);
    k2d::SetZenScriptOutput(nullptr, nullptr);

    k2d::GameObject *bullet = scene.find("bullet");
    ok = ok && bullet && nearEqual(bullet->position().x, 30.0f) && nearEqual(bullet->position().y, 40.0f);
    ok = ok && nearEqual(object->rotationDegrees(), 90.0f);
    ok = ok && nearEqual(object->position().x, 25.0f);
    scene.update(0.016f);
    scene.update(0.016f);
    ok = ok && nearEqual(object->position().x, 60.0f);
    ok = ok && !gCapturedError && gCapturedOutput.size() > 0;
    ok = ok && std::strstr(gCapturedOutput.c_str(), "spawned") != nullptr;
    ok = ok && std::strstr(gCapturedOutput.c_str(), "bullet") != nullptr;
    ok = ok && std::strstr(gCapturedOutput.c_str(), "50") != nullptr;

    std::remove(prefabPath);
    return ok;
}

static bool testScriptsGate()
{
    k2d::SetZenScriptsEnabled(false);
    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("gated");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class G:\n    def __init__(self, node):\n        self.node = node\n"
                                 "    def on_update(self, dt):\n        self.node.set_position(9, 9)\n", "gate");

    scene.update(0.016f);
    ok = ok && nearEqual(object->position().x, 0.0f);

    k2d::SetZenScriptsEnabled(true);
    scene.update(0.016f);
    ok = ok && nearEqual(object->position().x, 9.0f);
    return ok;
}

static bool testBlackboardAndEvents()
{
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    k2d::GameObject *sender = scene.createObject("sender");
    k2d::ZenScriptComponent *senderScript = sender->addComponent<k2d::ZenScriptComponent>();
    bool ok = senderScript->loadSource(
        "class Sender:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "        self.fired = False\n"
        "    def on_update(self, dt):\n"
        "        if not self.fired:\n"
        "            self.fired = True\n"
        "            set_number(\"hp\", 75)\n"
        "            set_string(\"stage\", \"boss\")\n"
        "            set_flag(\"alive\", True)\n"
        "            emit(\"hit\", 12)\n",
        "sender");

    k2d::GameObject *receiver = scene.createObject("receiver");
    k2d::ZenScriptComponent *receiverScript = receiver->addComponent<k2d::ZenScriptComponent>();
    ok = ok && receiverScript->loadSource(
                   "class Receiver:\n"
                   "    def __init__(self, node):\n"
                   "        self.node = node\n"
                   "    def on_event(self, name, value):\n"
                   "        if name == \"hit\":\n"
                   "            set_number(\"damage\", value * 2)\n"
                   "            self.node.set_position(get_number(\"hp\", 0), 1)\n",
                   "receiver");

    scene.update(0.016f);
    ok = ok && k2d::ZenBlackboard::pendingEventCount() == 1;
    k2d::DispatchZenScriptEvents(scene.root());
    ok = ok && k2d::ZenBlackboard::pendingEventCount() == 0;

    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("damage"), 24.0f);
    ok = ok && nearEqual(receiver->position().x, 75.0f);

    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("hp"), 75.0f);
    ok = ok && k2d::ZenBlackboard::getString("stage") == ct::String("boss");
    ok = ok && k2d::ZenBlackboard::getBool("alive");
    ok = ok && k2d::ZenBlackboard::has("hp") && !k2d::ZenBlackboard::has("nope");

    k2d::ZenBlackboard::setNumber("from_host", 3.5);
    k2d::GameObject *reader = scene.createObject("reader");
    k2d::ZenScriptComponent *readerScript = reader->addComponent<k2d::ZenScriptComponent>();
    ok = ok && readerScript->loadSource(
                   "class Reader:\n"
                   "    def __init__(self, node):\n"
                   "        self.node = node\n"
                   "    def on_update(self, dt):\n"
                   "        self.node.set_position(get_number(\"from_host\", -1), get_number(\"missing\", 8))\n",
                   "reader");
    scene.update(0.016f);
    ok = ok && nearEqual(reader->position().x, 3.5f) && nearEqual(reader->position().y, 8.0f);

    k2d::BroadcastZenScriptEvent(scene.root(), "hit", 50.0);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("damage"), 100.0f);

    static int hostSeen = 0;
    static double hostValue = 0.0;
    k2d::ZenBlackboard::setHostHandler([](const char *name, double value, void *)
    {
        if (std::strcmp(name, "hit") == 0)
        {
            ++hostSeen;
            hostValue = value;
        }
    }, nullptr);
    k2d::ZenBlackboard::emit("hit", 7.0);
    ok = ok && hostSeen == 1 && nearEqual((float)hostValue, 7.0f);
    k2d::ZenBlackboard::setHostHandler(nullptr, nullptr);

    ok = ok && receiverScript->hasFunction("on_event") && !receiverScript->hasFunction("nope_method");
    ok = ok && receiverScript->callEvent("hit", 4.0);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("damage"), 8.0f);

    k2d::ZenBlackboard::clear();
    ok = ok && !k2d::ZenBlackboard::has("hp") && k2d::ZenBlackboard::pendingEventCount() == 0;
    return ok;
}

static bool testHotReload()
{
    const char *path = "/tmp/k2d_zen_hotreload.py";
    FILE *file = std::fopen(path, "w");
    if (!file)
        return false;
    std::fputs("class H:\n    def __init__(self, node):\n        self.node = node\n    def on_update(self, dt):\n        self.node.set_position(1, 1)\n", file);
    std::fclose(file);

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("reloader");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadFile(path);
    scene.update(0.016f);
    ok = ok && nearEqual(object->position().x, 1.0f);

    ok = ok && !script->reloadIfChanged();
    ok = ok && k2d::ReloadChangedZenScripts(scene.root()) == 0;

    std::filesystem::last_write_time(
        path, std::filesystem::last_write_time(path) + std::chrono::seconds(2));
    file = std::fopen(path, "w");
    std::fputs("class H:\n    def __init__(self, node):\n        self.node = node\n    def on_update(self, dt):\n        self.node.set_position(2, 2)\n", file);
    std::fclose(file);
    std::filesystem::last_write_time(
        path, std::filesystem::last_write_time(path) + std::chrono::seconds(4));

    ok = ok && k2d::ReloadChangedZenScripts(scene.root()) == 1;
    scene.update(0.016f);
    ok = ok && nearEqual(object->position().x, 2.0f);
    ok = ok && k2d::ReloadChangedZenScripts(scene.root()) == 0;

    std::remove(path);
    return ok;
}

static bool testNetAndHttpModulesImport()
{
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("importer");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("import net\n"
                                 "import http\n"
                                 "\n"
                                 "class Importer:\n"
                                 "    def __init__(self, node):\n"
                                 "        self.node = node\n"
                                 "\n"
                                 "    def on_start(self):\n"
                                 "        set_flag(\"net\", net != None)\n"
                                 "        set_flag(\"http\", http != None)\n",
                                 "modules");

    scene.update(0.016f);

    ok = ok && k2d::ZenBlackboard::getBool("net", false);
    ok = ok && k2d::ZenBlackboard::getBool("http", false);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testExampleScripts()
{
    k2d::Scene scene;
    k2d::GameObject *player = scene.createObject("player");
    player->setPosition(Math::Vec2(10.0f, 20.0f));
    k2d::ZenScriptComponent *playerScript = player->addComponent<k2d::ZenScriptComponent>();
    bool ok = playerScript->loadFile("../assets/scripts/player.py");

    k2d::GameObject *satellite = scene.createObject("satellite");
    k2d::ZenScriptComponent *orbitScript = satellite->addComponent<k2d::ZenScriptComponent>();
    ok = ok && orbitScript->loadFile("../assets/scripts/orbit.py");

    scene.update(0.016f);
    ok = ok && std::fabs(satellite->position().x - 10.0f) < 200.0f &&
         std::fabs(satellite->position().y - 20.0f) < 200.0f &&
         satellite->rotationDegrees() > 0.0f;

    k2d::GameObject *spawner = scene.createObject("spawner");
    k2d::ZenScriptComponent *spawnerScript = spawner->addComponent<k2d::ZenScriptComponent>();
    ok = ok && spawnerScript->loadFile("../assets/scripts/spawner.py");

    k2d::GameObject *hud = scene.createObject("hud");
    k2d::ZenScriptComponent *hudScript = hud->addComponent<k2d::ZenScriptComponent>();
    ok = ok && hudScript->loadFile("../assets/scripts/hud.py");

    scene.update(0.016f);
    k2d::DispatchZenScriptEvents(scene.root());
    ok = ok && k2d::ZenBlackboard::has("spawned") && k2d::ZenBlackboard::has("score");

    k2d::ZenBlackboard::emit("enemy_killed", 30.0);
    k2d::DispatchZenScriptEvents(scene.root());
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("score"), 30.0f);

    k2d::ZenBlackboard::emit("player_died");
    k2d::DispatchZenScriptEvents(scene.root());
    ok = ok && k2d::ZenBlackboard::getString("state") == ct::String("gameover");
    ok = ok && !k2d::ZenBlackboard::getBool("can_shoot", true);

    k2d::ZenBlackboard::clear();
    return ok;
}

int main()
{
    k2d::SetZenScriptsEnabled(true);

    const bool basics = testBasics();
    const bool hierarchy = testHierarchy();
    const bool components = testComponents();
    const bool inputOk = testInput();
    const bool destroy = testDestroy();
    const bool serialization = testSerialization();
    const bool spawnMath = testSpawnAndMath();
    const bool gate = testScriptsGate();
    const bool channel = testBlackboardAndEvents();
    const bool hotReload = testHotReload();
    const bool modules = testNetAndHttpModulesImport();
    const bool examples = testExampleScripts();

    std::printf("zen: basics=%s hierarchy=%s components=%s input=%s destroy=%s serialization=%s "
                "spawn_math=%s gate=%s channel=%s hot_reload=%s modules=%s examples=%s\n",
                basics ? "pass" : "fail", hierarchy ? "pass" : "fail",
                components ? "pass" : "fail", inputOk ? "pass" : "fail",
                destroy ? "pass" : "fail", serialization ? "pass" : "fail",
                spawnMath ? "pass" : "fail", gate ? "pass" : "fail",
                channel ? "pass" : "fail", hotReload ? "pass" : "fail",
                modules ? "pass" : "fail", examples ? "pass" : "fail");
    return basics && hierarchy && components && inputOk && destroy && serialization &&
                   spawnMath && gate && channel && hotReload && modules && examples
               ? 0
               : 1;
}
