#include <k2d/Scene.h>
#include <k2d/GameObject.h>
#include <k2d/Prefab.h>
#include <k2d/FileSystem.h>

#include <cstdio>

namespace
{
    const char *kPrefabPath = "/tmp/claude-1000/-media-projectos-projects-cpp-Kinetix2D/b6ac282d-eb09-4dac-bdb4-a87a643d6e0a/scratchpad/k2d_prefab_test.json";

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
}

int main()
{
    bool roundTrip = TestSaveThenLoadRoundTrip();
    bool independent = TestInstantiateProducesIndependentTrees();
    bool cachedNotReread = TestLoadFromDiskInstantiateDoesNotReread();

    std::printf("prefab: round_trip=%s independent_instances=%s cached_not_reread=%s\n",
                roundTrip ? "pass" : "fail",
                independent ? "pass" : "fail",
                cachedNotReread ? "pass" : "fail");

    bool ok = roundTrip && independent && cachedNotReread;
    return ok ? 0 : 1;
}
