#include <k2d/Animation2D.h>
#include <k2d/AudioPlayer.h>
#include <k2d/AudioEngine.h>
#include <k2d/Assets.h>
#include <k2d/Bone2D.h>
#include <k2d/Camera2D.h>
#include <k2d/CameraComponent.h>
#include <k2d/CapsuleShape.h>
#include <k2d/CircleShape.h>
#include <k2d/DirectionalLight2D.h>
#include <k2d/FileSystem.h>
#include <k2d/GameObject.h>
#include <k2d/Input.h>
#include <k2d/InputActionMap.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Polygon2D.h>
#include <k2d/RectShape.h>
#include <k2d/RenderQueue.h>
#include <k2d/Scene.h>
#include <k2d/SceneManager.h>
#include <k2d/Skeleton2D.h>
#include <k2d/ScreenFade.h>
#include <k2d/Serializer.h>
#include <k2d/SpriteComponent.h>
#include <k2d/SpriteBatch.h>
#include <k2d/TileMapComponent.h>
#include <k2d/ZenRuntime.h>
#include <k2d/ZenScriptComponent.h>
#include <k2d/UiControls.h>
#include <k2d/UserData.h>
#include <k2d/VirtualPad.h>
#include <k2d/BoxCollider2D.h>
#include <k2d/ChainCollider2D.h>
#include <k2d/CharacterBody2D.h>
#include <k2d/CircleCollider2D.h>
#include <k2d/EdgeCollider2D.h>
#include <k2d/Light2D.h>
#include <k2d/LightOccluder2D.h>
#include <k2d/Line2D.h>
#include <k2d/MotionStreak2D.h>
#include <k2d/MotionTween2D.h>
#include <k2d/NavigationAgent2D.h>
#include <k2d/NavigationRegion2D.h>
#include <k2d/NinePatchComponent.h>
#include <k2d/PolygonCollider2D.h>
#include <k2d/RigidBody2D.h>

#include "k2d_test_paths.h"

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
    k2d::GameObject* object = scene.createObject("player");
    object->setPosition(Math::Vec2(5.0f, 6.0f));

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class Basics:\n"
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

static bool testScriptComponentBaseProvidesNode()
{
    k2d::ZenRuntime::instance().reset();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("base_script");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class BaseScript(ScriptComponent):\n"
                                 "    speed = 25\n"
                                 "    def __init__(self):\n"
                                 "        self.started = True\n"
                                 "    def on_update(self, dt):\n"
                                 "        self.node.translate(self.speed * dt, 0)\n",
                                 "script_component_base");
    scene.update(1.0f);
    return ok && script->loaded() && nearEqual(object->position().x, 25.0f) &&
           script->declaredProperty("node") == nullptr && script->declaredProperty("speed") != nullptr;
}

static bool testDrawApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("draw_script");
    object->setZIndex(4);
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class DrawScript(ScriptComponent):\n"
                                 "    def on_draw(self):\n"
                                 "        set_draw_color(1, 0, 0, 0.5)\n"
                                 "        draw_rect(10, 20, 30, 40)\n"
                                 "        draw_circle(80, 80, 20, False, 12, 2)\n"
                                 "        draw_line(0, 0, 20, 20, 3)\n"
                                 "        draw_text(4, 5, \"draw\", 10)\n"
                                 "        set_number(\"draw_width\", draw_text_width(\"abc\", 10))\n"
                                 "    def on_draw_ui(self):\n"
                                 "        draw_text(4, 20, \"hud\", 10)\n",
                                 "draw_api");

    k2d::RenderQueue& queue = scene.buildRenderQueue();
    ok = ok && queue.ItemCount() == 2 && queue.CommandCount() == 5 &&
         nearEqual((float)k2d::ZenBlackboard::getNumber("draw_width"), 30.0f);
    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testObjectCount()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* main = scene.createObject("main");
    scene.createObject("one");
    scene.createObject("two");
    k2d::ZenScriptComponent* script = main->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource("class Counter(ScriptComponent):\n"
                                           "    def on_update(self, dt):\n"
                                           "        set_number(\"objects\", object_count())\n",
                                           "object_count");
    scene.update(0.016f);
    const bool ok = loaded && nearEqual((float)k2d::ZenBlackboard::getNumber("objects"), 3.0f);
    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testBunnymarkSpawn()
{
    k2d::ZenBlackboard::clear();
    k2d::RegisterZenScriptSerializer();
    k2d::Input input;
    k2d::SetZenScriptInput(&input);
    input.NewFrame();
    input.OnMouseMove(120.0f, 160.0f);
    input.OnMouseButton(0, true);

    k2d::Scene scene;
    k2d::GameObject* main = scene.createObject("bunnymark_main");
    k2d::ZenScriptComponent* script = main->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadFile("scripts/bunnymark_main.py");
    scene.update(0.016f);
    const std::size_t spawned = static_cast<std::size_t>(k2d::ZenBlackboard::getNumber("bunnymark_bunnies"));
    ok = ok && spawned > 0 && scene.objectCount() == spawned + 1;
    input.NewFrame();
    scene.update(0.016f);
    ok = ok && scene.objectCount() == spawned + 1;
    k2d::RenderQueue& queue = scene.buildRenderQueue();
    ok = ok && queue.ItemCount() == 1 && queue.CommandCount() == 3;

    k2d::SetZenScriptInput(nullptr);
    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testHierarchy()
{
    k2d::Scene scene;
    k2d::GameObject* parent = scene.createObject("parent");
    k2d::GameObject* child = scene.createObject("child", parent);
    scene.createObject("target")->setPosition(Math::Vec2(50.0f, 60.0f));
    (void)child;

    k2d::ZenScriptComponent* script = parent->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class Hier:\n"
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
    k2d::GameObject* object = scene.createObject("holder");
    k2d::SpriteComponent* sprite = object->addComponent<k2d::SpriteComponent>();
    k2d::Animation2D* animation = object->addComponent<k2d::Animation2D>();
    animation->addClip("run", nullptr, 8, 8, 4, 10.0f, k2d::AnimationMode::Loop);
    animation->addClip("idle", nullptr, 8, 8, 2, 5.0f, k2d::AnimationMode::Loop);
    k2d::ParticleComponent* particle = object->addComponent<k2d::ParticleComponent>();
    particle->system().SetCapacity(64);
    k2d::CameraComponent* camera = object->addComponent<k2d::CameraComponent>();
    camera->setViewport(640.0f, 360.0f);

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class Comp:\n"
                                 "    def __init__(self, node):\n"
                                 "        self.node = node\n"
                                 "    def on_start(self):\n"
                                 "        s = self.node.get_sprite()\n"
                                 "        s.set_color(255, 0, 0, 255)\n"
                                 "        s.set_flip(True, False)\n"
                                 "        s.set_size(32, 16)\n"
                                 "        s.set_water_enabled(True)\n"
                                 "        s.set_water_flow(0.1, 0.2, -0.05, 0.07)\n"
                                 "        s.set_water_strength(0.03)\n"
                                 "        c = self.node.get_camera()\n"
                                 "        c.set_trauma_profile(10, 6, 12, 2)\n"
                                 "        c.add_trauma(1)\n"
                                 "        c.start_zoom_punch(0.2, 0.3)\n"
                                 "        self.node.get_animation().play(\"idle\")\n"
                                 "        self.node.get_particle().burst(5)\n",
                                 "components");
    scene.update(0.016f);

    ok = ok && sprite->flipX() && !sprite->flipY();
    ok = ok && nearEqual(sprite->size().x, 32.0f) && nearEqual(sprite->size().y, 16.0f);
    ok = ok && sprite->waterEnabled() && nearEqual(sprite->water().strength, 0.03f) &&
         nearEqual(sprite->water().flowA.x, 0.1f) && nearEqual(sprite->water().flowB.y, 0.07f);
    ok = ok && camera->camera().traumaValue() > 0.0f && camera->camera().isZoomPunching();
    ok = ok && std::strcmp(animation->currentClip(), "idle") == 0;
    ok = ok && particle->system().ActiveCount() == 5;
    return ok;
}

static bool testGenericAngleBracketCalls()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("generic_holder");
    object->addComponent<k2d::SpriteComponent>();
    object->addComponent<k2d::Animation2D>();
    object->addComponent<k2d::CameraComponent>();
    object->addComponent<k2d::ParticleComponent>();
    object->addComponent<k2d::UiButton>();
    object->addComponent<k2d::UiCheckBox>();
    object->addComponent<k2d::UiSlider>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "def generic_type<T>(value):\n"
        "    return T\n"
        "\n"
        "class GenericCalls(ScriptComponent):\n"
        "    def get_type<T>(self):\n"
        "        return T\n"
        "    def on_start(self):\n"
        "        set_flag(\"sprite\", self.node.get_component<Sprite>() != None)\n"
        "        set_flag(\"animation\", self.node.get_component<Animation>() != None)\n"
        "        set_flag(\"camera\", self.node.get_component<Camera>() != None)\n"
        "        set_flag(\"particle\", self.node.get_component<Particle>() != None)\n"
        "        set_flag(\"button\", self.node.get_component<Button>() != None)\n"
        "        set_flag(\"checkbox\", self.node.get_component<CheckBox>() != None)\n"
        "        set_flag(\"slider\", self.node.get_component<Slider>() != None)\n"
        "        set_flag(\"missing_body\", self.node.get_component<RigidBody>() == None)\n"
        "        set_flag(\"free_generic\", generic_type<Sprite>(0) == Sprite)\n"
        "        set_flag(\"method_generic\", self.get_type<Animation>() == Animation)\n"
        "        set_flag(\"less_than\", 1 < 2)\n"
        "        set_flag(\"greater_than\", 3 > 2)\n",
        "generic_angle_brackets");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    const char* flags[] = {"sprite", "animation", "camera", "particle", "button", "checkbox",
                           "slider", "missing_body", "free_generic", "method_generic", "less_than",
                           "greater_than"};
    for (const char* flag : flags)
        ok = ok && k2d::ZenBlackboard::getBool(flag, false);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testActiveCamera()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* aObject = scene.createObject("cam_a");
    k2d::CameraComponent* aCam = aObject->addComponent<k2d::CameraComponent>();
    k2d::GameObject* bObject = scene.createObject("cam_b");
    k2d::CameraComponent* bCam = bObject->addComponent<k2d::CameraComponent>();
    bCam->setRenderPriority(5);

    k2d::GameObject* object = scene.createObject("probe");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Probe(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        cam = get_active_camera()\n"
        "        set_flag(\"got\", cam != None)\n"
        "        cam.add_trauma(0.8)\n"
        "        self.node.get_root().find(\"cam_b\").get_camera().set_active(False)\n"
        "        fallback = get_active_camera()\n"
        "        set_flag(\"got_fallback\", fallback != None)\n"
        "        fallback.add_trauma(0.5)\n",
        "active_camera");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("got", false);
    ok = ok && k2d::ZenBlackboard::getBool("got_fallback", false);
    ok = ok && bCam->camera().trauma.value > 0.0f;
    ok = ok && aCam->camera().trauma.value > 0.0f;
    ok = ok && !bCam->active();
    ok = ok && scene.activeCamera() == aCam;

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testNodeApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* parent = scene.createObject("parent");
    parent->setPosition(Math::Vec2(10.0f, 20.0f));
    k2d::GameObject* object = scene.createObject("child", parent);
    object->setPosition(Math::Vec2(5.0f, 7.0f));
    object->setTag("enemy");
    k2d::GameObject* other = scene.createObject("other");

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Api(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        n = self.node\n"
        "        set_flag(\"id\", n.get_id() != 0)\n"
        "        set_string(\"tag\", n.get_tag())\n"
        "        n.set_tag(\"boss\")\n"
        "        n.set_name(\"renamed\")\n"
        "        set_string(\"name\", n.get_name())\n"
        "        gx, gy = n.get_global_position()\n"
        "        set_number(\"gx\", gx)\n"
        "        set_number(\"gy\", gy)\n"
        "        rx, ry = n.get_right()\n"
        "        set_number(\"rx\", rx)\n"
        "        set_flag(\"in_hierarchy\", n.is_active_in_hierarchy())\n"
        "        set_flag(\"no_sprite\", not n.has_component<Sprite>())\n"
        "        s = n.add_component<Sprite>()\n"
        "        set_flag(\"added\", s != None)\n"
        "        set_flag(\"now_has\", n.has_component<Sprite>())\n"
        "        set_flag(\"removed\", n.remove_component<Sprite>())\n"
        "        set_flag(\"gone\", not n.has_component<Sprite>())\n"
        "        set_flag(\"abstract_add\", n.add_component<Collider>() == None)\n"
        "        root = n.get_root()\n"
        "        set_flag(\"root\", root != None)\n"
        "        set_flag(\"reparented\", n.reparent(root.find(\"other\")))\n",
        "node_api");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("id", false);
    ok = ok && k2d::ZenBlackboard::getString("tag") == ct::String("enemy");
    ok = ok && object->tag() == ct::String("boss");
    ok = ok && k2d::ZenBlackboard::getString("name") == ct::String("renamed");
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("gx"), 15.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("gy"), 27.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("rx"), 1.0f);
    ok = ok && k2d::ZenBlackboard::getBool("in_hierarchy", false);
    ok = ok && k2d::ZenBlackboard::getBool("no_sprite", false);
    ok = ok && k2d::ZenBlackboard::getBool("added", false);
    ok = ok && k2d::ZenBlackboard::getBool("now_has", false);
    ok = ok && k2d::ZenBlackboard::getBool("removed", false);
    ok = ok && k2d::ZenBlackboard::getBool("gone", false);
    ok = ok && k2d::ZenBlackboard::getBool("abstract_add", false);
    ok = ok && k2d::ZenBlackboard::getBool("root", false);
    ok = ok && k2d::ZenBlackboard::getBool("reparented", false);
    ok = ok && object->parent() == other;

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testSkeleton()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* root = scene.createObject("rig");
    k2d::Skeleton2D* skeleton = root->addComponent<k2d::Skeleton2D>();
    k2d::GameObject* upper = scene.createObject("upper_arm", root);
    k2d::Bone2D* upperBone = upper->addComponent<k2d::Bone2D>();
    upperBone->setLength(40.0f);
    upper->setPosition(Math::Vec2(5.0f, 7.0f));
    upperBone->saveRestPose();

    k2d::BoneAnimationClip clip;
    clip.name = "wave";
    clip.duration = 1.0f;
    skeleton->addClip(clip);

    k2d::ZenScriptComponent* script = root->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Rig(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        s = self.node.get_component<Skeleton>()\n"
        "        set_flag(\"has_skeleton\", s != None)\n"
        "        set_number(\"clips\", s.clip_count())\n"
        "        set_flag(\"played\", s.play(\"wave\", True, 2.0))\n"
        "        set_flag(\"playing\", s.is_playing())\n"
        "        set_string(\"current\", s.current())\n"
        "        set_number(\"speed\", s.get_speed())\n"
        "        s.pause()\n"
        "        set_flag(\"paused\", not s.is_playing())\n"
        "        b = s.find_bone(\"upper_arm\")\n"
        "        set_flag(\"has_bone\", b != None)\n"
        "        set_number(\"bone_length\", b.get_length())\n"
        "        rx, ry = b.get_rest_position()\n"
        "        set_number(\"rest_x\", rx)\n"
        "        set_number(\"rest_y\", ry)\n"
        "        b.set_length(60.0)\n"
        "        set_number(\"new_length\", b.get_length())\n"
        "        set_flag(\"missing_bone\", s.find_bone(\"nope\") == None)\n",
        "skeleton");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_skeleton", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("clips"), 1.0f);
    ok = ok && k2d::ZenBlackboard::getBool("played", false);
    ok = ok && k2d::ZenBlackboard::getBool("playing", false);
    ok = ok && k2d::ZenBlackboard::getString("current") == ct::String("wave");
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("speed"), 2.0f);
    ok = ok && k2d::ZenBlackboard::getBool("paused", false);
    ok = ok && k2d::ZenBlackboard::getBool("has_bone", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("bone_length"), 40.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("rest_x"), 5.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("rest_y"), 7.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("new_length"), 60.0f);
    ok = ok && nearEqual(upperBone->length(), 60.0f);
    ok = ok && k2d::ZenBlackboard::getBool("missing_bone", false);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testAudioPlayerApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("jukebox");
    k2d::AudioPlayer* player = object->addComponent<k2d::AudioPlayer>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Jukebox(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        p = self.node.get_component<AudioPlayer>()\n"
        "        set_flag(\"has_player\", p != None)\n"
        "        set_flag(\"idle\", not p.is_playing())\n"
        "        p.set_volume(0.5)\n"
        "        set_number(\"volume\", p.get_volume())\n"
        "        p.set_loop(True)\n"
        "        set_flag(\"loop\", p.get_loop())\n"
        "        set_flag(\"stop_noop\", not p.stop())\n"
        "        set_flag(\"pause_noop\", not p.pause())\n",
        "audio_player");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_player", false);
    ok = ok && k2d::ZenBlackboard::getBool("idle", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("volume"), 0.5f);
    ok = ok && k2d::ZenBlackboard::getBool("loop", false);
    ok = ok && k2d::ZenBlackboard::getBool("stop_noop", false);
    ok = ok && k2d::ZenBlackboard::getBool("pause_noop", false);
    ok = ok && nearEqual(player->volume(), 0.5f);
    ok = ok && player->loop();

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testLight2DApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("torch");
    k2d::Light2D* light = object->addComponent<k2d::Light2D>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Torch(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        l = self.node.get_component<Light2D>()\n"
        "        set_flag(\"has_light\", l != None)\n"
        "        l.set_color(1.0, 0.5, 0.25, 0.75)\n"
        "        r, g, b, a = l.get_color()\n"
        "        set_number(\"r\", r)\n"
        "        set_number(\"g\", g)\n"
        "        set_number(\"b\", b)\n"
        "        set_number(\"a\", a)\n"
        "        l.set_energy(2.5)\n"
        "        set_number(\"energy\", l.get_energy())\n"
        "        l.set_radius(120.0)\n"
        "        set_number(\"radius\", l.get_radius())\n",
        "light2d");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_light", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("r"), 1.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("g"), 0.5f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("b"), 0.25f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("a"), 0.75f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("energy"), 2.5f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("radius"), 120.0f);
    ok = ok && nearEqual(light->color().r, 1.0f) && nearEqual(light->color().g, 0.5f) &&
         nearEqual(light->color().b, 0.25f) && nearEqual(light->color().a, 0.75f);
    ok = ok && nearEqual(light->energy(), 2.5f);
    ok = ok && nearEqual(light->radius(), 120.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testAnimationEvents()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::RouteZenScriptAnimationEvents(scene);

    k2d::GameObject* object = scene.createObject("runner");
    k2d::Animation2D* animation = object->addComponent<k2d::Animation2D>();
    animation->addClip("run", nullptr, 16, 16, 6, 60.0f, k2d::AnimationMode::Loop);
    animation->addEvent("run", 3, "step");
    animation->addEvent("run", 5, "land");

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Runner(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        self.node.get_component<Animation>().play(\"run\")\n"
        "\n"
        "    def on_animation_event(self, name):\n"
        "        set_number(name, get_number(name, 0) + 1)\n"
        "\n"
        "    def on_animation_finished(self, clip):\n"
        "        set_number(\"finished\", get_number(\"finished\", 0) + 1)\n",
        "animation_events");

    // 60 fps clip of 6 frames: 12 frames of playback is exactly two loops.
    for (int i = 0; i < 12; ++i)
        scene.update(1.0f / 60.0f);

    bool ok = loaded && script->loaded();
    const double steps = k2d::ZenBlackboard::getNumber("step", 0.0);
    const double lands = k2d::ZenBlackboard::getNumber("land", 0.0);
    ok = ok && nearEqual((float)steps, 2.0f) && nearEqual((float)lands, 2.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("finished", 0.0), 0.0f);

    std::printf("  animation_events: step=%g land=%g (expected 2 and 2)\n", steps, lands);
    return ok;
}

static bool testAnimationEventsSurviveLagSpike()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::RouteZenScriptAnimationEvents(scene);

    k2d::GameObject* object = scene.createObject("laggy");
    k2d::Animation2D* animation = object->addComponent<k2d::Animation2D>();
    animation->addClip("run", nullptr, 16, 16, 6, 60.0f, k2d::AnimationMode::Loop);
    animation->addEvent("run", 1, "a");
    animation->addEvent("run", 2, "b");
    animation->addEvent("run", 3, "c");

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Laggy(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        self.node.get_component<Animation>().play(\"run\")\n"
        "\n"
        "    def on_animation_event(self, name):\n"
        "        set_number(name, get_number(name, 0) + 1)\n",
        "animation_lag");

    scene.update(0.0f);
    // One update worth five frames at once: nothing may be skipped.
    scene.update(5.0f / 60.0f);

    bool ok = loaded && script->loaded();
    const double a = k2d::ZenBlackboard::getNumber("a", 0.0);
    const double b = k2d::ZenBlackboard::getNumber("b", 0.0);
    const double c = k2d::ZenBlackboard::getNumber("c", 0.0);
    ok = ok && nearEqual((float)a, 1.0f) && nearEqual((float)b, 1.0f) && nearEqual((float)c, 1.0f);

    std::printf("  animation_lag: a=%g b=%g c=%g after one 5-frame step\n", a, b, c);
    return ok;
}

static bool testAnimationFinished()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::RouteZenScriptAnimationEvents(scene);

    k2d::GameObject* object = scene.createObject("attacker");
    k2d::Animation2D* animation = object->addComponent<k2d::Animation2D>();
    animation->addClip("attack", nullptr, 16, 16, 4, 60.0f, k2d::AnimationMode::OneShot);

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Attacker(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        self.node.get_component<Animation>().play(\"attack\")\n"
        "\n"
        "    def on_animation_finished(self, clip):\n"
        "        set_string(\"clip\", clip)\n"
        "        set_number(\"finished\", get_number(\"finished\", 0) + 1)\n",
        "animation_finished");

    for (int i = 0; i < 30; ++i)
        scene.update(1.0f / 60.0f);

    bool ok = loaded && script->loaded();
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("finished", 0.0), 1.0f);
    ok = ok && k2d::ZenBlackboard::getString("clip") == ct::String("attack");
    ok = ok && !animation->playing();

    std::printf("  animation_finished: count=%g clip=%s playing=%d\n",
                k2d::ZenBlackboard::getNumber("finished", 0.0), k2d::ZenBlackboard::getString("clip").c_str(),
                animation->playing() ? 1 : 0);
    return ok;
}

static bool testAnimationEventsRoundTrip()
{
    k2d::Scene source;
    k2d::GameObject* object = source.createObject("clipped");
    k2d::Animation2D* animation = object->addComponent<k2d::Animation2D>();
    animation->addClip("run", nullptr, 16, 16, 6, 12.0f, k2d::AnimationMode::Loop);
    animation->addEvent("run", 2, "step");
    animation->addEvent("run", 4, "land");

    const ct::Json json = k2d::Serializer::WriteObject(*object);

    k2d::Scene target;
    k2d::GameObject* loaded = k2d::Serializer::ReadObject(target, json);
    k2d::Animation2D* out = loaded ? loaded->getComponent<k2d::Animation2D>() : nullptr;

    bool ok = out && out->eventCount("run") == 2;
    if (ok)
    {
        const k2d::AnimationEvent* first = out->eventAt("run", 0);
        const k2d::AnimationEvent* second = out->eventAt("run", 1);
        ok = first && second && first->frame == 2 && second->frame == 4 &&
             first->name == ct::String("step") && second->name == ct::String("land");
    }

    std::printf("  animation_event_serializer: events=%d\n", out ? (int)out->eventCount("run") : -1);
    return ok;
}

static bool testTileMapApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("grid");
    object->setPosition(Math::Vec2(100.0f, 50.0f));
    k2d::TileMapComponent* tileMap = object->addComponent<k2d::TileMapComponent>();
    tileMap->setCellSize(32.0f, 32.0f);
    tileMap->setMapSize(4, 3);

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Grid(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        t = self.node.get_component<TileMap>()\n"
        "        set_flag(\"has_tilemap\", t != None)\n"
        "        set_number(\"columns\", t.get_columns())\n"
        "        set_number(\"rows\", t.get_rows())\n"
        "        cw, ch = t.get_cell_size()\n"
        "        set_number(\"cw\", cw)\n"
        "        set_number(\"ch\", ch)\n"
        "        t.set_tile(2, 1, 5)\n"
        "        set_number(\"tile\", t.get_tile(2, 1))\n"
        "        t.set_collision(2, 1, True)\n"
        "        set_flag(\"solid\", t.has_collision(2, 1))\n"
        "        wx, wy = t.cell_to_world(2, 1)\n"
        "        set_number(\"wx\", wx)\n"
        "        set_number(\"wy\", wy)\n"
        "        cx, cy = t.world_to_cell(wx, wy)\n"
        "        set_number(\"cx\", cx)\n"
        "        set_number(\"cy\", cy)\n",
        "tilemap");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_tilemap", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("columns"), 4.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("rows"), 3.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("cw"), 32.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("ch"), 32.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("tile"), 5.0f);
    ok = ok && k2d::ZenBlackboard::getBool("solid", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("wx"), 164.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("wy"), 82.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("cx"), 2.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("cy"), 1.0f);
    ok = ok && tileMap->getTile(2, 1) == 5;
    ok = ok && tileMap->hasCollision(2, 1);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testNavigationAgentApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* floor = scene.createObject("floor");
    k2d::NavigationRegion2D* region = floor->addComponent<k2d::NavigationRegion2D>();
    const Math::Vec2 polygon[] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(200.0f, 0.0f), Math::Vec2(200.0f, 200.0f),
                                  Math::Vec2(0.0f, 200.0f)};
    region->setPolygon(polygon, 4);

    k2d::GameObject* object = scene.createObject("walker");
    object->setPosition(Math::Vec2(20.0f, 20.0f));
    k2d::NavigationAgent2D* agent = object->addComponent<k2d::NavigationAgent2D>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Walker(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        a = self.node.get_component<NavigationAgent>()\n"
        "        set_flag(\"has_agent\", a != None)\n"
        "        a.set_max_speed(500.0)\n"
        "        set_number(\"max_speed\", a.get_max_speed())\n"
        "        a.set_auto_move(True)\n"
        "        set_flag(\"auto_move\", a.get_auto_move())\n"
        "        set_flag(\"reached\", a.set_target(150.0, 150.0))\n"
        "        set_flag(\"has_path\", a.has_path())\n"
        "        set_number(\"path_count\", a.path_count())\n"
        "        set_flag(\"not_finished\", not a.is_finished())\n"
        "        px, py = a.path_point(a.path_count() - 1)\n"
        "        set_number(\"px\", px)\n"
        "        set_number(\"py\", py)\n"
        "        tx, ty = a.get_target()\n"
        "        set_number(\"tx\", tx)\n"
        "        set_number(\"ty\", ty)\n",
        "nav_agent");

    // Frame 1 runs the script's on_start (sets the target and repaths).
    // The agent's own onUpdate registered before the script, so auto-move
    // only visibly displaces the object from frame 2 onward.
    scene.update(0.016f);
    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_agent", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("max_speed"), 500.0f);
    ok = ok && k2d::ZenBlackboard::getBool("auto_move", false);
    ok = ok && k2d::ZenBlackboard::getBool("reached", false);
    ok = ok && k2d::ZenBlackboard::getBool("has_path", false);
    ok = ok && k2d::ZenBlackboard::getNumber("path_count") >= 1.0;
    ok = ok && k2d::ZenBlackboard::getBool("not_finished", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("px"), 150.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("py"), 150.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("tx"), 150.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("ty"), 150.0f);
    ok = ok && nearEqual(agent->maxSpeed(), 500.0f);
    ok = ok && agent->autoMove();
    ok = ok && nearEqual(agent->targetPosition().x, 150.0f) && nearEqual(agent->targetPosition().y, 150.0f);
    ok = ok && object->position().x > 21.0f && object->position().y > 21.0f;

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testDirectionalLightApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("sun");
    k2d::DirectionalLight2D* light = object->addComponent<k2d::DirectionalLight2D>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Sun(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        l = self.node.get_component<DirectionalLight2D>()\n"
        "        set_flag(\"has_light\", l != None)\n"
        "        l.set_color(0.2, 0.4, 0.6, 0.8)\n"
        "        r, g, b, a = l.get_color()\n"
        "        set_number(\"r\", r)\n"
        "        set_number(\"g\", g)\n"
        "        set_number(\"b\", b)\n"
        "        set_number(\"a\", a)\n"
        "        l.set_energy(3.0)\n"
        "        set_number(\"energy\", l.get_energy())\n",
        "directional_light");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_light", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("r"), 0.2f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("g"), 0.4f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("b"), 0.6f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("a"), 0.8f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("energy"), 3.0f);
    ok = ok && nearEqual(light->color().r, 0.2f) && nearEqual(light->color().g, 0.4f) &&
         nearEqual(light->color().b, 0.6f) && nearEqual(light->color().a, 0.8f);
    ok = ok && nearEqual(light->energy(), 3.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testLightOccluderApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("wall");
    k2d::LightOccluder2D* occluder = object->addComponent<k2d::LightOccluder2D>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Wall(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        o = self.node.get_component<LightOccluder>()\n"
        "        set_flag(\"has_occluder\", o != None)\n"
        "        o.set_points([[0, 0], [10, 0], [10, 20], [0, 20]])\n"
        "        set_number(\"count\", o.point_count())\n"
        "        x2, y2 = o.get_point(2)\n"
        "        set_number(\"x2\", x2)\n"
        "        set_number(\"y2\", y2)\n",
        "occluder");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_occluder", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("count"), 4.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("x2"), 10.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("y2"), 20.0f);
    ok = ok && occluder->points().size() == 4;
    ok = ok && nearEqual(occluder->points()[2].x, 10.0f) && nearEqual(occluder->points()[2].y, 20.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testMotionTweenApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("mover");
    object->setPosition(Math::Vec2(0.0f, 0.0f));
    k2d::MotionTween2D* tween = object->addComponent<k2d::MotionTween2D>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Mover(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        t = self.node.get_component<MotionTween>()\n"
        "        set_flag(\"has_tween\", t != None)\n"
        "        t.set_one_shot(False)\n"
        "        set_flag(\"one_shot\", t.get_one_shot())\n"
        "        t.set_loop(\"repeat\")\n"
        "        set_string(\"loop\", t.get_loop())\n"
        "        t.add_track(\"position\", 0.0, 0.0, 100.0, 0.0, 1.0, 0.0, \"linear\")\n"
        "        set_number(\"tracks\", t.track_count())\n"
        "        t.play(True)\n"
        "        set_flag(\"playing\", t.is_playing())\n"
        "        set_flag(\"not_paused\", not t.is_paused())\n"
        "        set_number(\"time0\", t.get_time())\n",
        "motion_tween");

    // Frame 1 runs on_start (builds the track and plays). The tween's own
    // onUpdate was registered before the script's, so the position track
    // only visibly advances the object from frame 2 onward.
    scene.update(0.016f);
    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_tween", false);
    ok = ok && !k2d::ZenBlackboard::getBool("one_shot", true);
    ok = ok && k2d::ZenBlackboard::getString("loop") == ct::String("repeat");
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("tracks"), 1.0f);
    ok = ok && k2d::ZenBlackboard::getBool("playing", false);
    ok = ok && k2d::ZenBlackboard::getBool("not_paused", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("time0"), 0.0f);
    ok = ok && !tween->oneShot();
    ok = ok && tween->loop() == k2d::MotionTweenLoop::Repeat;
    ok = ok && tween->trackCount() == 1;
    ok = ok && tween->playing();
    ok = ok && nearEqual(object->position().x, 1.6f) && nearEqual(object->position().y, 0.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testMotionStreakApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("blade");
    k2d::MotionStreak2D* streak = object->addComponent<k2d::MotionStreak2D>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Blade(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        s = self.node.get_component<MotionStreak>()\n"
        "        set_flag(\"has_streak\", s != None)\n"
        "        s.set_lifetime(0.25)\n"
        "        set_number(\"lifetime\", s.get_lifetime())\n"
        "        s.set_width(40.0)\n"
        "        set_number(\"width\", s.get_width())\n"
        "        s.set_min_distance(2.0)\n"
        "        set_number(\"min_distance\", s.get_min_distance())\n"
        "        s.set_color(1.0, 0.0, 0.0, 1.0)\n"
        "        r, g, b, a = s.get_color()\n"
        "        set_number(\"r\", r)\n"
        "        set_number(\"g\", g)\n"
        "        s.reset()\n",
        "motion_streak");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_streak", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("lifetime"), 0.25f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("width"), 40.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("min_distance"), 2.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("r"), 1.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("g"), 0.0f);
    ok = ok && nearEqual(streak->lifetime(), 0.25f);
    ok = ok && nearEqual(streak->width(), 40.0f);
    ok = ok && nearEqual(streak->minDistance(), 2.0f);
    ok = ok && nearEqual(streak->color().r, 1.0f) && nearEqual(streak->color().g, 0.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testSpriteBatchApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("particles");
    k2d::SpriteBatch* batch = object->addComponent<k2d::SpriteBatch>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Particles(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        b = self.node.get_component<SpriteBatch>()\n"
        "        set_flag(\"has_batch\", b != None)\n"
        "        i0 = b.add(\"missing_sprite.png\", 10.0, 20.0, 8.0, 8.0, 255, 0, 0, 128)\n"
        "        b.add(\"missing_sprite.png\", 0.0, 0.0, 4.0, 4.0)\n"
        "        set_number(\"count\", b.count())\n"
        "        b.set_source(i0, 1.0, 2.0, 16.0, 16.0)\n"
        "        b.set_flip(i0, True, False)\n"
        "        b.set_color(i0, 0, 255, 0, 255)\n"
        "        b.remove(1)\n"
        "        set_number(\"count_after_remove\", b.count())\n",
        "sprite_batch");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_batch", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("count"), 2.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("count_after_remove"), 1.0f);
    ok = ok && batch->count() == 1;
    const k2d::SpriteBatch::Entry* entry = batch->entry(0);
    ok = ok && entry != nullptr;
    if (entry)
    {
        ok = ok && nearEqual(entry->position.x, 10.0f) && nearEqual(entry->position.y, 20.0f);
        ok = ok && nearEqual(entry->size.x, 8.0f) && nearEqual(entry->size.y, 8.0f);
        ok = ok && nearEqual(entry->source.x, 1.0f) && nearEqual(entry->source.y, 2.0f) &&
             nearEqual(entry->source.z, 16.0f) && nearEqual(entry->source.w, 16.0f);
        ok = ok && entry->flags == 1;
        ok = ok && nearEqual(entry->color.r, 0.0f) && nearEqual(entry->color.g, 1.0f) &&
             nearEqual(entry->color.b, 0.0f) && nearEqual(entry->color.a, 1.0f);
    }

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testLine2DApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("laser");
    k2d::Line2D* line = object->addComponent<k2d::Line2D>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Laser(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        l = self.node.get_component<Line2D>()\n"
        "        set_flag(\"has_line\", l != None)\n"
        "        l.set_points([[0, 0], [50, 0], [50, 30]])\n"
        "        set_number(\"count\", l.point_count())\n"
        "        x1, y1 = l.get_point(1)\n"
        "        set_number(\"x1\", x1)\n"
        "        set_number(\"y1\", y1)\n"
        "        l.set_width(6.0)\n"
        "        set_number(\"width\", l.get_width())\n"
        "        l.set_color(0, 255, 128, 200)\n"
        "        r, g, b, a = l.get_color()\n"
        "        set_number(\"r\", r)\n"
        "        set_number(\"g\", g)\n"
        "        set_number(\"b\", b)\n"
        "        set_number(\"a\", a)\n",
        "line2d");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_line", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("count"), 3.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("x1"), 50.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("y1"), 0.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("width"), 6.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("r"), 0.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("g"), 255.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("b"), 128.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("a"), 200.0f);
    ok = ok && line->points().size() == 3;
    ok = ok && nearEqual(line->points()[2].x, 50.0f) && nearEqual(line->points()[2].y, 30.0f);
    ok = ok && nearEqual(line->width(), 6.0f);
    ok = ok && nearEqual(line->color().r, 0.0f) && nearEqual(line->color().g, 1.0f) &&
         nearEqual(line->color().b, 128.0f / 255.0f) && nearEqual(line->color().a, 200.0f / 255.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testPolygon2DApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("shard");
    k2d::Polygon2D* polygon = object->addComponent<k2d::Polygon2D>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Shard(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        p = self.node.get_component<Polygon2D>()\n"
        "        set_flag(\"has_polygon\", p != None)\n"
        "        set_flag(\"invalid_before\", not p.is_valid())\n"
        "        p.set_points([[0, 0], [40, 0], [40, 40], [0, 40]])\n"
        "        set_number(\"count\", p.point_count())\n"
        "        set_flag(\"valid_after\", p.is_valid())\n"
        "        x2, y2 = p.get_point(2)\n"
        "        set_number(\"x2\", x2)\n"
        "        set_number(\"y2\", y2)\n"
        "        p.set_color(10, 20, 30, 255)\n"
        "        r, g, b, a = p.get_color()\n"
        "        set_number(\"r\", r)\n"
        "        set_number(\"g\", g)\n"
        "        set_number(\"b\", b)\n",
        "polygon2d");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_polygon", false);
    ok = ok && k2d::ZenBlackboard::getBool("invalid_before", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("count"), 4.0f);
    ok = ok && k2d::ZenBlackboard::getBool("valid_after", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("x2"), 40.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("y2"), 40.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("r"), 10.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("g"), 20.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("b"), 30.0f);
    ok = ok && polygon->polygon().size() == 4;
    ok = ok && polygon->valid();
    ok = ok && !polygon->triangles().empty();
    ok = ok && nearEqual(polygon->color().r, 10.0f / 255.0f) && nearEqual(polygon->color().g, 20.0f / 255.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testNinePatchApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("dialog");
    k2d::NinePatchComponent* patch = object->addComponent<k2d::NinePatchComponent>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Dialog(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        n = self.node.get_component<NinePatch>()\n"
        "        set_flag(\"has_patch\", n != None)\n"
        "        n.set_size(240.0, 96.0)\n"
        "        w, h = n.get_size()\n"
        "        set_number(\"w\", w)\n"
        "        set_number(\"h\", h)\n"
        "        n.set_color(200, 210, 220, 255)\n"
        "        r, g, b, a = n.get_color()\n"
        "        set_number(\"r\", r)\n"
        "        set_number(\"g\", g)\n"
        "        set_number(\"b\", b)\n",
        "nine_patch");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_patch", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("w"), 240.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("h"), 96.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("r"), 200.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("g"), 210.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("b"), 220.0f);
    ok = ok && nearEqual(patch->size().x, 240.0f) && nearEqual(patch->size().y, 96.0f);
    ok = ok && nearEqual(patch->color().r, 200.0f / 255.0f) && nearEqual(patch->color().g, 210.0f / 255.0f) &&
         nearEqual(patch->color().b, 220.0f / 255.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testCircleShapeApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("ping");
    k2d::CircleShape* shape = object->addComponent<k2d::CircleShape>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Ping(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        s = self.node.get_component<CircleShape>()\n"
        "        set_flag(\"has_shape\", s != None)\n"
        "        s.set_radius(48.0)\n"
        "        set_number(\"radius\", s.get_radius())\n"
        "        s.set_mode(\"line\")\n"
        "        set_string(\"mode\", s.get_mode())\n"
        "        s.set_line_width(3.0)\n"
        "        set_number(\"line_width\", s.get_line_width())\n"
        "        s.set_color(255, 128, 0, 200)\n"
        "        r, g, b, a = s.get_color()\n"
        "        set_number(\"r\", r)\n"
        "        set_number(\"g\", g)\n"
        "        set_number(\"b\", b)\n"
        "        set_number(\"a\", a)\n",
        "circle_shape");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_shape", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("radius"), 48.0f);
    ok = ok && k2d::ZenBlackboard::getString("mode") == ct::String("line");
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("line_width"), 3.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("r"), 255.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("g"), 128.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("b"), 0.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("a"), 200.0f);
    ok = ok && nearEqual(shape->radius(), 48.0f);
    ok = ok && shape->mode() == k2d::ShapeRenderMode::Line;
    ok = ok && nearEqual(shape->lineWidth(), 3.0f);
    ok = ok && nearEqual(shape->color().r, 1.0f) && nearEqual(shape->color().g, 128.0f / 255.0f) &&
         nearEqual(shape->color().b, 0.0f) && nearEqual(shape->color().a, 200.0f / 255.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testRectShapeApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("box_gizmo");
    k2d::RectShape* shape = object->addComponent<k2d::RectShape>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class BoxGizmo(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        s = self.node.get_component<RectShape>()\n"
        "        set_flag(\"has_shape\", s != None)\n"
        "        s.set_size(64.0, 32.0)\n"
        "        w, h = s.get_size()\n"
        "        set_number(\"w\", w)\n"
        "        set_number(\"h\", h)\n"
        "        s.set_mode(\"line\")\n"
        "        set_string(\"mode\", s.get_mode())\n"
        "        s.set_line_width(2.0)\n"
        "        set_number(\"line_width\", s.get_line_width())\n"
        "        s.set_color(10, 20, 30, 255)\n"
        "        r, g, b, a = s.get_color()\n"
        "        set_number(\"r\", r)\n"
        "        set_number(\"g\", g)\n"
        "        set_number(\"b\", b)\n",
        "rect_shape");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_shape", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("w"), 64.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("h"), 32.0f);
    ok = ok && k2d::ZenBlackboard::getString("mode") == ct::String("line");
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("line_width"), 2.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("r"), 10.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("g"), 20.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("b"), 30.0f);
    ok = ok && nearEqual(shape->size().x, 64.0f) && nearEqual(shape->size().y, 32.0f);
    ok = ok && shape->mode() == k2d::ShapeRenderMode::Line;
    ok = ok && nearEqual(shape->lineWidth(), 2.0f);
    ok = ok && nearEqual(shape->color().r, 10.0f / 255.0f) && nearEqual(shape->color().g, 20.0f / 255.0f) &&
         nearEqual(shape->color().b, 30.0f / 255.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testCapsuleShapeApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("capsule_gizmo");
    k2d::CapsuleShape* shape = object->addComponent<k2d::CapsuleShape>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class CapsuleGizmo(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        s = self.node.get_component<CapsuleShape>()\n"
        "        set_flag(\"has_shape\", s != None)\n"
        "        s.set_size(20.0, 60.0)\n"
        "        w, h = s.get_size()\n"
        "        set_number(\"w\", w)\n"
        "        set_number(\"h\", h)\n"
        "        s.set_mode(\"fill\")\n"
        "        set_string(\"mode\", s.get_mode())\n"
        "        s.set_line_width(1.5)\n"
        "        set_number(\"line_width\", s.get_line_width())\n"
        "        s.set_color(5, 6, 7, 255)\n"
        "        r, g, b, a = s.get_color()\n"
        "        set_number(\"r\", r)\n"
        "        set_number(\"g\", g)\n"
        "        set_number(\"b\", b)\n",
        "capsule_shape");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_shape", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("w"), 20.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("h"), 60.0f);
    ok = ok && k2d::ZenBlackboard::getString("mode") == ct::String("fill");
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("line_width"), 1.5f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("r"), 5.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("g"), 6.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("b"), 7.0f);
    ok = ok && nearEqual(shape->size().x, 20.0f) && nearEqual(shape->size().y, 60.0f);
    ok = ok && shape->mode() == k2d::ShapeRenderMode::Fill;
    ok = ok && nearEqual(shape->lineWidth(), 1.5f);
    ok = ok && nearEqual(shape->color().r, 5.0f / 255.0f) && nearEqual(shape->color().g, 6.0f / 255.0f) &&
         nearEqual(shape->color().b, 7.0f / 255.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testBoxColliderApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("crate");
    object->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    k2d::BoxCollider2D* box = object->addComponent<k2d::BoxCollider2D>();
    box->setSize(Math::Vec2(20.0f, 20.0f));

    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    ct::Vector<k2d::GameObject*> foundBefore;
    scene.overlapCircle(Math::Vec2(60.0f, 0.0f), 5.0f, foundBefore);
    const bool missedBefore = foundBefore.empty();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Crate(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        c = self.node.get_component<BoxCollider>()\n"
        "        set_flag(\"has_collider\", c != None)\n"
        "        c.set_size(200.0, 200.0)\n"
        "        w, h = c.get_size()\n"
        "        set_number(\"w\", w)\n"
        "        set_number(\"h\", h)\n",
        "box_collider");

    scene.update(0.016f);

    ct::Vector<k2d::GameObject*> foundAfter;
    scene.overlapCircle(Math::Vec2(60.0f, 0.0f), 5.0f, foundAfter);
    bool hitAfter = false;
    for (size_t i = 0; i < foundAfter.size(); ++i)
        if (foundAfter[i] == object)
            hitAfter = true;

    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_collider", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("w"), 200.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("h"), 200.0f);
    ok = ok && nearEqual(box->size().x, 200.0f) && nearEqual(box->size().y, 200.0f);
    ok = ok && missedBefore;
    ok = ok && hitAfter;

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testCircleColliderApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("orb");
    object->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    k2d::CircleCollider2D* circle = object->addComponent<k2d::CircleCollider2D>();
    circle->setRadius(10.0f);

    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    ct::Vector<k2d::GameObject*> foundBefore;
    scene.overlapCircle(Math::Vec2(60.0f, 0.0f), 5.0f, foundBefore);
    const bool missedBefore = foundBefore.empty();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Orb(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        c = self.node.get_component<CircleCollider>()\n"
        "        set_flag(\"has_collider\", c != None)\n"
        "        c.set_radius(80.0)\n"
        "        set_number(\"radius\", c.get_radius())\n",
        "circle_collider");

    scene.update(0.016f);

    ct::Vector<k2d::GameObject*> foundAfter;
    scene.overlapCircle(Math::Vec2(60.0f, 0.0f), 5.0f, foundAfter);
    bool hitAfter = false;
    for (size_t i = 0; i < foundAfter.size(); ++i)
        if (foundAfter[i] == object)
            hitAfter = true;

    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_collider", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("radius"), 80.0f);
    ok = ok && nearEqual(circle->radius(), 80.0f);
    ok = ok && missedBefore;
    ok = ok && hitAfter;

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testEdgeColliderApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("wire");
    object->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    k2d::EdgeCollider2D* edge = object->addComponent<k2d::EdgeCollider2D>();

    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    ct::Vector<k2d::GameObject*> foundBefore;
    scene.overlapCircle(Math::Vec2(150.0f, 100.0f), 5.0f, foundBefore);
    const bool missedBefore = foundBefore.empty();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Wire(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        e = self.node.get_component<EdgeCollider>()\n"
        "        set_flag(\"has_collider\", e != None)\n"
        "        e.set_points(100.0, 100.0, 200.0, 100.0)\n"
        "        sx, sy, ex, ey = e.get_points()\n"
        "        set_number(\"sx\", sx)\n"
        "        set_number(\"sy\", sy)\n"
        "        set_number(\"ex\", ex)\n"
        "        set_number(\"ey\", ey)\n",
        "edge_collider");

    scene.update(0.016f);

    ct::Vector<k2d::GameObject*> foundAfter;
    scene.overlapCircle(Math::Vec2(150.0f, 100.0f), 5.0f, foundAfter);
    bool hitAfter = false;
    for (size_t i = 0; i < foundAfter.size(); ++i)
        if (foundAfter[i] == object)
            hitAfter = true;

    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_collider", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("sx"), 100.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("sy"), 100.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("ex"), 200.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("ey"), 100.0f);
    ok = ok && nearEqual(edge->start().x, 100.0f) && nearEqual(edge->start().y, 100.0f);
    ok = ok && nearEqual(edge->end().x, 200.0f) && nearEqual(edge->end().y, 100.0f);
    ok = ok && missedBefore;
    ok = ok && hitAfter;

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testPolygonColliderApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("shard");
    object->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    k2d::PolygonCollider2D* polygon = object->addComponent<k2d::PolygonCollider2D>();

    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    // Default is a regular square of circumradius 16 with a vertex on +x, so
    // a probe at x=20 (between the old and new radius) misses it.
    ct::Vector<k2d::GameObject*> foundBefore;
    scene.overlapCircle(Math::Vec2(20.0f, 0.0f), 2.0f, foundBefore);
    const bool missedBefore = foundBefore.empty();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Shard(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        p = self.node.get_component<PolygonCollider>()\n"
        "        set_flag(\"has_collider\", p != None)\n"
        "        p.set_points([[-40, -40], [40, -40], [40, 40], [-40, 40]])\n"
        "        set_number(\"count\", p.point_count())\n"
        "        x2, y2 = p.get_point(2)\n"
        "        set_number(\"x2\", x2)\n"
        "        set_number(\"y2\", y2)\n"
        "        p.set_regular(6, 24.0)\n"
        "        set_number(\"regular_count\", p.point_count())\n",
        "polygon_collider");

    scene.update(0.016f);

    // The final state after on_start is the regular hexagon (circumradius
    // 24, also with a vertex on +x) which now reaches the same probe point.
    ct::Vector<k2d::GameObject*> foundAfter;
    scene.overlapCircle(Math::Vec2(20.0f, 0.0f), 2.0f, foundAfter);
    bool hitAfter = false;
    for (size_t i = 0; i < foundAfter.size(); ++i)
        if (foundAfter[i] == object)
            hitAfter = true;

    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_collider", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("count"), 4.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("x2"), 40.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("y2"), 40.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("regular_count"), 6.0f);
    ok = ok && polygon->points().size() == 6;
    ok = ok && missedBefore;
    ok = ok && hitAfter;

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testChainColliderApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("fence");
    object->addComponent<k2d::RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    k2d::ChainCollider2D* chain = object->addComponent<k2d::ChainCollider2D>();

    scene.setSimulationEnabled(true);
    scene.update(1.0f / 60.0f);

    ct::Vector<k2d::GameObject*> foundBefore;
    scene.overlapCircle(Math::Vec2(150.0f, 100.0f), 5.0f, foundBefore);
    const bool missedBefore = foundBefore.empty();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Fence(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        c = self.node.get_component<ChainCollider>()\n"
        "        set_flag(\"has_collider\", c != None)\n"
        "        c.set_points([[100, 100], [200, 100], [200, 200]])\n"
        "        set_number(\"count\", c.point_count())\n"
        "        x1, y1 = c.get_point(1)\n"
        "        set_number(\"x1\", x1)\n"
        "        set_number(\"y1\", y1)\n"
        "        c.set_loop(True)\n"
        "        set_flag(\"loop\", c.get_loop())\n",
        "chain_collider");

    scene.update(0.016f);

    ct::Vector<k2d::GameObject*> foundAfter;
    scene.overlapCircle(Math::Vec2(150.0f, 100.0f), 5.0f, foundAfter);
    bool hitAfter = false;
    for (size_t i = 0; i < foundAfter.size(); ++i)
        if (foundAfter[i] == object)
            hitAfter = true;

    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_collider", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("count"), 3.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("x1"), 200.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("y1"), 100.0f);
    ok = ok && k2d::ZenBlackboard::getBool("loop", false);
    ok = ok && chain->points().size() == 3;
    ok = ok && chain->loop();
    ok = ok && missedBefore;
    ok = ok && hitAfter;

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testNavigationRegionApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* floor = scene.createObject("floor");
    k2d::NavigationRegion2D* region = floor->addComponent<k2d::NavigationRegion2D>();

    k2d::GameObject* walker = scene.createObject("walker");
    walker->setPosition(Math::Vec2(10.0f, 10.0f));
    k2d::NavigationAgent2D* agent = walker->addComponent<k2d::NavigationAgent2D>();

    k2d::ZenScriptComponent* script = floor->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class Floor(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        r = self.node.get_component<NavigationRegion>()\n"
        "        set_flag(\"has_region\", r != None)\n"
        "        set_flag(\"invalid_before\", not r.is_valid())\n"
        "        r.set_points([[0, 0], [100, 0], [100, 100], [0, 100]])\n"
        "        set_number(\"count\", r.point_count())\n"
        "        set_flag(\"valid_after\", r.is_valid())\n"
        "        x2, y2 = r.get_point(2)\n"
        "        set_number(\"x2\", x2)\n"
        "        set_number(\"y2\", y2)\n",
        "navigation_region");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("has_region", false);
    ok = ok && k2d::ZenBlackboard::getBool("invalid_before", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("count"), 4.0f);
    ok = ok && k2d::ZenBlackboard::getBool("valid_after", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("x2"), 100.0f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("y2"), 100.0f);
    ok = ok && region->polygon().size() == 4;
    ok = ok && region->valid();
    ok = ok && !region->triangles().empty();

    // Prove the reshaped region is live for pathfinding, not just a stored
    // point list: an agent inside the new polygon should find a real path.
    ok = ok && agent->setTargetPosition(Math::Vec2(80.0f, 80.0f));
    ok = ok && agent->hasPath();

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testAStarGridApi()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("astar_grid");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    // 5x5 grid, no diagonals, a wall at x=2 open only at y=4: the shortest
    // route from (0,2) to (4,2) is forced down to the gap and back up, a
    // hand-countable 8-step, 9-node detour (mirrors k2d_astar_tests.cpp's
    // TestGridWallDetour geometry).
    const bool loaded = script->loadSource(
        "class GridScript(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        grid = AStarGrid()\n"
        "        grid.set_size(5, 5)\n"
        "        grid.set_diagonal_mode(ASTAR_DIAGONAL_NEVER)\n"
        "        for y in range(4):\n"
        "            grid.set_solid(2, y, True)\n"
        "        set_flag(\"gap_open\", not grid.is_solid(2, 4))\n"
        "        path = grid.get_point_path(0, 2, 4, 2)\n"
        "        set_number(\"path_length\", path.len())\n",
        "astar_grid");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    ok = ok && k2d::ZenBlackboard::getBool("gap_open", false);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("path_length"), 9.0f);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testAllComponentHandles()
{
    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("all_components");
    object->addComponent<k2d::SpriteComponent>();
    object->addComponent<k2d::Animation2D>();
    object->addComponent<k2d::CameraComponent>();
    object->addComponent<k2d::ParticleComponent>();
    object->addComponent<k2d::RigidBody2D>();
    object->addComponent<k2d::CharacterBody2D>();
    object->addComponent<k2d::BoxCollider2D>();
    object->addComponent<k2d::CircleCollider2D>();
    object->addComponent<k2d::EdgeCollider2D>();
    object->addComponent<k2d::PolygonCollider2D>();
    object->addComponent<k2d::ChainCollider2D>();
    object->addComponent<k2d::TileMapComponent>();
    object->addComponent<k2d::SpriteBatch>();
    object->addComponent<k2d::Polygon2D>();
    object->addComponent<k2d::Line2D>();
    object->addComponent<k2d::NinePatchComponent>();
    object->addComponent<k2d::Light2D>();
    object->addComponent<k2d::DirectionalLight2D>();
    object->addComponent<k2d::LightOccluder2D>();
    object->addComponent<k2d::AudioPlayer>();
    object->addComponent<k2d::CircleShape>();
    object->addComponent<k2d::RectShape>();
    object->addComponent<k2d::CapsuleShape>();
    object->addComponent<k2d::UiCanvas>();
    object->addComponent<k2d::UiPanel>();
    object->addComponent<k2d::UiLabel>();
    object->addComponent<k2d::UiButton>();
    object->addComponent<k2d::UiCheckBox>();
    object->addComponent<k2d::UiSlider>();
    object->addComponent<k2d::NavigationRegion2D>();
    object->addComponent<k2d::NavigationAgent2D>();
    object->addComponent<k2d::MotionTween2D>();
    object->addComponent<k2d::MotionStreak2D>();
    object->addComponent<k2d::Skeleton2D>();
    object->addComponent<k2d::Bone2D>();

    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const bool loaded = script->loadSource(
        "class AllComponents(ScriptComponent):\n"
        "    def on_start(self):\n"
        "        self.check(\"script\", self.node.get_component<ScriptComponent>())\n"
        "        self.check(\"sprite\", self.node.get_component<Sprite>())\n"
        "        self.check(\"animation\", self.node.get_component<Animation>())\n"
        "        self.check(\"camera\", self.node.get_component<Camera>())\n"
        "        self.check(\"particle\", self.node.get_component<Particle>())\n"
        "        self.check(\"rigid_body\", self.node.get_component<RigidBody>())\n"
        "        self.check(\"character\", self.node.get_component<CharacterBody>())\n"
        "        self.check(\"collider\", self.node.get_component<Collider>())\n"
        "        self.check(\"box_collider\", self.node.get_component<BoxCollider>())\n"
        "        self.check(\"circle_collider\", self.node.get_component<CircleCollider>())\n"
        "        self.check(\"edge_collider\", self.node.get_component<EdgeCollider>())\n"
        "        self.check(\"polygon_collider\", self.node.get_component<PolygonCollider>())\n"
        "        self.check(\"chain_collider\", self.node.get_component<ChainCollider>())\n"
        "        self.check(\"tile_map\", self.node.get_component<TileMap>())\n"
        "        self.check(\"sprite_batch\", self.node.get_component<SpriteBatch>())\n"
        "        self.check(\"polygon\", self.node.get_component<Polygon2D>())\n"
        "        self.check(\"line\", self.node.get_component<Line2D>())\n"
        "        self.check(\"nine_patch\", self.node.get_component<NinePatch>())\n"
        "        self.check(\"light\", self.node.get_component<Light>())\n"
        "        self.check(\"light_2d\", self.node.get_component<Light2D>())\n"
        "        self.check(\"directional_light\", self.node.get_component<DirectionalLight2D>())\n"
        "        self.check(\"occluder\", self.node.get_component<LightOccluder>())\n"
        "        self.check(\"audio\", self.node.get_component<AudioPlayer>())\n"
        "        self.check(\"circle_shape\", self.node.get_component<CircleShape>())\n"
        "        self.check(\"rect_shape\", self.node.get_component<RectShape>())\n"
        "        self.check(\"capsule_shape\", self.node.get_component<CapsuleShape>())\n"
        "        self.check(\"canvas\", self.node.get_component<UiCanvas>())\n"
        "        self.check(\"panel\", self.node.get_component<Panel>())\n"
        "        self.check(\"label\", self.node.get_component<Label>())\n"
        "        self.check(\"button\", self.node.get_component<Button>())\n"
        "        self.check(\"checkbox\", self.node.get_component<CheckBox>())\n"
        "        self.check(\"slider\", self.node.get_component<Slider>())\n"
        "        self.check(\"navigation_region\", self.node.get_component<NavigationRegion>())\n"
        "        self.check(\"navigation_agent\", self.node.get_component<NavigationAgent>())\n"
        "        self.check(\"motion_tween\", self.node.get_component<MotionTween>())\n"
        "        self.check(\"motion_streak\", self.node.get_component<MotionStreak>())\n"
        "        self.check(\"skeleton\", self.node.get_component<Skeleton>())\n"
        "        self.check(\"bone\", self.node.get_component<Bone>())\n"
        "    def check(self, name, component):\n"
        "        set_flag(name, component != None)\n"
        "        if component != None:\n"
        "            component.set_active(False)\n"
        "            set_flag(name + \"_setter\", not component.is_active())\n"
        "            component.set_active(True)\n",
        "all_component_handles");

    scene.update(0.016f);
    bool ok = loaded && script->loaded();
    const char* names[] = {"script", "sprite", "animation", "camera", "particle", "rigid_body", "character",
                           "collider", "box_collider", "circle_collider", "edge_collider", "polygon_collider",
                           "chain_collider", "tile_map", "sprite_batch", "polygon", "line", "nine_patch", "light",
                           "light_2d", "directional_light", "occluder", "audio", "circle_shape", "rect_shape",
                           "capsule_shape", "canvas", "panel", "label", "button", "checkbox", "slider",
                           "navigation_region", "navigation_agent", "motion_tween", "motion_streak",
                           "skeleton", "bone"};
    for (const char* name : names)
    {
        ok = ok && k2d::ZenBlackboard::getBool(name, false);
        char setter[64];
        std::snprintf(setter, sizeof(setter), "%s_setter", name);
        ok = ok && k2d::ZenBlackboard::getBool(setter, false);
    }

    k2d::ZenBlackboard::clear();
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
    k2d::GameObject* object = scene.createObject("listener");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class In:\n"
                                 "    def __init__(self, node):\n"
                                 "        self.node = node\n"
                                 "    def on_update(self, dt):\n"
                                 "        if key_down(KEY_SPACE) and mouse_down(0):\n"
                                 "            self.node.set_position(mouse_x(), mouse_y())\n"
                                 "        if key_down(KEY_ESCAPE):\n"
                                 "            self.node.set_position(-1, -1)\n",
                                 "input");
    scene.update(0.016f);

    ok = ok && nearEqual(object->position().x, 120.0f) && nearEqual(object->position().y, 240.0f);
    k2d::SetZenScriptInput(nullptr);
    return ok;
}

static bool testFadeAndVirtualInput()
{
    k2d::Input input;
    input.SetVirtualKey(44, true);
    bool ok = input.KeyDown(44) && input.KeyPressed(44);
    k2d::InputActionMap actions;
    actions.Bind("jump", 44);
    ok = ok && actions.Down(input, "jump") && actions.Pressed(input, "jump");
    input.NewFrame();
    ok = ok && input.KeyDown(44) && !input.KeyPressed(44);
    input.SetVirtualKey(44, false);
    ok = ok && !input.KeyDown(44) && input.KeyReleased(44);
    ok = ok && actions.Released(input, "jump");

    k2d::ScreenFade fade;
    fade.FadeOut(2.0f);
    fade.Update(0.5f);
    ok = ok && fade.IsFading() && nearEqual(fade.Alpha(), 0.25f) && nearEqual(fade.Progress(), 0.25f);
    fade.FadeIn(1.0f);
    fade.Update(1.0f);
    ok = ok && !fade.IsFading() && fade.IsClear();

    k2d::VirtualPad pad;
    ok = ok && !pad.Enabled();
    pad.AddVirtualKey(16, 100.0f, 100.0f, 80.0f, 60.0f);
    input.OnTouch(99, 120.0f, 120.0f, true, false);
    pad.Update(input, 1000.0f, 500.0f);
    ok = ok && input.KeyDown(16) && input.KeyPressed(16);
    pad.ClearVirtualKeys();
    pad.Update(input, 1000.0f, 500.0f);
    ok = ok && !input.KeyDown(16) && input.KeyReleased(16);
    input.OnTouch(99, 120.0f, 120.0f, false, true);
    pad.SetEnabled(true);
    pad.SetKeyBindings(10, 11, 12, 13, 14, 15);
    input.OnTouch(1, 32.0f, 411.0f, true, false);
    input.OnTouch(2, 911.0f, 411.0f, true, false);
    pad.Update(input, 1000.0f, 500.0f);
    ok = ok && input.KeyDown(10) && input.KeyDown(14) && pad.PrimaryDown();
    input.OnTouch(1, -200.0f, 411.0f, false, false);
    pad.Update(input, 1000.0f, 500.0f);
    ok = ok && input.KeyDown(10) && input.KeyDown(14);
    input.OnTouch(1, 32.0f, 411.0f, false, true);
    input.OnTouch(2, 911.0f, 411.0f, false, true);
    pad.Update(input, 1000.0f, 500.0f);
    ok = ok && !input.KeyDown(10) && !input.KeyDown(14);

    input.OnMouseMove(32.0f, 411.0f);
    input.OnMouseButton(0, true);
    pad.Update(input, 1000.0f, 500.0f);
    ok = ok && input.KeyDown(10);
    input.OnMouseMove(-200.0f, 411.0f);
    pad.Update(input, 1000.0f, 500.0f);
    ok = ok && input.KeyDown(10);
    input.OnMouseButton(0, false);
    pad.Update(input, 1000.0f, 500.0f);
    ok = ok && !input.KeyDown(10);
    return ok;
}

static bool testAudioApi()
{
    k2d::AudioEngine audio;
    audio.SetMasterVolume(0.5f);
    audio.SetSfxVolume(0.25f);
    audio.SetMusicVolume(0.75f);
    bool ok = !audio.Ready() && nearEqual(audio.MasterVolume(), 0.5f) && nearEqual(audio.SfxVolume(), 0.25f) &&
              nearEqual(audio.MusicVolume(), 0.75f);
    audio.SetMasterMuted(true);
    audio.SetSfxMuted(true);
    audio.SetMusicMuted(true);
    ok = ok && audio.MasterMuted() && audio.SfxMuted() && audio.MusicMuted() &&
         !audio.SetListenerPosition({10.0f, 20.0f}) && !audio.SetVoiceSpatial(1, true) && !audio.FadeIn(1, 0.2f) &&
         !audio.FadeOut(1, 0.2f, true) && audio.CrossfadeMusic(1) == 0;
    k2d::UserData settings;
    audio.SaveSettings(settings);
    k2d::AudioEngine restored;
    restored.LoadSettings(settings);
    ok = ok && nearEqual(restored.MasterVolume(), 0.5f) && nearEqual(restored.SfxVolume(), 0.25f) &&
         nearEqual(restored.MusicVolume(), 0.75f) && restored.MasterMuted() && restored.SfxMuted() &&
         restored.MusicMuted();
    ok = ok && audio.LoadSound("missing_audio_file.ogg") == 0 && audio.LoadMusic("missing_audio_file.ogg") == 0 &&
         audio.Play(1) == 0 && audio.PlayMusic(1) == 0 && !audio.Stop(1) && !audio.Pause(1) && !audio.Resume(1);
    const unsigned char encodedAudio[] = {0x52, 0x49, 0x46, 0x46};
    const k2d::AudioEngine::SoundId memorySfx = audio.LoadSoundMemory(encodedAudio, sizeof(encodedAudio));
    const k2d::AudioEngine::SoundId memoryMusic = audio.LoadMusicMemory(encodedAudio, sizeof(encodedAudio));
    ok = ok && memorySfx > 0 && memoryMusic > 0 && audio.Unload(memorySfx) && audio.Unload(memoryMusic);

    k2d::ZenBlackboard::clear();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("audio_script");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    ok = ok && script->loadSource("class AudioScript(ScriptComponent):\n"
                                  "    def on_start(self):\n"
                                  "        audio_set_master_volume(0.5)\n"
                                  "        audio_set_sfx_volume(0.25)\n"
                                  "        audio_set_music_volume(0.75)\n"
                                  "        audio_set_master_muted(True)\n"
                                  "        audio_set_master_muted(False)\n"
                                  "        audio_set_listener_position(10, 20)\n"
                                  "        set_number(\"audio_missing\", audio_load(\"missing_audio_file.ogg\"))\n"
                                  "        set_flag(\"audio_playing\", audio_playing(0))\n",
                                  "audio_api");
    scene.update(0.016f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("audio_missing"), 0.0f) &&
         !k2d::ZenBlackboard::getBool("audio_playing", true) && nearEqual(k2d::GetAudio().MasterVolume(), 0.5f) &&
         nearEqual(k2d::GetAudio().SfxVolume(), 0.25f) && nearEqual(k2d::GetAudio().MusicVolume(), 0.75f);
    k2d::GetAudio().SetMasterVolume(1.0f);
    k2d::GetAudio().SetSfxVolume(1.0f);
    k2d::GetAudio().SetMusicVolume(1.0f);
    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testSceneManager()
{
    k2d::Assets assets;
    k2d::Scene scene;
    k2d::SceneManager manager;
    k2d::GameObject* root = manager.Load(scene, assets, "assets/senes/ui_controls.k2dscene");
    bool ok = root && root->name() == ct::String("UI Controls Example") &&
              root->getComponent<k2d::UiCanvas>() != nullptr && scene.find("Settings Panel") != nullptr;
    manager.Request("assets/senes/ui_controls.k2dscene");
    ok = ok && manager.HasRequest() && manager.ApplyRequest(scene, assets) != nullptr && !manager.HasRequest();
    return ok;
}

static bool testGameViewportMouseInput()
{
    k2d::ZenBlackboard::clear();
    k2d::Input input;
    k2d::SetZenScriptInput(&input);
    k2d::SetZenScriptGameViewport(100.0f, 50.0f, 320.0f, 180.0f);
    k2d::Camera2D camera;
    camera.position = Math::Vec2(10.0f, 20.0f);
    k2d::SetZenScriptGameCamera(&camera);
    input.NewFrame();
    input.OnMouseMove(120.0f, 80.0f);
    input.OnMouseButton(0, true);

    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("game_input");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class GameInput(ScriptComponent):\n"
                                 "    def on_update(self, dt):\n"
                                 "        set_number(\"game_mouse_x\", mouse_x())\n"
                                 "        set_number(\"game_mouse_y\", mouse_y())\n"
                                 "        wx, wy = mouse_world_position()\n"
                                 "        set_number(\"game_world_x\", wx)\n"
                                 "        set_number(\"game_world_y\", wy)\n"
                                 "        set_flag(\"game_mouse_pressed\", mouse_pressed(0))\n",
                                 "game_viewport_input");
    scene.update(0.016f);
    ok = ok && nearEqual((float)k2d::ZenBlackboard::getNumber("game_mouse_x"), 20.0f) &&
         nearEqual((float)k2d::ZenBlackboard::getNumber("game_mouse_y"), 30.0f) &&
         nearEqual((float)k2d::ZenBlackboard::getNumber("game_world_x"), -130.0f) &&
         nearEqual((float)k2d::ZenBlackboard::getNumber("game_world_y"), -40.0f) &&
         k2d::ZenBlackboard::getBool("game_mouse_pressed", false);

    k2d::SetZenScriptGameViewport(0.0f, 0.0f, 0.0f, 0.0f);
    k2d::SetZenScriptGameCamera(nullptr);
    k2d::SetZenScriptInput(nullptr);
    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testDestroy()
{
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("mortal");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class Mortal:\n"
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

    const std::string scriptPath = k2d_tests::tempPath("k2d_zen_test_script.py");
    FILE* file = std::fopen(scriptPath.c_str(), "w");
    if (!file)
        return false;
    std::fputs("class S:\n    def __init__(self, node):\n        self.node = node\n    def on_update(self, dt):\n      "
               "  self.node.set_position(33, 44)\n",
               file);
    std::fclose(file);

    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("scripted");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadFile(scriptPath.c_str());

    const ct::Json json = k2d::Serializer::WriteObject(*object);
    const ct::Json& components = json["components"];
    bool foundEntry = false;
    for (size_t i = 0; i < components.size(); ++i)
        if (std::strcmp(components[i]["type"].as_cstr(""), "ZenScript") == 0 &&
            std::strcmp(components[i]["data"]["path"].as_cstr(""), scriptPath.c_str()) == 0)
            foundEntry = true;
    ok = ok && foundEntry;

    k2d::Scene loadedScene;
    k2d::GameObject* loaded = k2d::Serializer::ReadObject(loadedScene, json);
    ok = ok && loaded != nullptr;
    k2d::ZenScriptComponent* loadedScript = loaded ? loaded->getComponent<k2d::ZenScriptComponent>() : nullptr;
    ok = ok && loadedScript && loadedScript->loaded() && loadedScript->scriptPath() == ct::String(scriptPath.c_str());

    loadedScene.update(0.016f);
    ok = ok && loaded && nearEqual(loaded->position().x, 33.0f) && nearEqual(loaded->position().y, 44.0f);

    std::remove(scriptPath.c_str());
    return ok;
}

static ct::String gCapturedOutput;
static bool gCapturedError = false;

static bool testSpawnAndMath()
{
    const std::string prefabPath = k2d_tests::tempPath("k2d_zen_test_prefab.k2dprefab");
    FILE* file = std::fopen(prefabPath.c_str(), "w");
    if (!file)
        return false;
    std::fputs("{\"name\":\"bullet\",\"components\":[],\"children\":[]}", file);
    std::fclose(file);

    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("shooter");
    object->setPosition(Math::Vec2(0.0f, 0.0f));
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    const std::string source =
        std::string("class Shooter:\n") +
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "    def on_start(self):\n"
        "        b = self.node.spawn(\"" + prefabPath + "\", 30, 40)\n"
        "        print(\"spawned\", b.get_name(), self.node.distance_to(30, 40))\n"
        "        self.node.look_at(0, 100)\n"
        "    def on_update(self, dt):\n"
        "        self.node.move_toward(60, 0, 25)\n";
    bool ok = script->loadSource(source.c_str(), "spawner");

    k2d::SetZenScriptOutput(
        [](const char* text, bool isError, void*)
        {
            gCapturedOutput += text;
            if (isError)
                gCapturedError = true;
        },
        nullptr);
    scene.update(0.016f);
    k2d::SetZenScriptOutput(nullptr, nullptr);

    k2d::GameObject* bullet = scene.find("bullet");
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

    std::remove(prefabPath.c_str());
    return ok;
}

static bool testScriptsGate()
{
    k2d::SetZenScriptsEnabled(false);
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("gated");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class G:\n    def __init__(self, node):\n        self.node = node\n"
                                 "    def on_update(self, dt):\n        self.node.set_position(9, 9)\n",
                                 "gate");

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
    k2d::GameObject* sender = scene.createObject("sender");
    k2d::ZenScriptComponent* senderScript = sender->addComponent<k2d::ZenScriptComponent>();
    bool ok = senderScript->loadSource("class Sender:\n"
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

    k2d::GameObject* receiver = scene.createObject("receiver");
    k2d::ZenScriptComponent* receiverScript = receiver->addComponent<k2d::ZenScriptComponent>();
    ok = ok && receiverScript->loadSource("class Receiver:\n"
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
    k2d::GameObject* reader = scene.createObject("reader");
    k2d::ZenScriptComponent* readerScript = reader->addComponent<k2d::ZenScriptComponent>();
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
    k2d::ZenBlackboard::setHostHandler(
        [](const char* name, double value, void*)
        {
            if (std::strcmp(name, "hit") == 0)
            {
                ++hostSeen;
                hostValue = value;
            }
        },
        nullptr);
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
    const std::string path = k2d_tests::tempPath("k2d_zen_hotreload.py");
    FILE* file = std::fopen(path.c_str(), "w");
    if (!file)
        return false;
    std::fputs("class H:\n    def __init__(self, node):\n        self.node = node\n    def on_update(self, dt):\n      "
               "  self.node.set_position(1, 1)\n",
               file);
    std::fclose(file);

    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("reloader");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadFile(path.c_str());
    scene.update(0.016f);
    ok = ok && nearEqual(object->position().x, 1.0f);

    ok = ok && !script->reloadIfChanged();
    ok = ok && k2d::ReloadChangedZenScripts() == 0;

    std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + std::chrono::seconds(2));
    file = std::fopen(path.c_str(), "w");
    if (!file)
        return false;
    std::fputs("class H:\n    def __init__(self, node):\n        self.node = node\n    def on_update(self, dt):\n      "
               "  self.node.set_position(2, 2)\n",
               file);
    std::fclose(file);
    std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + std::chrono::seconds(4));

    ok = ok && k2d::ReloadChangedZenScripts() == 1;
    scene.update(0.016f);
    ok = ok && nearEqual(object->position().x, 2.0f);
    ok = ok && k2d::ReloadChangedZenScripts() == 0;

    std::remove(path.c_str());
    return ok;
}

static bool testNetAndHttpModulesImport()
{
    k2d::ZenBlackboard::clear();

    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("importer");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("import net\n"
                                 "import http\n"
                                 "import json\n"
                                 "\n"
                                 "class Importer:\n"
                                 "    def __init__(self, node):\n"
                                 "        self.node = node\n"
                                 "\n"
                                 "    def on_start(self):\n"
                                 "        set_flag(\"net\", net != None)\n"
                                 "        set_flag(\"http\", http != None)\n"
                                 "        set_flag(\"json\", json != None)\n",
                                 "modules");

    scene.update(0.016f);

    ok = ok && k2d::ZenBlackboard::getBool("net", false);
    ok = ok && k2d::ZenBlackboard::getBool("http", false);
    ok = ok && k2d::ZenBlackboard::getBool("json", false);

    k2d::ZenBlackboard::clear();
    return ok;
}

static bool testExampleScripts()
{
    k2d::Scene scene;
    k2d::GameObject* player = scene.createObject("player");
    player->setPosition(Math::Vec2(10.0f, 20.0f));
    k2d::ZenScriptComponent* playerScript = player->addComponent<k2d::ZenScriptComponent>();
    bool ok = playerScript->loadFile("../assets/scripts/player.py");

    k2d::GameObject* satellite = scene.createObject("satellite");
    k2d::ZenScriptComponent* orbitScript = satellite->addComponent<k2d::ZenScriptComponent>();
    ok = ok && orbitScript->loadFile("../assets/scripts/orbit.py");

    scene.update(0.016f);
    ok = ok && std::fabs(satellite->position().x - 10.0f) < 200.0f &&
         std::fabs(satellite->position().y - 20.0f) < 200.0f && satellite->rotationDegrees() > 0.0f;

    k2d::GameObject* spawner = scene.createObject("spawner");
    k2d::ZenScriptComponent* spawnerScript = spawner->addComponent<k2d::ZenScriptComponent>();
    ok = ok && spawnerScript->loadFile("../assets/scripts/spawner.py");

    k2d::GameObject* hud = scene.createObject("hud");
    k2d::ZenScriptComponent* hudScript = hud->addComponent<k2d::ZenScriptComponent>();
    ok = ok && hudScript->loadFile("../assets/scripts/hud.py");

    k2d::GameObject* bunnymarkMain = scene.createObject("bunnymark_main");
    k2d::ZenScriptComponent* bunnymarkMainScript = bunnymarkMain->addComponent<k2d::ZenScriptComponent>();
    ok = ok && bunnymarkMainScript->loadFile("../assets/scripts/bunnymark_main.py");

    k2d::GameObject* bunnymarkBunny = scene.createObject("bunnymark_bunny");
    k2d::ZenScriptComponent* bunnymarkBunnyScript = bunnymarkBunny->addComponent<k2d::ZenScriptComponent>();
    ok = ok && bunnymarkBunnyScript->loadFile("../assets/scripts/bunnymark_bunny.py");

    k2d::GameObject* fireworksDirector = scene.createObject("fireworks_director");
    k2d::ZenScriptComponent* fireworksDirectorScript = fireworksDirector->addComponent<k2d::ZenScriptComponent>();
    ok = ok && fireworksDirectorScript->loadFile("../assets/scripts/fireworks_director.py");

    k2d::GameObject* fireworkRocket = scene.createObject("firework_rocket");
    k2d::ZenScriptComponent* fireworkRocketScript = fireworkRocket->addComponent<k2d::ZenScriptComponent>();
    ok = ok && fireworkRocketScript->loadFile("../assets/scripts/firework_rocket.py");

    k2d::GameObject* fireworkSpark = scene.createObject("firework_spark");
    k2d::ZenScriptComponent* fireworkSparkScript = fireworkSpark->addComponent<k2d::ZenScriptComponent>();
    ok = ok && fireworkSparkScript->loadFile("../assets/scripts/firework_spark.py");

    k2d::GameObject* fireworkTrail = scene.createObject("firework_trail");
    k2d::ZenScriptComponent* fireworkTrailScript = fireworkTrail->addComponent<k2d::ZenScriptComponent>();
    ok = ok && fireworkTrailScript->loadFile("../assets/scripts/firework_trail.py");

    k2d::GameObject* fireworkExplosion = scene.createObject("firework_explosion");
    k2d::ZenScriptComponent* fireworkExplosionScript = fireworkExplosion->addComponent<k2d::ZenScriptComponent>();
    ok = ok && fireworkExplosionScript->loadFile("../assets/scripts/firework_explosion.py");

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

static bool testUiSerializationAndInput()
{
    k2d::Scene scene;
    k2d::GameObject* canvas = scene.createObject("ui");
    canvas->addComponent<k2d::UiCanvas>();
    k2d::GameObject* buttonObject = scene.createObject("play", canvas);
    k2d::UiButton* button = buttonObject->addComponent<k2d::UiButton>();
    button->setText("Play");
    button->setAnchors(Math::Vec4(0.5f, 0.5f, 0.5f, 0.5f));
    button->setOffsets(Math::Vec4(-60.0f, -18.0f, 60.0f, 18.0f));
    k2d::ZenScriptComponent* script = buttonObject->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class UiScript(ScriptComponent):\n"
                                 "    def on_start(self):\n"
                                 "        self.button = self.node.get_button()\n"
                                 "    def on_update(self, dt):\n"
                                 "        if self.button.clicked():\n"
                                 "            set_flag('ui_clicked', True)\n",
                                 "ui_script");

    k2d::GameObject* sliderObject = scene.createObject("volume", canvas);
    k2d::UiSlider* slider = sliderObject->addComponent<k2d::UiSlider>();
    slider->setRect(10.0f, 20.0f, 200.0f, 24.0f);
    slider->setRange(0.0f, 100.0f);
    slider->setValue(25.0f);

    const ct::Json json = k2d::Serializer::WriteObject(*canvas);
    k2d::Scene loadedScene;
    k2d::GameObject* loaded = k2d::Serializer::ReadObject(loadedScene, json);
    ok = ok && loaded && loaded->getComponent<k2d::UiCanvas>() && loaded->childCount() == 2;
    k2d::GameObject* loadedButtonNode = loaded ? loaded->findChild("play") : nullptr;
    k2d::UiButton* loadedButton = loadedButtonNode ? loadedButtonNode->getComponent<k2d::UiButton>() : nullptr;
    ok = ok && loadedButton && loadedButton->text() == ct::String("Play") &&
         nearEqual(loadedButton->anchors().x, 0.5f) && nearEqual(loadedButton->offsets().x, -60.0f);

    k2d::Input input;
    k2d::SetUiInput(&input);
    k2d::SetUiViewport(0.0f, 0.0f, 320.0f, 180.0f);
    input.OnMouseMove(160.0f, 90.0f);
    input.OnMouseButton(0, true);
    scene.update(0.016f);
    input.NewFrame();
    input.OnMouseButton(0, false);
    scene.update(0.016f);
    scene.update(0.016f);
    ok = ok && k2d::ZenBlackboard::getBool("ui_clicked", false);
    k2d::ZenBlackboard::clear();
    k2d::SetUiInput(nullptr);
    k2d::SetUiViewport(0.0f, 0.0f, 0.0f, 0.0f);
    return ok;
}

int main()
{
    k2d::FileSystem::Instance().Init();
    k2d::SetZenScriptsEnabled(true);

    const bool basics = testBasics();
    const bool scriptBase = testScriptComponentBaseProvidesNode();
    const bool drawApi = testDrawApi();
    const bool objectCount = testObjectCount();
    const bool bunnymark = testBunnymarkSpawn();
    const bool hierarchy = testHierarchy();
    const bool components = testComponents();
    const bool genericAngleBrackets = testGenericAngleBracketCalls();
    const bool allComponentHandles = testAllComponentHandles();
    const bool skeletonOk = testSkeleton();
    const bool audioPlayerApi = testAudioPlayerApi();
    const bool light2DApi = testLight2DApi();
    const bool tileMapApi = testTileMapApi();
    const bool animationEvents = testAnimationEvents();
    const bool animationLag = testAnimationEventsSurviveLagSpike();
    const bool animationFinished = testAnimationFinished();
    const bool animationEventsSaved = testAnimationEventsRoundTrip();
    const bool navigationAgentApi = testNavigationAgentApi();
    const bool directionalLightApi = testDirectionalLightApi();
    const bool lightOccluderApi = testLightOccluderApi();
    const bool motionTweenApi = testMotionTweenApi();
    const bool motionStreakApi = testMotionStreakApi();
    const bool spriteBatchApi = testSpriteBatchApi();
    const bool line2DApi = testLine2DApi();
    const bool polygon2DApi = testPolygon2DApi();
    const bool ninePatchApi = testNinePatchApi();
    const bool circleShapeApi = testCircleShapeApi();
    const bool rectShapeApi = testRectShapeApi();
    const bool capsuleShapeApi = testCapsuleShapeApi();
    const bool boxColliderApi = testBoxColliderApi();
    const bool circleColliderApi = testCircleColliderApi();
    const bool edgeColliderApi = testEdgeColliderApi();
    const bool polygonColliderApi = testPolygonColliderApi();
    const bool chainColliderApi = testChainColliderApi();
    const bool navigationRegionApi = testNavigationRegionApi();
    const bool astarGridApi = testAStarGridApi();
    const bool nodeApi = testNodeApi();
    const bool activeCamera = testActiveCamera();
    const bool inputOk = testInput();
    const bool fadeVirtualInput = testFadeAndVirtualInput();
    const bool audioApi = testAudioApi();
    const bool sceneManager = testSceneManager();
    const bool gameViewportInput = testGameViewportMouseInput();
    const bool destroy = testDestroy();
    const bool serialization = testSerialization();
    const bool spawnMath = testSpawnAndMath();
    const bool gate = testScriptsGate();
    const bool channel = testBlackboardAndEvents();
    const bool hotReload = testHotReload();
    const bool modules = testNetAndHttpModulesImport();
    const bool examples = testExampleScripts();
    const bool ui = testUiSerializationAndInput();

    std::printf(
        "zen: basics=%s script_base=%s draw_api=%s object_count=%s bunnymark=%s hierarchy=%s components=%s "
        "generic_angle_brackets=%s all_component_handles=%s skeleton=%s audio_player=%s light_2d=%s tile_map=%s "
        "animation_events=%s animation_lag=%s animation_finished=%s animation_events_saved=%s "
        "navigation_agent=%s directional_light=%s light_occluder=%s motion_tween=%s motion_streak=%s sprite_batch=%s "
        "line_2d=%s polygon_2d=%s nine_patch=%s "
        "circle_shape=%s rect_shape=%s capsule_shape=%s box_collider=%s circle_collider=%s edge_collider=%s "
        "polygon_collider=%s chain_collider=%s navigation_region=%s astar_grid=%s "
        "node_api=%s active_camera=%s input=%s fade_virtual_input=%s audio_api=%s scene_manager=%s game_viewport_input=%s destroy=%s serialization=%s "
        "spawn_math=%s gate=%s channel=%s hot_reload=%s modules=%s examples=%s ui=%s\n",
        basics ? "pass" : "fail", scriptBase ? "pass" : "fail", drawApi ? "pass" : "fail",
        objectCount ? "pass" : "fail", bunnymark ? "pass" : "fail", hierarchy ? "pass" : "fail",
        components ? "pass" : "fail", genericAngleBrackets ? "pass" : "fail", allComponentHandles ? "pass" : "fail",
        skeletonOk ? "pass" : "fail", audioPlayerApi ? "pass" : "fail", light2DApi ? "pass" : "fail",
        tileMapApi ? "pass" : "fail", animationEvents ? "pass" : "fail", animationLag ? "pass" : "fail",
        animationFinished ? "pass" : "fail", animationEventsSaved ? "pass" : "fail",
        navigationAgentApi ? "pass" : "fail",
        directionalLightApi ? "pass" : "fail", lightOccluderApi ? "pass" : "fail", motionTweenApi ? "pass" : "fail",
        motionStreakApi ? "pass" : "fail", spriteBatchApi ? "pass" : "fail", line2DApi ? "pass" : "fail",
        polygon2DApi ? "pass" : "fail", ninePatchApi ? "pass" : "fail",
        circleShapeApi ? "pass" : "fail", rectShapeApi ? "pass" : "fail", capsuleShapeApi ? "pass" : "fail",
        boxColliderApi ? "pass" : "fail", circleColliderApi ? "pass" : "fail", edgeColliderApi ? "pass" : "fail",
        polygonColliderApi ? "pass" : "fail", chainColliderApi ? "pass" : "fail", navigationRegionApi ? "pass" : "fail",
        astarGridApi ? "pass" : "fail",
        nodeApi ? "pass" : "fail", activeCamera ? "pass" : "fail",
        inputOk ? "pass" : "fail", fadeVirtualInput ? "pass" : "fail",
        audioApi ? "pass" : "fail", sceneManager ? "pass" : "fail", gameViewportInput ? "pass" : "fail",
        destroy ? "pass" : "fail", serialization ? "pass" : "fail", spawnMath ? "pass" : "fail", gate ? "pass" : "fail",
        channel ? "pass" : "fail", hotReload ? "pass" : "fail", modules ? "pass" : "fail", examples ? "pass" : "fail",
        ui ? "pass" : "fail");
    const bool passed = basics && scriptBase && drawApi && objectCount && bunnymark && hierarchy && components && genericAngleBrackets &&
                        allComponentHandles && skeletonOk && audioPlayerApi && light2DApi && tileMapApi &&
                        animationEvents && animationLag && animationFinished && animationEventsSaved &&
                        navigationAgentApi &&
                        directionalLightApi && lightOccluderApi && motionTweenApi && motionStreakApi &&
                        spriteBatchApi && line2DApi && polygon2DApi && ninePatchApi &&
                        circleShapeApi && rectShapeApi && capsuleShapeApi &&
                        boxColliderApi && circleColliderApi && edgeColliderApi && polygonColliderApi &&
                        chainColliderApi && navigationRegionApi && astarGridApi &&
                        nodeApi && activeCamera &&
                        inputOk && fadeVirtualInput && audioApi && sceneManager && gameViewportInput && destroy &&
                        serialization && spawnMath && gate && channel && hotReload && modules && examples && ui;
    k2d::FileSystem::Instance().Shutdown();
    return passed ? 0 : 1;
}
