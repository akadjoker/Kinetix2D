
#include <k2d/k2d.h>

#include <cstdio>
#include <cmath>

namespace
{
    bool Near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }
    bool NearVec2(const Math::Vec2 &a, const Math::Vec2 &b) { return Near(a.x, b.x) && Near(a.y, b.y); }
    bool NearVec4(const Math::Vec4 &a, const Math::Vec4 &b)
    {
        return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z) && Near(a.w, b.w);
    }
    bool NearColor(const k2d::Color &a, const k2d::Color &b)
    {
        return Near(a.r, b.r) && Near(a.g, b.g) && Near(a.b, b.b) && Near(a.a, b.a);
    }

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

            ok = ok && NearColor(copySprite->material().color(), rootSprite->material().color());
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

    class DummyScript : public k2d::ScriptComponent
    {
    public:
        DummyScript() : k2d::ScriptComponent() {}
    };

    bool TestUnregisteredTypeIsSkipped()
    {
        if (k2d::Serializer::IsRegistered(k2d::ComponentType::Script))
            return false; 

        k2d::Scene scene;
        k2d::GameObject *root = scene.createObject("Mixed");
        root->addComponent<k2d::SpriteComponent>();
        root->addComponent<DummyScript>();

        ct::Json written = k2d::Serializer::WriteObject(*root);
        const ct::Json &components = written["components"];
        return k2d::Serializer::IsRegistered(k2d::ComponentType::Sprite) &&
               components.is_array() && components.size() == 1;
    }

    bool TestTileMapRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Map");
        k2d::TileMapComponent *map = root->addComponent<k2d::TileMapComponent>();
        map->setCellSize(16.0f, 24.0f);
        map->setMapSize(3, 2);
        map->setAtlasTilesX(5);
        map->setTile(0, 0, 3);
        map->setTile(2, 1, 7);
        map->setCullRect(10.0f, 20.0f, 100.0f, 80.0f);
        map->setBlendMode(k2d::BLEND_ADD);

        ct::Json written = k2d::Serializer::WriteObject(*root);
        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written);
        k2d::TileMapComponent *copyMap = copy ? copy->getComponent<k2d::TileMapComponent>() : nullptr;
        if (!copyMap)
            return false;

        bool ok = Near(copyMap->cellWidth(), 16.0f) && Near(copyMap->cellHeight(), 24.0f);
        ok = ok && copyMap->columns() == 3 && copyMap->rows() == 2;
        ok = ok && copyMap->atlasTilesX() == 5;
        ok = ok && copyMap->getTile(0, 0) == 3 && copyMap->getTile(2, 1) == 7 &&
             copyMap->getTile(1, 0) == 0; 
        ok = ok && copyMap->hasCullRect();
        ok = ok && NearVec4(copyMap->cullRect(), {10.0f, 20.0f, 100.0f, 80.0f});
        ok = ok && copyMap->blendMode() == k2d::BLEND_ADD;
        return ok;
    }

    bool TestPolygonAndLineRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Shapes");

        k2d::Polygon2D *polygon = root->addComponent<k2d::Polygon2D>();
        Math::Vec2 triangle[3] = {{0.0f, 0.0f}, {10.0f, 0.0f}, {5.0f, 10.0f}};
        polygon->setPolygon(triangle, 3);
        polygon->setColor(11, 22, 33, 44);
        polygon->setBlendMode(k2d::BLEND_MUL);

        k2d::GameObject *lineObj = srcScene.createObject("Line", root);
        k2d::Line2D *line = lineObj->addComponent<k2d::Line2D>();
        Math::Vec2 pts[3] = {{0.0f, 0.0f}, {5.0f, 5.0f}, {10.0f, 0.0f}};
        line->setPoints(pts, 3);
        line->setWidth(6.0f);
        line->setClosed(true);
        line->setColor(200, 150, 100, 255);
        line->setBlendMode(k2d::BLEND_ADD);

        ct::Json written = k2d::Serializer::WriteObject(*root);
        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written);
        if (!copy)
            return false;

        k2d::Polygon2D *copyPolygon = copy->getComponent<k2d::Polygon2D>();
        bool ok = copyPolygon != nullptr;
        if (copyPolygon)
        {
            ok = ok && copyPolygon->polygon().size() == 3 &&
                 NearVec2(copyPolygon->polygon()[2], {5.0f, 10.0f});
            ok = ok && copyPolygon->color() == polygon->color();
            ok = ok && copyPolygon->blendMode() == k2d::BLEND_MUL;
        }

        ok = ok && copy->childCount() == 1;
        if (copy->childCount() == 1)
        {
            k2d::Line2D *copyLine = copy->child(0)->getComponent<k2d::Line2D>();
            ok = ok && copyLine != nullptr;
            if (copyLine)
            {
                ok = ok && copyLine->points().size() == 3 && copyLine->closed() == true &&
                     Near(copyLine->width(), 6.0f) && copyLine->color() == line->color() &&
                     copyLine->blendMode() == k2d::BLEND_ADD;
            }
        }
        return ok;
    }

    bool TestPrimitiveShapeRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("PrimitiveShapes");
        k2d::CircleShape *circle = root->addComponent<k2d::CircleShape>();
        circle->setRadius(26.0f);
        circle->setSegments(18);
        circle->setMode(k2d::ShapeRenderMode::Line);
        circle->setLineWidth(3.5f);
        circle->setColor(10, 20, 30, 40);
        circle->setBlendMode(k2d::BLEND_ADD);

        k2d::RectShape *rect = root->addComponent<k2d::RectShape>();
        rect->setSize({90.0f, 40.0f});
        rect->setMode(k2d::ShapeRenderMode::Line);
        rect->setLineWidth(5.0f);
        rect->setColor(90, 80, 70, 60);
        rect->setBlendMode(k2d::BLEND_MUL);

        ct::Json written = k2d::Serializer::WriteObject(*root);
        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written);
        k2d::CircleShape *copyCircle = copy ? copy->getComponent<k2d::CircleShape>() : nullptr;
        k2d::RectShape *copyRect = copy ? copy->getComponent<k2d::RectShape>() : nullptr;
        if (!copyCircle || !copyRect)
            return false;

        return Near(copyCircle->radius(), 26.0f) && copyCircle->segments() == 18 &&
               copyCircle->mode() == k2d::ShapeRenderMode::Line &&
               Near(copyCircle->lineWidth(), 3.5f) && copyCircle->color() == circle->color() &&
               copyCircle->blendMode() == k2d::BLEND_ADD && NearVec2(copyRect->size(), {90.0f, 40.0f}) &&
               copyRect->mode() == k2d::ShapeRenderMode::Line && Near(copyRect->lineWidth(), 5.0f) &&
               copyRect->color() == rect->color() && copyRect->blendMode() == k2d::BLEND_MUL;
    }

    bool TestNinePatchRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Panel");
        k2d::NinePatchComponent *patch = root->addComponent<k2d::NinePatchComponent>();
        patch->setSize({120.0f, 80.0f});
        patch->setMargins(4.0f, 6.0f, 8.0f, 10.0f);
        patch->setPivot({0.5f, 0.5f});
        patch->setColor(9, 8, 7, 255);
        patch->setBlendMode(k2d::BLEND_SUB);

        ct::Json written = k2d::Serializer::WriteObject(*root);
        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written);
        k2d::NinePatchComponent *copyPatch = copy ? copy->getComponent<k2d::NinePatchComponent>() : nullptr;
        if (!copyPatch)
            return false;

        return NearVec2(copyPatch->size(), {120.0f, 80.0f}) &&
               NearVec4(copyPatch->margins(), {4.0f, 6.0f, 8.0f, 10.0f}) &&
               NearVec2(copyPatch->pivot(), {0.5f, 0.5f}) && copyPatch->color() == patch->color() &&
               copyPatch->blendMode() == k2d::BLEND_SUB;
    }

    bool TestSpriteBatchRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Batch");
        k2d::SpriteBatch *batch = root->addComponent<k2d::SpriteBatch>();
        int idx = batch->add(nullptr, {1.0f, 2.0f}, {30.0f, 40.0f}, 0xAABBCCDDu);
        batch->setSource(idx, {0.0f, 0.0f, 16.0f, 16.0f});
        batch->setFlip(idx, true, false);
        batch->add(nullptr, {5.0f, 6.0f}, {10.0f, 10.0f});
        batch->setBlendMode(k2d::BLEND_ADD);

        ct::Json written = k2d::Serializer::WriteObject(*root);
        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written);
        k2d::SpriteBatch *copyBatch = copy ? copy->getComponent<k2d::SpriteBatch>() : nullptr;
        if (!copyBatch)
            return false;

        if (copyBatch->count() != 2)
            return false;
        const k2d::SpriteBatch::Entry *e0 = copyBatch->entry(0);
        return e0 && NearVec2(e0->position, {1.0f, 2.0f}) && NearVec2(e0->size, {30.0f, 40.0f}) &&
               NearVec4(e0->source, {0.0f, 0.0f, 16.0f, 16.0f}) && e0->color.Packed() == 0xAABBCCDDu &&
               (e0->flags & 0x1u) != 0 && (e0->flags & 0x2u) == 0 &&
               copyBatch->blendMode() == k2d::BLEND_ADD;
    }

    bool TestAnimationRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Anim");
        k2d::Animation2D *anim = root->addComponent<k2d::Animation2D>();
        anim->addClip("idle", nullptr, 16, 16, 4, 6.0f, k2d::AnimationMode::Loop);
        anim->addClip("run", nullptr, 20, 16, 8, 12.0f, k2d::AnimationMode::PingPong);
        anim->play("run");

        ct::Json written = k2d::Serializer::WriteObject(*root);
        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written);
        k2d::Animation2D *copyAnim = copy ? copy->getComponent<k2d::Animation2D>() : nullptr;
        if (!copyAnim)
            return false;

        bool ok = copyAnim->clipCount() == 2;
        ok = ok && ct::String(copyAnim->currentClip()) == ct::String("run");
        ok = ok && copyAnim->frameCount() == 8 && Near(copyAnim->framesPerSecond(), 12.0f) &&
             copyAnim->mode() == k2d::AnimationMode::PingPong;
        ok = ok && copyAnim->playing() == anim->playing();
        return ok;
    }

    bool TestLightDisambiguation()
    {
        k2d::Scene srcScene;
        k2d::GameObject *pointObj = srcScene.createObject("PointLight");
        k2d::Light2D *point = pointObj->addComponent<k2d::Light2D>();
        point->setColor(1.0f, 0.5f, 0.25f, 1.0f);
        point->setEnergy(1.8f);
        point->setRadius(222.0f);
        point->setCastShadow(true);
        point->setCullMask(3u);
        point->setHeight(12.0f);

        k2d::GameObject *dirObj = srcScene.createObject("DirLight");
        k2d::DirectionalLight2D *dir = dirObj->addComponent<k2d::DirectionalLight2D>();
        dir->setColor(0.2f, 0.3f, 0.9f, 1.0f);
        dir->setEnergy(0.6f);
        dir->setCastShadow(false);

        k2d::Scene dstScene;
        ct::Json pointJson = k2d::Serializer::WriteObject(*pointObj);
        ct::Json dirJson = k2d::Serializer::WriteObject(*dirObj);

        k2d::GameObject *copyPointObj = k2d::Serializer::ReadObject(dstScene, pointJson);
        k2d::GameObject *copyDirObj = k2d::Serializer::ReadObject(dstScene, dirJson);

        k2d::Light2D *copyPoint = copyPointObj ? copyPointObj->getComponent<k2d::Light2D>() : nullptr;
        k2d::DirectionalLight2D *copyDir = copyDirObj ? copyDirObj->getComponent<k2d::DirectionalLight2D>() : nullptr;
        if (!copyPoint || !copyDir)
            return false;

        bool ok = NearColor(copyPoint->color(), {1.0f, 0.5f, 0.25f, 1.0f});
        ok = ok && Near(copyPoint->energy(), 1.8f) && Near(copyPoint->radius(), 222.0f);
        ok = ok && copyPoint->castShadow() == true && copyPoint->cullMask() == 3u &&
             Near(copyPoint->height(), 12.0f);
        ok = ok && NearColor(copyDir->color(), {0.2f, 0.3f, 0.9f, 1.0f});
        ok = ok && Near(copyDir->energy(), 0.6f) && copyDir->castShadow() == false;

        ok = ok && dynamic_cast<k2d::DirectionalLight2D *>(copyPoint) == nullptr;
        ok = ok && dynamic_cast<k2d::Light2D *>(copyDir) == nullptr;
        return ok;
    }

    bool TestOccluderRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Wall");
        k2d::LightOccluder2D *occluder = root->addComponent<k2d::LightOccluder2D>();
        Math::Vec2 pts[4] = {{0.0f, 0.0f}, {40.0f, 0.0f}, {40.0f, 10.0f}, {0.0f, 10.0f}};
        occluder->setPolygon(pts, 4);

        ct::Json written = k2d::Serializer::WriteObject(*root);
        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written);
        k2d::LightOccluder2D *copyOccluder = copy ? copy->getComponent<k2d::LightOccluder2D>() : nullptr;
        return copyOccluder && copyOccluder->points().size() == 4 &&
               NearVec2(copyOccluder->points()[2], {40.0f, 10.0f});
    }

    bool TestCameraRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("Cam");
        k2d::CameraComponent *cam = root->addComponent<k2d::CameraComponent>();
        cam->setViewport(1280.0f, 720.0f);
        cam->camera().position = Math::Vec2(100.0f, 50.0f);
        cam->camera().rotationDegrees = 15.0f;
        cam->camera().zoom = Math::Vec2(2.0f, 2.0f);
        cam->camera().setLimits(-500.0f, -300.0f, 500.0f, 300.0f);
        cam->camera().setSmoothing(true, 8.0f);
        cam->camera().setDeadZonePixels(50.0f, 40.0f, 200.0f, 160.0f);
        cam->camera().setTarget(Math::Vec2(20.0f, -20.0f));

        ct::Json written = k2d::Serializer::WriteObject(*root);
        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written);
        k2d::CameraComponent *copyCam = copy ? copy->getComponent<k2d::CameraComponent>() : nullptr;
        if (!copyCam)
            return false;

        bool ok = Near(copyCam->viewportWidth(), 1280.0f) && Near(copyCam->viewportHeight(), 720.0f);
        const k2d::Camera2D &c = copyCam->camera();
        ok = ok && NearVec2(c.position, {100.0f, 50.0f}) && Near(c.rotationDegrees, 15.0f);
        ok = ok && NearVec2(c.zoom, {2.0f, 2.0f});
        ok = ok && c.limitEnabled && NearVec4(c.limits, {-500.0f, -300.0f, 500.0f, 300.0f});
        ok = ok && c.smoothingEnabled && Near(c.smoothingSpeed, 8.0f);
        ok = ok && c.deadZoneEnabled && NearVec4(c.deadZone, {50.0f, 40.0f, 200.0f, 160.0f});
        ok = ok && c.targetEnabled && NearVec2(c.target, {20.0f, -20.0f});
        return ok;
    }

    bool TestParticleComponentRoundTrip()
    {
        k2d::Scene srcScene;
        k2d::GameObject *root = srcScene.createObject("FX");
        k2d::ParticleComponent *pc = root->addComponent<k2d::ParticleComponent>(64);
        k2d::ParticleSystem &sys = pc->system();
        sys.SetGravity({0.0f, 30.0f});
        sys.SetMode(k2d::ParticleMode::Loop);
        sys.SetEmitterShape(k2d::ParticleEmitterShape::Circle);
        sys.SetEmitterRadius(14.0f);
        sys.SetEmissionRate(20.0f);
        sys.SetOneShotCount(5);
        pc->setBlendMode(k2d::BLEND_ADD);
        pc->setYSort(true);
        pc->setFollowOwner(false);

        k2d::ParticlePrefab prefab;
        prefab.direction = Math::Vec2(1.0f, 0.0f);
        prefab.spreadDegrees = 30.0f;
        prefab.speedMin = 10.0f;
        prefab.speedMax = 25.0f;
        prefab.lifeMin = 0.4f;
        prefab.lifeMax = 1.2f;
        prefab.sizeMin = 2.0f;
        prefab.sizeMax = 6.0f;
        prefab.endSize = 1.0f;
        prefab.rotationMin = -10.0f;
        prefab.rotationMax = 10.0f;
        prefab.angularVelocityMin = 5.0f;
        prefab.angularVelocityMax = 15.0f;
        prefab.drag = 0.5f;
        prefab.faceDirection = true;
        prefab.faceDirectionOffsetDegrees = 90.0f;
        prefab.fadeIn = 0.1f;
        prefab.fadeOut = 0.2f;
        prefab.colorStart = k2d::Color(1.0f, 1.0f, 0.0f, 1.0f);
        prefab.colorEnd = k2d::Color(1.0f, 0.0f, 0.0f, 0.0f);
        prefab.atlasBounds = Math::Vec4(0.0f, 0.0f, 8.0f, 8.0f);
        sys.SetPrefab(prefab);
        sys.Start();

        ct::Json written = k2d::Serializer::WriteObject(*root);
        k2d::Scene dstScene;
        k2d::GameObject *copy = k2d::Serializer::ReadObject(dstScene, written);
        k2d::ParticleComponent *copyPc = copy ? copy->getComponent<k2d::ParticleComponent>() : nullptr;
        if (!copyPc)
            return false;
        const k2d::ParticleSystem &copySys = copyPc->system();

        bool ok = copySys.Capacity() == 64;
        ok = ok && copySys.GetMode() == k2d::ParticleMode::Loop;
        ok = ok && copySys.GetEmitterShape() == k2d::ParticleEmitterShape::Circle;
        ok = ok && Near(copySys.EmitterSize().x, 14.0f);
        ok = ok && Near(copySys.EmissionRate(), 20.0f);
        ok = ok && copySys.OneShotCount() == 5;
        ok = ok && NearVec2(copySys.Gravity(), {0.0f, 30.0f});
        ok = ok && copySys.IsPlaying() == sys.IsPlaying();
        ok = ok && copyPc->blendMode() == k2d::BLEND_ADD && copyPc->ySort() == true &&
             copyPc->followOwner() == false;

        const k2d::ParticlePrefab &p = copySys.GetPrefab();
        ok = ok && NearVec2(p.direction, {1.0f, 0.0f}) && Near(p.spreadDegrees, 30.0f);
        ok = ok && Near(p.speedMin, 10.0f) && Near(p.speedMax, 25.0f);
        ok = ok && Near(p.lifeMin, 0.4f) && Near(p.lifeMax, 1.2f);
        ok = ok && Near(p.sizeMin, 2.0f) && Near(p.sizeMax, 6.0f) && Near(p.endSize, 1.0f);
        ok = ok && Near(p.rotationMin, -10.0f) && Near(p.rotationMax, 10.0f);
        ok = ok && Near(p.angularVelocityMin, 5.0f) && Near(p.angularVelocityMax, 15.0f);
        ok = ok && Near(p.drag, 0.5f) && p.faceDirection == true && Near(p.faceDirectionOffsetDegrees, 90.0f);
        ok = ok && Near(p.fadeIn, 0.1f) && Near(p.fadeOut, 0.2f);
        ok = ok && NearColor(p.colorStart, {1.0f, 1.0f, 0.0f, 1.0f});
        ok = ok && NearColor(p.colorEnd, {1.0f, 0.0f, 0.0f, 0.0f});
        ok = ok && NearVec4(p.atlasBounds, {0.0f, 0.0f, 8.0f, 8.0f});
        return ok;
    }
}

int main()
{
    bool roundTrip = TestRoundTripThroughText();
    bool defaults = TestDefaultsRoundTrip();
    bool unregistered = TestUnregisteredTypeIsSkipped();
    bool tileMap = TestTileMapRoundTrip();
    bool polygonLine = TestPolygonAndLineRoundTrip();
    bool primitiveShapes = TestPrimitiveShapeRoundTrip();
    bool ninePatch = TestNinePatchRoundTrip();
    bool spriteBatch = TestSpriteBatchRoundTrip();
    bool animation = TestAnimationRoundTrip();
    bool lightDisambiguation = TestLightDisambiguation();
    bool occluder = TestOccluderRoundTrip();
    bool camera = TestCameraRoundTrip();
    bool particle = TestParticleComponentRoundTrip();

    std::printf("Serializer: round_trip=%s defaults=%s unregistered_skipped=%s\n",
                roundTrip ? "pass" : "fail", defaults ? "pass" : "fail",
                unregistered ? "pass" : "fail");
    std::printf("Serializer components: tilemap=%s polygon_line=%s primitive_shapes=%s ninepatch=%s spritebatch=%s "
                "animation=%s light_disambiguation=%s occluder=%s camera=%s particle=%s\n",
                tileMap ? "pass" : "fail", polygonLine ? "pass" : "fail", primitiveShapes ? "pass" : "fail", ninePatch ? "pass" : "fail",
                spriteBatch ? "pass" : "fail", animation ? "pass" : "fail",
                lightDisambiguation ? "pass" : "fail", occluder ? "pass" : "fail",
                camera ? "pass" : "fail", particle ? "pass" : "fail");

    return (roundTrip && defaults && unregistered && tileMap && polygonLine && primitiveShapes && ninePatch &&
            spriteBatch && animation && lightDisambiguation && occluder && camera && particle)
               ? 0
               : 1;
}
