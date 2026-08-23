#include <k2d/Scene.h>
#include <k2d/GameObject.h>
#include <k2d/Prefab.h>
#include <k2d/FileSystem.h>
#include <k2d/ScriptComponent.h>

#include <cstdio>

namespace
{
    const char *kPrefabPath = "/tmp/claude-1000/-media-projectos-projects-cpp-Kinetix2D/b6ac282d-eb09-4dac-bdb4-a87a643d6e0a/scratchpad/k2d_prefab_test.json";

    // Serializer::WriteObject/ReadObject (what Prefab is built on) only knows
    // the engine's own registered component types -- there is no
    // ComponentType::Script entry, so a user ScriptComponent subclass has no
    // name/factory to round-trip by and is silently dropped if it's part of
    // what gets saved. This bit the first version of prefab_bullet_demo: a
    // BulletScript added to the template never made it into the prefab, so
    // every spawned bullet had no velocity and just sat on top of the gun
    // forever. The fix -- and the pattern these two tests hold in place --
    // is that the prefab is visual/structural only, and any script gets
    // attached fresh after each Instantiate() call.
    class MovingScript : public k2d::ScriptComponent
    {
    public:
        void Fire(const Math::Vec2 &velocity, float lifetime)
        {
            mVelocity = velocity;
            mLifetime = lifetime;
        }

    protected:
        void onUpdate(float deltaTime) override
        {
            owner()->translate(mVelocity * deltaTime);
            mLifetime -= deltaTime;
            if (mLifetime <= 0.0f)
                owner()->dispose();
        }

    private:
        Math::Vec2 mVelocity{0.0f, 0.0f};
        float mLifetime = 0.0f;
    };

    k2d::GameObject *MakeSourceObject(k2d::Scene &scene)
    {
        k2d::GameObject *root = scene.createObject("Enemy");
        root->setPosition(Math::Vec2(3.0f, 4.0f));
        root->setTag("enemy");
        k2d::GameObject *child = scene.createObject("Weapon", root);
        child->setPosition(Math::Vec2(1.0f, 0.0f));
        return root;
    }

    bool TestSaveThenLoadRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = MakeSourceObject(srcScene);

        k2d::Prefab writer;
        bool saved = writer.SaveToFile(kPrefabPath, *root);

        k2d::Prefab reader;
        bool loaded = reader.Load(kPrefabPath);

        return saved && loaded && reader.valid();
    }

    bool TestInstantiateProducesIndependentTrees()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = MakeSourceObject(srcScene);

        k2d::Prefab prefab;
        prefab.SaveFromObject(*root);

        k2d::Scene dstScene;
        k2d::GameObject *a = prefab.Instantiate(dstScene);
        k2d::GameObject *b = prefab.Instantiate(dstScene);
        k2d::GameObject *c = prefab.Instantiate(dstScene);

        bool ok = true;
        ok &= (a != nullptr && b != nullptr && c != nullptr);
        ok &= (a != b && b != c && a != c);
        ok &= (a->id() != b->id() && b->id() != c->id());
        ok &= (a->name() == ct::String("Enemy"));
        ok &= (a->tag() == ct::String("enemy"));
        ok &= (a->childCount() == 1 && b->childCount() == 1 && c->childCount() == 1);
        ok &= (a->child(0) != b->child(0));
        ok &= (a->position().x == 3.0f && a->position().y == 4.0f);

        a->setPosition(Math::Vec2(99.0f, 99.0f));
        ok &= (b->position().x == 3.0f && b->position().y == 4.0f);
        ok &= (c->position().x == 3.0f && c->position().y == 4.0f);

        dstScene.destroy(a);
        ok &= (b->name() == ct::String("Enemy"));
        ok &= (c->name() == ct::String("Enemy"));

        return ok;
    }

    bool TestLoadFromDiskInstantiateDoesNotReread()
    {
        k2d::Prefab prefab;
        if (!prefab.Load(kPrefabPath))
            return false;

        k2d::FileSystem::Instance().SaveTextFile(kPrefabPath, ct::String("{\"corrupted\":true}"));

        k2d::Scene scene;
        k2d::GameObject *obj = prefab.Instantiate(scene);

        return obj != nullptr && obj->name() == ct::String("Enemy");
    }

    bool TestScriptComponentDoesNotSurviveSaveFromObject()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Scripted");
        root->addComponent<MovingScript>();

        k2d::Prefab prefab;
        prefab.SaveFromObject(*root);

        k2d::Scene dstScene;
        k2d::GameObject *instance = prefab.Instantiate(dstScene);

        return instance != nullptr && instance->getComponent<MovingScript>() == nullptr;
    }

    bool TestAttachScriptAfterInstantiateMovesAndExpires()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Bullet");
        root->setPosition(Math::Vec2(0.0f, 0.0f));

        k2d::Prefab prefab;
        prefab.SaveFromObject(*root);

        k2d::Scene scene;
        k2d::GameObject *bullet = prefab.Instantiate(scene);
        if (!bullet)
            return false;

        MovingScript *script = bullet->addComponent<MovingScript>();
        script->Fire(Math::Vec2(100.0f, 0.0f), 0.5f);

        bool ok = true;
        const float dt = 0.1f;
        for (int i = 0; i < 3; ++i)
            scene.update(dt);
        ok &= (bullet->position().x > 25.0f && bullet->position().x < 35.0f);
        ok &= (scene.objectCount() == 1);

        for (int i = 0; i < 5; ++i)
            scene.update(dt);
        ok &= (scene.objectCount() == 0);

        return ok;
    }
}

int main()
{
    bool roundTrip = TestSaveThenLoadRoundTrip();
    bool independent = TestInstantiateProducesIndependentTrees();
    bool cachedNotReread = TestLoadFromDiskInstantiateDoesNotReread();
    bool scriptDropped = TestScriptComponentDoesNotSurviveSaveFromObject();
    bool attachAfter = TestAttachScriptAfterInstantiateMovesAndExpires();

    std::printf("prefab: round_trip=%s independent_instances=%s cached_not_reread=%s "
                "script_not_serialized=%s attach_script_after_instantiate=%s\n",
                roundTrip ? "pass" : "fail",
                independent ? "pass" : "fail",
                cachedNotReread ? "pass" : "fail",
                scriptDropped ? "pass" : "fail",
                attachAfter ? "pass" : "fail");

    bool ok = roundTrip && independent && cachedNotReread && scriptDropped && attachAfter;
    return ok ? 0 : 1;
}
