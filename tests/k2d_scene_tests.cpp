#include <k2d/Scene.h>
#include <k2d/ScriptComponent.h>

#include <cstdio>
#include <cmath>

namespace
{
    int gUpdates = 0;
    int gLateUpdates = 0;
    int gSpawnedUpdates = 0;

    class CounterScript : public k2d::ScriptComponent
    {
    protected:
        void onUpdate(float deltaTime) override
        {
            ++gUpdates;
            owner()->translate(Math::Vec2(deltaTime, 0.0f));
        }

        void onLateUpdate(float) override
        {
            ++gLateUpdates;
        }
    };

    class SpawnedCounterScript : public k2d::ScriptComponent
    {
    protected:
        void onUpdate(float) override { ++gSpawnedUpdates; }
    };

    class SpawnChildrenScript : public k2d::ScriptComponent
    {
    protected:
        void onUpdate(float) override
        {
            if (mSpawned)
                return;
            mSpawned = true;
            for (int i = 0; i < 100; ++i)
            {
                k2d::GameObject *child = owner()->scene()->createObject("spawned", owner());
                if (child)
                    child->addComponent<SpawnedCounterScript>();
            }
        }

    private:
        bool mSpawned = false;
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
    child->setPosition(Math::Vec2(10.0f, 5.0f));
    parent->setPosition(Math::Vec2(20.0f, 30.0f));
    parent->setRotationDegrees(90.0f);
    child->addComponent<CounterScript>();

    bool hierarchy = scene.objectCount() == 2 && scene.find("child") == child &&
                     child->parent() == parent;
    Math::Vec2 global = child->globalPosition();
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

    k2d::GameObject *first = scene.createObject("first", parent);
    k2d::GameObject *second = scene.createObject("second", parent);
    k2d::GameObject *third = scene.createObject("third", parent);
    bool reorderInitial = parent->childIndex(first) == 0 && parent->childIndex(second) == 1 &&
                          parent->childIndex(third) == 2;

    bool moveUpFailFirst = !parent->moveChildUp(first);
    bool moveUpOk = parent->moveChildUp(second);
    bool reorderedAfterUp = parent->childIndex(second) == 0 && parent->childIndex(first) == 1 &&
                            parent->childIndex(third) == 2;

    bool moveDownFailLast = !parent->moveChildDown(third);
    bool moveDownOk = parent->moveChildDown(second);
    bool reorderedAfterDown = parent->childIndex(first) == 0 && parent->childIndex(second) == 1 &&
                              parent->childIndex(third) == 2;
    bool reorder = reorderInitial && moveUpFailFirst && moveUpOk && reorderedAfterUp &&
                   moveDownFailLast && moveDownOk && reorderedAfterDown;

    bool reparentOk = scene.reparent(third, first);
    bool reparented = reparentOk && third->parent() == first && first->childCount() == 1 &&
                      parent->childCount() == 2;

    bool reparentCycleRejected = !scene.reparent(first, third) && first->parent() == parent;
    bool reparentSelfRejected = !scene.reparent(first, first);
    bool reparentToRoot = scene.reparent(third, nullptr);
    bool reparentedToRoot = reparentToRoot && third->parent() == &scene.root() &&
                            first->childCount() == 0;
    bool reparent = reparented && reparentCycleRejected && reparentSelfRejected && reparentedToRoot;

    k2d::Scene spawnScene;
    k2d::GameObject *spawner = spawnScene.createObject("spawner");
    spawner->addComponent<SpawnChildrenScript>();
    gSpawnedUpdates = 0;
    spawnScene.update(0.0f);
    const bool spawnSnapshot = spawnScene.objectCount() == 101 && gSpawnedUpdates == 0;
    spawnScene.update(0.0f);
    const bool spawnedNextFrame = gSpawnedUpdates == 100;

    scene.clear();
    bool cleared = scene.objectCount() == 0 && scene.root().childCount() == 0;

    std::printf("scene: hierarchy=%s transform=%s lifecycle=%s activation=%s deferred=%s destroyed=%s "
               "reorder=%s reparent=%s spawn_snapshot=%s spawn_next_frame=%s cleared=%s\n",
                hierarchy ? "pass" : "fail", transform ? "pass" : "fail",
                lifecycle ? "pass" : "fail", hierarchyActivation ? "pass" : "fail",
                deferred ? "pass" : "fail", destroyed ? "pass" : "fail",
                reorder ? "pass" : "fail", reparent ? "pass" : "fail",
                spawnSnapshot ? "pass" : "fail", spawnedNextFrame ? "pass" : "fail",
                cleared ? "pass" : "fail");
    return hierarchy && transform && lifecycle && hierarchyActivation &&
                   deferred && destroyed && reorder && reparent && spawnSnapshot && spawnedNextFrame && cleared
               ? 0
               : 1;
}
