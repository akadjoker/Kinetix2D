#include <k2d/Scene.h>
#include <k2d/ScriptComponent.h>

#include <cstdio>
#include <cmath>

namespace
{
    int gUpdates = 0;
    int gLateUpdates = 0;

    class CounterScript : public k2d::ScriptComponent
    {
    protected:
        void onUpdate(float deltaTime) override
        {
            ++gUpdates;
            owner()->translate(glm::vec2(deltaTime, 0.0f));
        }

        void onLateUpdate(float) override
        {
            ++gLateUpdates;
        }
    };

    bool Near(float a, float b)
    {
        return std::fabs(a - b) < 0.0001f;
    }
}

int main()
{
    k2d::Scene scene;
    k2d::GameObject *parent = scene.createObject("parent");
    k2d::GameObject *child = scene.createObject("child", parent);
    child->setPosition(glm::vec2(10.0f, 5.0f));
    parent->setPosition(glm::vec2(20.0f, 30.0f));
    parent->setRotationDegrees(90.0f);
    child->addComponent<CounterScript>();

    bool hierarchy = scene.objectCount() == 2 && scene.find("child") == child &&
                     child->parent() == parent;
    glm::vec2 global = child->globalPosition();
    bool transform = Near(global.x, 15.0f) && Near(global.y, 40.0f);

    gUpdates = 0;
    gLateUpdates = 0;
    scene.update(0.5f);
    bool lifecycle = gUpdates == 1 && gLateUpdates == 1 &&
                    Near(child->position().x, 10.5f);

    parent->setActive(false);
    scene.update(0.5f);
    bool hierarchyActivation = gUpdates == 1 && gLateUpdates == 1;
    parent->setActive(true);

    scene.destroy(child);
    bool deferred = scene.objectCount() == 2 && child->disposed();
    scene.update(0.0f);
    bool destroyed = scene.objectCount() == 1 && scene.find("child") == nullptr &&
                     parent->childCount() == 0;

    scene.clear();
    bool cleared = scene.objectCount() == 0 && scene.root().childCount() == 0;

    std::printf("scene: hierarchy=%s transform=%s lifecycle=%s activation=%s deferred=%s destroyed=%s cleared=%s\n",
                hierarchy ? "pass" : "fail", transform ? "pass" : "fail",
                lifecycle ? "pass" : "fail", hierarchyActivation ? "pass" : "fail",
                deferred ? "pass" : "fail", destroyed ? "pass" : "fail",
                cleared ? "pass" : "fail");
    return hierarchy && transform && lifecycle && hierarchyActivation &&
                   deferred && destroyed && cleared
               ? 0
               : 1;
}
