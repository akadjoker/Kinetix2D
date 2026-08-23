#include <k2d/GameObject.h>
#include <k2d/Scene.h>
#include <k2d/ZenScriptComponent.h>

#include <cmath>
#include <cstdio>

static bool nearEqual(float a, float b)
{
    return std::fabs(a - b) < 0.001f;
}

int main()
{
    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("player");
    object->setPosition(Math::Vec2(5.0f, 6.0f));

    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    const bool compiled = script->loadSource(
        "ready_count = 0\n"
        "seen_name = \"\"\n"
        "seen_x = -1.0\n"
        "def ready(node):\n"
        "    global ready_count, seen_name, seen_x\n"
        "    ready_count = ready_count + 1\n"
        "    seen_name = node.get_name()\n"
        "    seen_x = node.get_x()\n"
        "def update(node, dt):\n"
        "    node.translate(10 * dt, 0)\n"
        "    node.set_rotation(node.get_rotation() + 90 * dt)\n",
        "test_script");
    bool ok = compiled && script->loaded();

    scene.update(0.5f);
    ok = ok && nearEqual(object->position().x, 10.0f) && nearEqual(object->position().y, 6.0f);
    ok = ok && nearEqual(object->rotationDegrees(), 45.0f);

    scene.update(0.5f);
    const bool moved = nearEqual(object->position().x, 15.0f) && nearEqual(object->rotationDegrees(), 90.0f);
    ok = ok && moved;

    k2d::ZenScriptComponent probe;
    const bool badSource = !probe.loadSource("def broken(:\n", "bad") && !probe.loaded();
    ok = ok && badSource;

    k2d::GameObject *other = scene.createObject("enemy");
    k2d::ZenScriptComponent *setter = other->addComponent<k2d::ZenScriptComponent>();
    const bool compiled2 = setter->loadSource(
        "def update(node, dt):\n"
        "    node.set_position(100, 200)\n"
        "    node.set_scale(2, 3)\n"
        "    node.set_visible(False)\n",
        "setter_script");
    scene.update(0.016f);
    const bool applied = compiled2 &&
                         nearEqual(other->position().x, 100.0f) &&
                         nearEqual(other->position().y, 200.0f) &&
                         nearEqual(other->scale().x, 2.0f) &&
                         nearEqual(other->scale().y, 3.0f) &&
                         !other->visible();
    ok = ok && applied;

    std::printf("zen: compiled=%s moved=%s bad_source=%s setters=%s\n",
                compiled ? "pass" : "fail", moved ? "pass" : "fail",
                badSource ? "pass" : "fail", applied ? "pass" : "fail");
    return ok ? 0 : 1;
}
