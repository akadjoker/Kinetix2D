#include <k2d/Animation2D.h>
#include <k2d/GameObject.h>
#include <k2d/Input.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>
#include <k2d/SpriteComponent.h>
#include <k2d/ZenScriptComponent.h>

#include <cmath>
#include <cstdio>
#include <cstring>

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
        "def ready(node):\n"
        "    node.set_z_index(7)\n"
        "def update(node, dt):\n"
        "    node.translate(10 * dt, 0)\n"
        "    node.set_rotation(node.get_rotation() + 90 * dt)\n",
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
        "child_name = \"\"\n"
        "child_total = 0\n"
        "target_x = -1.0\n"
        "parent_of_child = \"\"\n"
        "spawned = \"\"\n"
        "def ready(node):\n"
        "    global child_name, child_total, target_x, parent_of_child, spawned\n"
        "    child_total = node.child_count()\n"
        "    first = node.get_child(0)\n"
        "    child_name = first.get_name()\n"
        "    parent_of_child = first.get_parent().get_name()\n"
        "    target = node.find(\"target\")\n"
        "    target_x = target.get_x()\n"
        "    made = node.create_child(\"spawned\")\n"
        "    made.set_position(1, 2)\n"
        "    spawned = made.get_name()\n"
        "def update(node, dt):\n"
        "    pass\n",
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
        "def ready(node):\n"
        "    s = node.get_sprite()\n"
        "    s.set_color(255, 0, 0, 255)\n"
        "    s.set_flip(True, False)\n"
        "    s.set_size(32, 16)\n"
        "    a = node.get_animation()\n"
        "    a.play(\"idle\")\n"
        "    p = node.get_particle()\n"
        "    p.burst(5)\n"
        "def update(node, dt):\n"
        "    pass\n",
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
        "def update(node, dt):\n"
        "    if key_down(\"space\") and mouse_down(0):\n"
        "        node.set_position(mouse_x(), mouse_y())\n"
        "    if key_down(\"escape\"):\n"
        "        node.set_position(-1, -1)\n",
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
        "def update(node, dt):\n"
        "    node.queue_destroy()\n",
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
    std::fputs("def update(node, dt):\n    node.set_position(33, 44)\n", file);
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
    return ok;
}

int main()
{
    const bool basics = testBasics();
    const bool hierarchy = testHierarchy();
    const bool components = testComponents();
    const bool inputOk = testInput();
    const bool destroy = testDestroy();
    const bool serialization = testSerialization();
    const bool examples = testExampleScripts();

    std::printf("zen: basics=%s hierarchy=%s components=%s input=%s destroy=%s serialization=%s examples=%s\n",
                basics ? "pass" : "fail", hierarchy ? "pass" : "fail",
                components ? "pass" : "fail", inputOk ? "pass" : "fail",
                destroy ? "pass" : "fail", serialization ? "pass" : "fail",
                examples ? "pass" : "fail");
    return basics && hierarchy && components && inputOk && destroy && serialization && examples ? 0 : 1;
}
