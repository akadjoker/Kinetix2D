#include <k2d/Animation2D.h>
#include <k2d/AudioPlayer.h>
#include <k2d/AudioEngine.h>
#include <k2d/Assets.h>
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
#include <k2d/PhysicsWorld2D.h>
#include <k2d/PolygonCollider2D.h>
#include <k2d/RigidBody2D.h>

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
                           "navigation_region", "navigation_agent", "motion_tween", "motion_streak"};
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

    const char* scriptPath = "/tmp/k2d_zen_test_script.py";
    FILE* file = std::fopen(scriptPath, "w");
    if (!file)
        return false;
    std::fputs("class S:\n    def __init__(self, node):\n        self.node = node\n    def on_update(self, dt):\n      "
               "  self.node.set_position(33, 44)\n",
               file);
    std::fclose(file);

    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("scripted");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadFile(scriptPath);

    const ct::Json json = k2d::Serializer::WriteObject(*object);
    const ct::Json& components = json["components"];
    bool foundEntry = false;
    for (size_t i = 0; i < components.size(); ++i)
        if (std::strcmp(components[i]["type"].as_cstr(""), "ZenScript") == 0 &&
            std::strcmp(components[i]["data"]["path"].as_cstr(""), scriptPath) == 0)
            foundEntry = true;
    ok = ok && foundEntry;

    k2d::Scene loadedScene;
    k2d::GameObject* loaded = k2d::Serializer::ReadObject(loadedScene, json);
    ok = ok && loaded != nullptr;
    k2d::ZenScriptComponent* loadedScript = loaded ? loaded->getComponent<k2d::ZenScriptComponent>() : nullptr;
    ok = ok && loadedScript && loadedScript->loaded() && loadedScript->scriptPath() == ct::String(scriptPath);

    loadedScene.update(0.016f);
    ok = ok && loaded && nearEqual(loaded->position().x, 33.0f) && nearEqual(loaded->position().y, 44.0f);

    std::remove(scriptPath);
    return ok;
}

static ct::String gCapturedOutput;
static bool gCapturedError = false;

static bool testSpawnAndMath()
{
    const char* prefabPath = "/tmp/k2d_zen_test_prefab.k2dprefab";
    FILE* file = std::fopen(prefabPath, "w");
    if (!file)
        return false;
    std::fputs("{\"name\":\"bullet\",\"components\":[],\"children\":[]}", file);
    std::fclose(file);

    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("shooter");
    object->setPosition(Math::Vec2(0.0f, 0.0f));
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadSource("class Shooter:\n"
                                 "    def __init__(self, node):\n"
                                 "        self.node = node\n"
                                 "    def on_start(self):\n"
                                 "        b = self.node.spawn(\"/tmp/k2d_zen_test_prefab.k2dprefab\", 30, 40)\n"
                                 "        print(\"spawned\", b.get_name(), self.node.distance_to(30, 40))\n"
                                 "        self.node.look_at(0, 100)\n"
                                 "    def on_update(self, dt):\n"
                                 "        self.node.move_toward(60, 0, 25)\n",
                                 "spawner");

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

    std::remove(prefabPath);
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
    const char* path = "/tmp/k2d_zen_hotreload.py";
    FILE* file = std::fopen(path, "w");
    if (!file)
        return false;
    std::fputs("class H:\n    def __init__(self, node):\n        self.node = node\n    def on_update(self, dt):\n      "
               "  self.node.set_position(1, 1)\n",
               file);
    std::fclose(file);

    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("reloader");
    k2d::ZenScriptComponent* script = object->addComponent<k2d::ZenScriptComponent>();
    bool ok = script->loadFile(path);
    scene.update(0.016f);
    ok = ok && nearEqual(object->position().x, 1.0f);

    ok = ok && !script->reloadIfChanged();
    ok = ok && k2d::ReloadChangedZenScripts() == 0;

    std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + std::chrono::seconds(2));
    file = std::fopen(path, "w");
    std::fputs("class H:\n    def __init__(self, node):\n        self.node = node\n    def on_update(self, dt):\n      "
               "  self.node.set_position(2, 2)\n",
               file);
    std::fclose(file);
    std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + std::chrono::seconds(4));

    ok = ok && k2d::ReloadChangedZenScripts() == 1;
    scene.update(0.016f);
    ok = ok && nearEqual(object->position().x, 2.0f);
    ok = ok && k2d::ReloadChangedZenScripts() == 0;

    std::remove(path);
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
        "generic_angle_brackets=%s all_component_handles=%s input=%s fade_virtual_input=%s audio_api=%s scene_manager=%s game_viewport_input=%s destroy=%s serialization=%s "
        "spawn_math=%s gate=%s channel=%s hot_reload=%s modules=%s examples=%s ui=%s\n",
        basics ? "pass" : "fail", scriptBase ? "pass" : "fail", drawApi ? "pass" : "fail",
        objectCount ? "pass" : "fail", bunnymark ? "pass" : "fail", hierarchy ? "pass" : "fail",
        components ? "pass" : "fail", genericAngleBrackets ? "pass" : "fail", allComponentHandles ? "pass" : "fail",
        inputOk ? "pass" : "fail", fadeVirtualInput ? "pass" : "fail",
        audioApi ? "pass" : "fail", sceneManager ? "pass" : "fail", gameViewportInput ? "pass" : "fail",
        destroy ? "pass" : "fail", serialization ? "pass" : "fail", spawnMath ? "pass" : "fail", gate ? "pass" : "fail",
        channel ? "pass" : "fail", hotReload ? "pass" : "fail", modules ? "pass" : "fail", examples ? "pass" : "fail",
        ui ? "pass" : "fail");
    const bool passed = basics && scriptBase && drawApi && objectCount && bunnymark && hierarchy && components && genericAngleBrackets &&
                        allComponentHandles &&
                        inputOk && fadeVirtualInput && audioApi && sceneManager && gameViewportInput && destroy &&
                        serialization && spawnMath && gate && channel && hotReload && modules && examples && ui;
    k2d::FileSystem::Instance().Shutdown();
    return passed ? 0 : 1;
}
