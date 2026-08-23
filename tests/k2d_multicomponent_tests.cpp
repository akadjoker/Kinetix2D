#include <k2d/Scene.h>
#include <k2d/GameObject.h>
#include <k2d/ScriptComponent.h>
#include <k2d/Light2D.h>
#include <k2d/DirectionalLight2D.h>

#include <cstdio>
#include <vector>

namespace
{
    bool g_ok = true;
    void Check(bool cond, const char *label)
    {
        if (!cond)
        {
            g_ok = false;
            std::printf("  FAIL: %s\n", label);
        }
    }

    class TagScript : public k2d::ScriptComponent
    {
    public:
        explicit TagScript(int tag) : mTag(tag) {}
        int tag() const { return mTag; }
        int updateCount() const { return mUpdateCount; }

    protected:
        void onUpdate(float) override { ++mUpdateCount; }

    private:
        int mTag;
        int mUpdateCount = 0;
    };

    class SelfRemoveScript : public k2d::ScriptComponent
    {
    protected:
        void onUpdate(float) override { owner()->removeComponent(this); }
    };

    class RemoveOtherScript : public k2d::ScriptComponent
    {
    public:
        void setTarget(k2d::Component *target) { mTarget = target; }

    protected:
        void onUpdate(float) override
        {
            if (mTarget)
            {
                owner()->removeComponent(mTarget);
                mTarget = nullptr;
            }
        }

    private:
        k2d::Component *mTarget = nullptr;
    };
}

// TagScript/SelfRemoveScript/RemoveOtherScript all share ComponentType::Script
// (every ScriptComponent subclass does), so distinguishing between them via
// getComponent<T>/componentCount<T> needs a real ComponentMatch<T> the same
// way Light2D/DirectionalLight2D do -- without it, the default
// ComponentMatch<T>::test (always true) would count any Script-type sibling
// as a match for whichever T is asked for.
template <> struct k2d::ComponentMatch<TagScript>
{
    static bool test(const k2d::Component *c) { return dynamic_cast<const TagScript *>(c) != nullptr; }
};
template <> struct k2d::ComponentMatch<SelfRemoveScript>
{
    static bool test(const k2d::Component *c) { return dynamic_cast<const SelfRemoveScript *>(c) != nullptr; }
};
template <> struct k2d::ComponentMatch<RemoveOtherScript>
{
    static bool test(const k2d::Component *c) { return dynamic_cast<const RemoveOtherScript *>(c) != nullptr; }
};

namespace
{

    bool TestMultipleInstancesBasics()
    {
        k2d::Scene scene;
        k2d::GameObject *obj = scene.createObject("multi");

        TagScript *a = obj->addComponent<TagScript>(1);
        TagScript *b = obj->addComponent<TagScript>(2);
        TagScript *c = obj->addComponent<TagScript>(3);

        bool ok = true;
        ok &= (a != nullptr && b != nullptr && c != nullptr);
        ok &= (obj->componentCount<TagScript>() == 3);
        ok &= (obj->getComponent<TagScript>() == a);
        ok &= (obj->getComponentAt<TagScript>(0) == a);
        ok &= (obj->getComponentAt<TagScript>(1) == b);
        ok &= (obj->getComponentAt<TagScript>(2) == c);
        ok &= (obj->getComponentAt<TagScript>(3) == nullptr);
        ok &= (obj->getComponent<TagScript>(a->id()) == a);
        ok &= (obj->getComponent<TagScript>(b->id()) == b);
        ok &= (a->id() != b->id() && b->id() != c->id() && a->id() != c->id());

        obj->removeComponent(b);
        ok &= (obj->componentCount<TagScript>() == 2);
        ok &= (obj->getComponentAt<TagScript>(0) == a);
        ok &= (obj->getComponentAt<TagScript>(1) == c);
        ok &= (obj->getComponent<TagScript>(b->id()) == nullptr);

        return ok;
    }

    bool TestSelfRemovalDuringUpdate()
    {
        k2d::Scene scene;
        k2d::GameObject *obj = scene.createObject("selfremove");

        obj->addComponent<SelfRemoveScript>();
        obj->addComponent<TagScript>(99);

        scene.update(0.016f);

        bool ok = true;
        ok &= (obj->componentCount<SelfRemoveScript>() == 0);
        ok &= (obj->componentCount<TagScript>() == 1);
        ok &= (obj->getComponent<TagScript>()->updateCount() == 1);

        scene.update(0.016f);
        ok &= (obj->getComponent<TagScript>()->updateCount() == 2);

        return ok;
    }

    bool TestSiblingRemovalDuringUpdate()
    {
        k2d::Scene scene;
        k2d::GameObject *obj = scene.createObject("siblingremove");

        RemoveOtherScript *remover = obj->addComponent<RemoveOtherScript>();
        TagScript *victim = obj->addComponent<TagScript>(1);
        TagScript *survivor = obj->addComponent<TagScript>(2);
        remover->setTarget(victim);

        scene.update(0.016f);

        bool ok = true;
        ok &= (obj->componentCount<TagScript>() == 1);
        ok &= (obj->getComponent<TagScript>() == survivor);
        ok &= (survivor->updateCount() == 1);

        scene.update(0.016f);
        ok &= (survivor->updateCount() == 2);

        return ok;
    }

    bool TestLightComponentMatchDisambiguation()
    {
        k2d::Scene scene;
        k2d::GameObject *obj = scene.createObject("light");

        obj->addComponent<k2d::DirectionalLight2D>();

        bool ok = true;
        ok &= (obj->getComponent<k2d::DirectionalLight2D>() != nullptr);
        ok &= (obj->getComponent<k2d::Light2D>() == nullptr);

        k2d::GameObject *obj2 = scene.createObject("light2");
        obj2->addComponent<k2d::Light2D>();
        ok &= (obj2->getComponent<k2d::Light2D>() != nullptr);
        ok &= (obj2->getComponent<k2d::DirectionalLight2D>() == nullptr);

        return ok;
    }

    bool TestBackwardCompatSingleAdd()
    {
        k2d::Scene scene;
        k2d::GameObject *obj = scene.createObject("single");

        TagScript *a = obj->addComponent<TagScript>(1);
        TagScript *b = obj->addComponent<TagScript>(2);

        bool ok = true;
        ok &= (a != nullptr && b != nullptr && a != b);
        ok &= (obj->getComponent<TagScript>() == a);
        obj->removeComponent<TagScript>();
        ok &= (obj->getComponent<TagScript>() == b);
        ok &= (obj->componentCount<TagScript>() == 1);

        return ok;
    }
}

int main()
{
    bool basics = TestMultipleInstancesBasics();
    bool selfRemove = TestSelfRemovalDuringUpdate();
    bool siblingRemove = TestSiblingRemovalDuringUpdate();
    bool lightMatch = TestLightComponentMatchDisambiguation();
    bool backwardCompat = TestBackwardCompatSingleAdd();

    std::printf("multicomponent: basics=%s self_remove=%s sibling_remove=%s "
                "light_match=%s backward_compat=%s\n",
                basics ? "pass" : "fail", selfRemove ? "pass" : "fail",
                siblingRemove ? "pass" : "fail", lightMatch ? "pass" : "fail",
                backwardCompat ? "pass" : "fail");

    bool allOk = g_ok && basics && selfRemove && siblingRemove && lightMatch && backwardCompat;
    return allOk ? 0 : 1;
}
