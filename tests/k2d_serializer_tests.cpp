// Headless coverage for k2d::Serializer: the GameObject-tree <-> ct::Json
// walker plus the SpriteComponent row in its factory table. Runs without a
// GL context, so texture references are exercised with assets=nullptr here
// (a real GL texture round-trip is covered separately by serializer_diag,
// which needs a live Device).
#include <k2d/k2d.h>

#include <cstdio>
#include <cmath>

namespace
{
    bool Near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }
    bool NearVec2(const glm::vec2 &a, const glm::vec2 &b) { return Near(a.x, b.x) && Near(a.y, b.y); }
    bool NearVec4(const glm::vec4 &a, const glm::vec4 &b)
    {
        return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z) && Near(a.w, b.w);
    }

    // Builds a small tree (root sprite + one child sprite) with every
    // serializable field set to a non-default value, round-trips it through
    // Serializer -> ct::Json text -> Serializer, and checks the copy matches.
    bool TestRoundTripThroughText()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Root");
        root->setTag("enemy");
        root->setActive(false);
        root->setVisible(false);
        root->setZIndex(7);
        root->setPosition({12.5f, -30.0f});
        root->setRotationDegrees(45.0f);
        root->setScale({2.0f, 0.5f});

        k2d::SpriteComponent *rootSprite = root->addComponent<k2d::SpriteComponent>();
        rootSprite->setSize({64.0f, 48.0f});
        rootSprite->setPivot({0.25f, 0.75f});
        rootSprite->setTiling(3.0f, 2.0f);
        rootSprite->setColor(200, 100, 50, 220);
        rootSprite->setSourceRect(4.0f, 8.0f, 16.0f, 16.0f);
        rootSprite->setLightMask(0b0110u);
        rootSprite->setFlip(true, false);
        rootSprite->setYSort(true);
        rootSprite->setBlendMode(k2d::BLEND_ADD);

        k2d::GameObject *child = srcScene.createObject("Child", root);
        child->setPosition({5.0f, 5.0f});
        k2d::SpriteComponent *childSprite = child->addComponent<k2d::SpriteComponent>();
        childSprite->setSize({8.0f, 8.0f});

        ct::Json written = k2d::Serializer::WriteObject(*root);
        ct::String text = written.dump();

        ct::Json::Error err;
        ct::Json reparsed = ct::Json::parse(text, &err);
        if (err)
        {
            std::printf("json parse error: %s\n", err.message);
            return false;
        }

        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, reparsed);
        if (!copy)
            return false;

        bool ok = true;
        ok = ok && ct::String(copy->name()) == ct::String("Root");
        ok = ok && ct::String(copy->tag()) == ct::String("enemy");
        ok = ok && copy->active() == false;
        ok = ok && copy->visible() == false;
        ok = ok && copy->zIndex() == 7;
        ok = ok && NearVec2(copy->position(), {12.5f, -30.0f});
        ok = ok && Near(copy->rotationDegrees(), 45.0f);
        ok = ok && NearVec2(copy->scale(), {2.0f, 0.5f});

        k2d::SpriteComponent *copySprite = copy->getComponent<k2d::SpriteComponent>();
        ok = ok && copySprite != nullptr;
        if (copySprite)
        {
            ok = ok && NearVec2(copySprite->size(), {64.0f, 48.0f});
            ok = ok && NearVec2(copySprite->pivot(), {0.25f, 0.75f});
            ok = ok && NearVec2(copySprite->tiling(), {3.0f, 2.0f});
            // Material2D::color() is normalized (0..1); compare against the
            // source sprite's own normalized value, not the raw 0..255 bytes.
            ok = ok && NearVec4(copySprite->material().color(), rootSprite->material().color());
            ok = ok && copySprite->material().hasSourceRect();
            ok = ok && NearVec4(copySprite->material().sourceRect(), {4.0f, 8.0f, 16.0f, 16.0f});
            ok = ok && copySprite->lightMask() == 0b0110u;
            ok = ok && copySprite->flipX() == true && copySprite->flipY() == false;
            ok = ok && copySprite->ySort() == true;
            ok = ok && copySprite->blendMode() == k2d::BLEND_ADD;
        }

        ok = ok && copy->childCount() == 1;
        if (copy->childCount() == 1)
        {
            k2d::GameObject *copyChild = copy->child(0);
            ok = ok && ct::String(copyChild->name()) == ct::String("Child");
            ok = ok && NearVec2(copyChild->position(), {5.0f, 5.0f});
            k2d::SpriteComponent *copyChildSprite = copyChild->getComponent<k2d::SpriteComponent>();
            ok = ok && copyChildSprite != nullptr;
            ok = ok && copyChildSprite && NearVec2(copyChildSprite->size(), {8.0f, 8.0f});
        }

        return ok;
    }

    // Defaults (a bare object with no components/children) must round-trip
    // to sane values, not garbage from an all-zero glm::vec2 default -- scale
    // in particular must come back (1,1), not (0,0).
    bool TestDefaultsRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Bare");

        ct::Json written = k2d::Serializer::WriteObject(*root);

        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written);
        if (!copy)
            return false;

        return copy->active() && copy->visible() && NearVec2(copy->scale(), {1.0f, 1.0f}) &&
               copy->childCount() == 0 && copy->getComponent<k2d::SpriteComponent>() == nullptr;
    }

    // An attached-but-unregistered component (TileMapComponent isn't in the
    // factory table yet) must be silently skipped on write, not crash and
    // not leak an empty/garbage entry into the components array.
    bool TestUnregisteredTypeIsSkipped()
    {
        if (k2d::Serializer::IsRegistered(k2d::ComponentType::TileMap))
            return false; // this test needs an actually-unregistered type

        k2d::Scene scene;
        k2d::GameObject *root = scene.createObject("Mixed");
        root->addComponent<k2d::SpriteComponent>();
        root->addComponent<k2d::TileMapComponent>();

        ct::Json written = k2d::Serializer::WriteObject(*root);
        const ct::Json &components = written["components"];
        return k2d::Serializer::IsRegistered(k2d::ComponentType::Sprite) &&
               components.is_array() && components.size() == 1;
    }
}

int main()
{
    bool roundTrip = TestRoundTripThroughText();
    bool defaults = TestDefaultsRoundTrip();
    bool unregistered = TestUnregisteredTypeIsSkipped();

    std::printf("Serializer: round_trip=%s defaults=%s unregistered_skipped=%s\n",
                roundTrip ? "pass" : "fail", defaults ? "pass" : "fail",
                unregistered ? "pass" : "fail");

    return (roundTrip && defaults && unregistered) ? 0 : 1;
}
