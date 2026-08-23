#include "k2d/Serializer.h"

#include "k2d/GameObject.h"
#include "k2d/Scene.h"
#include "k2d/Assets.h"
#include "k2d/SpriteComponent.h"
#include "k2d/TileMapComponent.h"
#include "k2d/Polygon2D.h"
#include "k2d/Line2D.h"
#include "k2d/NinePatchComponent.h"
#include "k2d/SpriteBatch.h"
#include "k2d/Animation2D.h"
#include "k2d/Light2D.h"
#include "k2d/DirectionalLight2D.h"
#include "k2d/LightOccluder2D.h"
#include "k2d/CameraComponent.h"
#include "k2d/ParticleComponent.h"

#include <cmath>
#include <cstring>

namespace k2d
{

    namespace
    {
        ct::Json WriteVec2(const Math::Vec2 &v)
        {
            ct::Json a = ct::Json::array();
            a.push_back(ct::Json(v.x));
            a.push_back(ct::Json(v.y));
            return a;
        }

        Math::Vec2 ReadVec2(const ct::Json &j, Math::Vec2 def = Math::Vec2(0.0f))
        {
            if (!j.is_array() || j.size() < 2)
                return def;
            return Math::Vec2((float)j[0].as_double(def.x), (float)j[1].as_double(def.y));
        }

        ct::Json WriteVec4(const Math::Vec4 &v)
        {
            ct::Json a = ct::Json::array();
            a.push_back(ct::Json(v.x));
            a.push_back(ct::Json(v.y));
            a.push_back(ct::Json(v.z));
            a.push_back(ct::Json(v.w));
            return a;
        }

        Math::Vec4 ReadVec4(const ct::Json &j, Math::Vec4 def = Math::Vec4(0.0f))
        {
            if (!j.is_array() || j.size() < 4)
                return def;
            return Math::Vec4((float)j[0].as_double(def.x), (float)j[1].as_double(def.y),
                              (float)j[2].as_double(def.z), (float)j[3].as_double(def.w));
        }

        ct::Json WriteColor(const Color &c)
        {
            ct::Json a = ct::Json::array();
            a.push_back(ct::Json(c.r));
            a.push_back(ct::Json(c.g));
            a.push_back(ct::Json(c.b));
            a.push_back(ct::Json(c.a));
            return a;
        }

        Color ReadColor(const ct::Json &j, Color def = Color(1.0f))
        {
            if (!j.is_array() || j.size() < 4)
                return def;
            return Color((float)j[0].as_double(def.r), (float)j[1].as_double(def.g),
                        (float)j[2].as_double(def.b), (float)j[3].as_double(def.a));
        }

        ct::Json WritePackedColor(unsigned int packed)
        {
            ct::Json a = ct::Json::array();
            a.push_back(ct::Json((int)(packed & 0xFFu)));
            a.push_back(ct::Json((int)((packed >> 8) & 0xFFu)));
            a.push_back(ct::Json((int)((packed >> 16) & 0xFFu)));
            a.push_back(ct::Json((int)((packed >> 24) & 0xFFu)));
            return a;
        }

        void ReadPackedColorBytes(const ct::Json &j, unsigned char &r, unsigned char &g,
                                   unsigned char &b, unsigned char &a)
        {
            r = (j.is_array() && j.size() > 0) ? (unsigned char)j[0].as_int(255) : 255;
            g = (j.is_array() && j.size() > 1) ? (unsigned char)j[1].as_int(255) : 255;
            b = (j.is_array() && j.size() > 2) ? (unsigned char)j[2].as_int(255) : 255;
            a = (j.is_array() && j.size() > 3) ? (unsigned char)j[3].as_int(255) : 255;
        }

        using CreateFn = Component *(*)(GameObject &owner);
        using WriteFn = void (*)(const Component &component, ct::Json &data, Assets *assets);
        using ReadFn = void (*)(Component &component, const ct::Json &data, Assets *assets);
        using MatchFn = bool (*)(const Component &component);

        struct TypeEntry
        {
            ComponentType type;
            const char *name;
            CreateFn create;
            WriteFn write;
            ReadFn read;
            MatchFn matches;
        };

        Component *CreateSprite(GameObject &owner)
        {
            return owner.addComponent<SpriteComponent>();
        }

        void WriteSprite(const Component &component, ct::Json &data, Assets *assets)
        {
            const SpriteComponent &sprite = static_cast<const SpriteComponent &>(component);
            const Material2D &material = sprite.material();

            if (assets)
            {
                if (const char *texName = assets->FindTextureName(sprite.texture()))
                    data.set("texture", ct::Json(texName));
                if (const char *normalName = assets->FindTextureName(sprite.normalMap()))
                    data.set("normalMap", ct::Json(normalName));
            }

            data.set("size", WriteVec2(sprite.size()));
            data.set("pivot", WriteVec2(sprite.pivot()));
            data.set("tiling", WriteVec2(sprite.tiling()));
            data.set("color", WriteColor(material.color()));
            if (material.hasSourceRect())
                data.set("sourceRect", WriteVec4(material.sourceRect()));
            data.set("lightMask", ct::Json(sprite.lightMask()));
            data.set("flipX", ct::Json(sprite.flipX()));
            data.set("flipY", ct::Json(sprite.flipY()));
            data.set("ySort", ct::Json(sprite.ySort()));
            data.set("blendMode", ct::Json((int)sprite.blendMode()));
        }

        void ReadSprite(Component &component, const ct::Json &data, Assets *assets)
        {
            SpriteComponent &sprite = static_cast<SpriteComponent &>(component);

            if (assets)
            {
                if (const ct::Json *tex = data.find("texture"))
                    sprite.setTexture(assets->GetTexture(tex->as_cstr()));
                if (const ct::Json *normal = data.find("normalMap"))
                    sprite.setNormalMap(assets->GetTexture(normal->as_cstr()));
            }

            sprite.setSize(ReadVec2(data["size"], sprite.size()));
            sprite.setPivot(ReadVec2(data["pivot"], sprite.pivot()));
            const Math::Vec2 tiling = ReadVec2(data["tiling"], Math::Vec2(1.0f, 1.0f));
            sprite.setTiling(tiling.x, tiling.y);

            const Color color = ReadColor(data["color"], Color(1.0f, 1.0f, 1.0f, 1.0f));
            sprite.setColor((unsigned char)std::lround(color.r * 255.0f),
                             (unsigned char)std::lround(color.g * 255.0f),
                             (unsigned char)std::lround(color.b * 255.0f),
                             (unsigned char)std::lround(color.a * 255.0f));

            if (const ct::Json *rect = data.find("sourceRect"))
            {
                const Math::Vec4 r = ReadVec4(*rect);
                sprite.setSourceRect(r.x, r.y, r.z, r.w);
            }

            sprite.setLightMask((unsigned int)data["lightMask"].as_uint(1));
            sprite.setFlip(data["flipX"].as_bool(false), data["flipY"].as_bool(false));
            sprite.setYSort(data["ySort"].as_bool(false));
            sprite.setBlendMode((BlendMode)data["blendMode"].as_int(BLEND_MIX));
        }

        Component *CreateTileMap(GameObject &owner)
        {
            return owner.addComponent<TileMapComponent>();
        }

        void WriteTileMap(const Component &component, ct::Json &data, Assets *assets)
        {
            const TileMapComponent &tileMap = static_cast<const TileMapComponent &>(component);

            if (assets)
                if (const char *texName = assets->FindTextureName(tileMap.texture()))
                    data.set("texture", ct::Json(texName));

            data.set("cellWidth", ct::Json((double)tileMap.cellWidth()));
            data.set("cellHeight", ct::Json((double)tileMap.cellHeight()));
            data.set("columns", ct::Json(tileMap.columns()));
            data.set("rows", ct::Json(tileMap.rows()));
            data.set("atlasTilesX", ct::Json(tileMap.atlasTilesX()));

            ct::Json cells = ct::Json::array();
            cells.reserve((size_t)(tileMap.columns() * tileMap.rows()));
            for (int y = 0; y < tileMap.rows(); ++y)
                for (int x = 0; x < tileMap.columns(); ++x)
                    cells.push_back(ct::Json(tileMap.getTile(x, y)));
            data.set("cells", cells);

            if (tileMap.hasCullRect())
                data.set("cullRect", WriteVec4(tileMap.cullRect()));
            data.set("blendMode", ct::Json((int)tileMap.blendMode()));
        }

        void ReadTileMap(Component &component, const ct::Json &data, Assets *assets)
        {
            TileMapComponent &tileMap = static_cast<TileMapComponent &>(component);

            if (assets)
                if (const ct::Json *tex = data.find("texture"))
                    tileMap.setTexture(assets->GetTexture(tex->as_cstr()));

            tileMap.setCellSize((float)data["cellWidth"].as_double(32.0),
                                 (float)data["cellHeight"].as_double(32.0));
            const int columns = (int)data["columns"].as_int(0);
            const int rows = (int)data["rows"].as_int(0);
            tileMap.setMapSize(columns, rows);
            tileMap.setAtlasTilesX((int)data["atlasTilesX"].as_int(1));

            const ct::Json &cells = data["cells"];
            if (cells.is_array())
            {
                size_t index = 0;
                for (int y = 0; y < rows && index < cells.size(); ++y)
                    for (int x = 0; x < columns && index < cells.size(); ++x, ++index)
                        tileMap.setTile(x, y, (int)cells[index].as_int(0));
            }

            if (const ct::Json *rect = data.find("cullRect"))
            {
                const Math::Vec4 r = ReadVec4(*rect);
                tileMap.setCullRect(r.x, r.y, r.z, r.w);
            }
            tileMap.setBlendMode((BlendMode)data["blendMode"].as_int(BLEND_MIX));
        }

        Component *CreatePolygon(GameObject &owner)
        {
            return owner.addComponent<Polygon2D>();
        }

        void WritePolygon(const Component &component, ct::Json &data, Assets *assets)
        {
            const Polygon2D &polygon = static_cast<const Polygon2D &>(component);

            if (assets)
                if (const char *texName = assets->FindTextureName(polygon.texture()))
                    data.set("texture", ct::Json(texName));

            ct::Json points = ct::Json::array();
            for (size_t i = 0; i < polygon.polygon().size(); ++i)
                points.push_back(WriteVec2(polygon.polygon()[i]));
            data.set("points", points);
            data.set("color", WritePackedColor(polygon.color().Packed()));
            data.set("blendMode", ct::Json((int)polygon.blendMode()));
        }

        void ReadPolygon(Component &component, const ct::Json &data, Assets *assets)
        {
            Polygon2D &polygon = static_cast<Polygon2D &>(component);

            if (assets)
                if (const ct::Json *tex = data.find("texture"))
                    polygon.setTexture(assets->GetTexture(tex->as_cstr()));

            const ct::Json &pointsJson = data["points"];
            if (pointsJson.is_array() && pointsJson.size() >= 3)
            {
                ct::Vector<Math::Vec2> points;
                for (size_t i = 0; i < pointsJson.size(); ++i)
                    points.push_back(ReadVec2(pointsJson[i]));
                polygon.setPolygon(points.data(), (int)points.size());
            }

            unsigned char r, g, b, a;
            ReadPackedColorBytes(data["color"], r, g, b, a);
            polygon.setColor(r, g, b, a);
            polygon.setBlendMode((BlendMode)data["blendMode"].as_int(BLEND_MIX));
        }

        Component *CreateLine(GameObject &owner)
        {
            return owner.addComponent<Line2D>();
        }

        void WriteLine(const Component &component, ct::Json &data, Assets *assets)
        {
            const Line2D &line = static_cast<const Line2D &>(component);

            if (assets)
                if (const char *texName = assets->FindTextureName(line.texture()))
                    data.set("texture", ct::Json(texName));

            ct::Json points = ct::Json::array();
            for (size_t i = 0; i < line.points().size(); ++i)
                points.push_back(WriteVec2(line.points()[i]));
            data.set("points", points);
            data.set("width", ct::Json((double)line.width()));
            data.set("closed", ct::Json(line.closed()));
            data.set("color", WritePackedColor(line.color().Packed()));
            data.set("blendMode", ct::Json((int)line.blendMode()));
        }

        void ReadLine(Component &component, const ct::Json &data, Assets *assets)
        {
            Line2D &line = static_cast<Line2D &>(component);

            if (assets)
                if (const ct::Json *tex = data.find("texture"))
                    line.setTexture(assets->GetTexture(tex->as_cstr()));

            const ct::Json &pointsJson = data["points"];
            if (pointsJson.is_array())
            {
                ct::Vector<Math::Vec2> points;
                for (size_t i = 0; i < pointsJson.size(); ++i)
                    points.push_back(ReadVec2(pointsJson[i]));
                if (!points.empty())
                    line.setPoints(points.data(), (int)points.size());
            }

            line.setWidth((float)data["width"].as_double(4.0));
            line.setClosed(data["closed"].as_bool(false));

            unsigned char r, g, b, a;
            ReadPackedColorBytes(data["color"], r, g, b, a);
            line.setColor(r, g, b, a);
            line.setBlendMode((BlendMode)data["blendMode"].as_int(BLEND_MIX));
        }

        Component *CreateNinePatch(GameObject &owner)
        {
            return owner.addComponent<NinePatchComponent>();
        }

        void WriteNinePatch(const Component &component, ct::Json &data, Assets *assets)
        {
            const NinePatchComponent &ninePatch = static_cast<const NinePatchComponent &>(component);

            if (assets)
                if (const char *texName = assets->FindTextureName(ninePatch.texture()))
                    data.set("texture", ct::Json(texName));

            data.set("size", WriteVec2(ninePatch.size()));
            data.set("margins", WriteVec4(ninePatch.margins()));
            data.set("pivot", WriteVec2(ninePatch.pivot()));
            data.set("color", WritePackedColor(ninePatch.color().Packed()));
            data.set("blendMode", ct::Json((int)ninePatch.blendMode()));
        }

        void ReadNinePatch(Component &component, const ct::Json &data, Assets *assets)
        {
            NinePatchComponent &ninePatch = static_cast<NinePatchComponent &>(component);

            if (assets)
                if (const ct::Json *tex = data.find("texture"))
                    ninePatch.setTexture(assets->GetTexture(tex->as_cstr()));

            ninePatch.setSize(ReadVec2(data["size"], Math::Vec2(64.0f, 64.0f)));
            const Math::Vec4 margins = ReadVec4(data["margins"], Math::Vec4(8.0f));
            ninePatch.setMargins(margins.x, margins.y, margins.z, margins.w);
            ninePatch.setPivot(ReadVec2(data["pivot"]));

            unsigned char r, g, b, a;
            ReadPackedColorBytes(data["color"], r, g, b, a);
            ninePatch.setColor(r, g, b, a);
            ninePatch.setBlendMode((BlendMode)data["blendMode"].as_int(BLEND_MIX));
        }

        Component *CreateSpriteBatch(GameObject &owner)
        {
            return owner.addComponent<SpriteBatch>();
        }

        void WriteSpriteBatch(const Component &component, ct::Json &data, Assets *assets)
        {
            const SpriteBatch &batch = static_cast<const SpriteBatch &>(component);

            ct::Json entries = ct::Json::array();
            for (int i = 0; i < batch.count(); ++i)
            {
                const SpriteBatch::Entry *e = batch.entry(i);
                if (!e)
                    continue;
                ct::Json entryJson = ct::Json::object();
                if (assets)
                    if (const char *texName = assets->FindTextureName(e->texture))
                        entryJson.set("texture", ct::Json(texName));
                entryJson.set("position", WriteVec2(e->position));
                entryJson.set("size", WriteVec2(e->size));
                entryJson.set("source", WriteVec4(e->source));
                entryJson.set("color", ct::Json(e->color.Packed()));
                entryJson.set("flipX", ct::Json((e->flags & 0x1u) != 0));
                entryJson.set("flipY", ct::Json((e->flags & 0x2u) != 0));
                entries.push_back(entryJson);
            }
            data.set("entries", entries);
            data.set("blendMode", ct::Json((int)batch.blendMode()));
        }

        void ReadSpriteBatch(Component &component, const ct::Json &data, Assets *assets)
        {
            SpriteBatch &batch = static_cast<SpriteBatch &>(component);
            batch.clear();

            const ct::Json &entries = data["entries"];
            if (!entries.is_array())
                return;

            for (size_t i = 0; i < entries.size(); ++i)
            {
                const ct::Json &e = entries[i];
                Texture *texture = nullptr;
                if (assets)
                    if (const ct::Json *tex = e.find("texture"))
                        texture = assets->GetTexture(tex->as_cstr());

                const unsigned int color = (unsigned int)e["color"].as_uint(0xFFFFFFFFu);
                const int index = batch.add(texture, ReadVec2(e["position"]),
                                             ReadVec2(e["size"], Math::Vec2(1.0f)), color);
                batch.setSource(index, ReadVec4(e["source"]));
                batch.setFlip(index, e["flipX"].as_bool(false), e["flipY"].as_bool(false));
            }
            batch.setBlendMode((BlendMode)data["blendMode"].as_int(BLEND_MIX));
        }

        Component *CreateAnimation(GameObject &owner)
        {
            return owner.addComponent<Animation2D>();
        }

        void WriteAnimation(const Component &component, ct::Json &data, Assets *assets)
        {
            const Animation2D &anim = static_cast<const Animation2D &>(component);

            ct::Json clips = ct::Json::array();
            for (size_t i = 0; i < anim.clipCount(); ++i)
            {
                const AnimationClip *clip = anim.clipAt(i);
                if (!clip)
                    continue;
                ct::Json clipJson = ct::Json::object();
                clipJson.set("name", ct::Json(clip->name.c_str()));
                if (assets)
                    if (const char *texName = assets->FindTextureName(clip->texture))
                        clipJson.set("texture", ct::Json(texName));
                clipJson.set("frameWidth", ct::Json(clip->frameWidth));
                clipJson.set("frameHeight", ct::Json(clip->frameHeight));
                clipJson.set("frameCount", ct::Json(clip->frameCount));
                clipJson.set("framesPerSecond", ct::Json((double)clip->framesPerSecond));
                clipJson.set("mode", ct::Json((int)clip->mode));
                clips.push_back(clipJson);
            }
            data.set("clips", clips);
            data.set("activeClip", ct::Json(anim.currentClip()));
            data.set("playing", ct::Json(anim.playing()));
        }

        void ReadAnimation(Component &component, const ct::Json &data, Assets *assets)
        {
            Animation2D &anim = static_cast<Animation2D &>(component);

            const ct::Json &clips = data["clips"];
            if (clips.is_array())
            {
                for (size_t i = 0; i < clips.size(); ++i)
                {
                    const ct::Json &clip = clips[i];
                    Texture *texture = nullptr;
                    if (assets)
                        if (const ct::Json *tex = clip.find("texture"))
                            texture = assets->GetTexture(tex->as_cstr());
                    anim.addClip(clip["name"].as_cstr(""), texture,
                                 (int)clip["frameWidth"].as_int(0), (int)clip["frameHeight"].as_int(0),
                                 (int)clip["frameCount"].as_int(0),
                                 (float)clip["framesPerSecond"].as_double(0.0),
                                 (AnimationMode)clip["mode"].as_int((int)AnimationMode::Loop));
                }
            }

            const char *activeName = data["activeClip"].as_cstr(nullptr);
            if (activeName && *activeName)
                anim.play(activeName);
            if (data["playing"].as_bool(false))
                anim.play();
            else
                anim.stop();
        }

        Component *CreatePointLight(GameObject &owner)
        {
            return owner.addComponent<Light2D>();
        }

        Component *CreateDirectionalLight(GameObject &owner)
        {
            return owner.addComponent<DirectionalLight2D>();
        }

        bool IsPointLight(const Component &component)
        {
            return dynamic_cast<const Light2D *>(&component) != nullptr;
        }

        bool IsDirectionalLight(const Component &component)
        {
            return dynamic_cast<const DirectionalLight2D *>(&component) != nullptr;
        }

        void WritePointLight(const Component &component, ct::Json &data, Assets *)
        {
            const Light2D &light = static_cast<const Light2D &>(component);
            data.set("color", WriteColor(light.color()));
            data.set("energy", ct::Json((double)light.energy()));
            data.set("radius", ct::Json((double)light.radius()));
            data.set("castShadow", ct::Json(light.castShadow()));
            data.set("shadowColor", WriteColor(light.shadowColor()));
            data.set("shadowFilter", ct::Json((int)light.shadowFilter()));
            data.set("cullMask", ct::Json(light.cullMask()));
            data.set("height", ct::Json((double)light.height()));
        }

        void ReadPointLight(Component &component, const ct::Json &data, Assets *)
        {
            Light2D &light = static_cast<Light2D &>(component);
            const Color color = ReadColor(data["color"], Color(1.0f));
            light.setColor(color.r, color.g, color.b, color.a);
            light.setEnergy((float)data["energy"].as_double(1.0));
            light.setRadius((float)data["radius"].as_double(300.0));
            light.setCastShadow(data["castShadow"].as_bool(false));
            const Color shadowColor = ReadColor(data["shadowColor"], Color(0.0f, 0.0f, 0.0f, 1.0f));
            light.setShadowColor(shadowColor.r, shadowColor.g, shadowColor.b, shadowColor.a);
            light.setShadowFilter((ShadowFilter)data["shadowFilter"].as_int(SHADOW_FILTER_NEAREST));
            light.setCullMask((unsigned int)data["cullMask"].as_uint(1));
            light.setHeight((float)data["height"].as_double(0.0));
        }

        void WriteDirectionalLight(const Component &component, ct::Json &data, Assets *)
        {
            const DirectionalLight2D &light = static_cast<const DirectionalLight2D &>(component);
            data.set("color", WriteColor(light.color()));
            data.set("energy", ct::Json((double)light.energy()));
            data.set("castShadow", ct::Json(light.castShadow()));
            data.set("shadowColor", WriteColor(light.shadowColor()));
            data.set("shadowFilter", ct::Json((int)light.shadowFilter()));
            data.set("cullMask", ct::Json(light.cullMask()));
            data.set("height", ct::Json((double)light.height()));
        }

        void ReadDirectionalLight(Component &component, const ct::Json &data, Assets *)
        {
            DirectionalLight2D &light = static_cast<DirectionalLight2D &>(component);
            const Color color = ReadColor(data["color"], Color(1.0f));
            light.setColor(color.r, color.g, color.b, color.a);
            light.setEnergy((float)data["energy"].as_double(1.0));
            light.setCastShadow(data["castShadow"].as_bool(false));
            const Color shadowColor = ReadColor(data["shadowColor"], Color(0.0f, 0.0f, 0.0f, 1.0f));
            light.setShadowColor(shadowColor.r, shadowColor.g, shadowColor.b, shadowColor.a);
            light.setShadowFilter((ShadowFilter)data["shadowFilter"].as_int(SHADOW_FILTER_NEAREST));
            light.setCullMask((unsigned int)data["cullMask"].as_uint(1));
            light.setHeight((float)data["height"].as_double(0.0));
        }

        Component *CreateOccluder(GameObject &owner)
        {
            return owner.addComponent<LightOccluder2D>();
        }

        void WriteOccluder(const Component &component, ct::Json &data, Assets *)
        {
            const LightOccluder2D &occluder = static_cast<const LightOccluder2D &>(component);
            ct::Json points = ct::Json::array();
            for (size_t i = 0; i < occluder.points().size(); ++i)
                points.push_back(WriteVec2(occluder.points()[i]));
            data.set("points", points);
        }

        void ReadOccluder(Component &component, const ct::Json &data, Assets *)
        {
            LightOccluder2D &occluder = static_cast<LightOccluder2D &>(component);
            const ct::Json &pointsJson = data["points"];
            if (!pointsJson.is_array())
                return;
            ct::Vector<Math::Vec2> points;
            for (size_t i = 0; i < pointsJson.size(); ++i)
                points.push_back(ReadVec2(pointsJson[i]));
            if (!points.empty())
                occluder.setPolygon(points.data(), (int)points.size());
        }

        Component *CreateCamera(GameObject &owner)
        {
            return owner.addComponent<CameraComponent>();
        }

        void WriteCamera(const Component &component, ct::Json &data, Assets *)
        {
            const CameraComponent &cameraComponent = static_cast<const CameraComponent &>(component);
            const Camera2D &camera = cameraComponent.camera();

            data.set("viewportWidth", ct::Json((double)cameraComponent.viewportWidth()));
            data.set("viewportHeight", ct::Json((double)cameraComponent.viewportHeight()));
            data.set("position", WriteVec2(camera.position));
            data.set("rotationDegrees", ct::Json((double)camera.rotationDegrees));
            data.set("zoom", WriteVec2(camera.zoom));
            data.set("offset", WriteVec2(camera.offset));
            data.set("limitEnabled", ct::Json(camera.limitEnabled));
            data.set("limits", WriteVec4(camera.limits));
            data.set("smoothingEnabled", ct::Json(camera.smoothingEnabled));
            data.set("smoothingSpeed", ct::Json((double)camera.smoothingSpeed));
            data.set("deadZoneEnabled", ct::Json(camera.deadZoneEnabled));
            data.set("deadZone", WriteVec4(camera.deadZone));
            data.set("targetEnabled", ct::Json(camera.targetEnabled));
            data.set("target", WriteVec2(camera.target));
        }

        void ReadCamera(Component &component, const ct::Json &data, Assets *)
        {
            CameraComponent &cameraComponent = static_cast<CameraComponent &>(component);
            cameraComponent.setViewport((float)data["viewportWidth"].as_double(0.0),
                                         (float)data["viewportHeight"].as_double(0.0));

            Camera2D &camera = cameraComponent.camera();
            camera.position = ReadVec2(data["position"]);
            camera.rotationDegrees = (float)data["rotationDegrees"].as_double(0.0);
            camera.zoom = ReadVec2(data["zoom"], Math::Vec2(1.0f, 1.0f));
            camera.offset = ReadVec2(data["offset"]);
            camera.limitEnabled = data["limitEnabled"].as_bool(false);
            camera.limits = ReadVec4(data["limits"]);
            camera.smoothingEnabled = data["smoothingEnabled"].as_bool(false);
            camera.smoothingSpeed = (float)data["smoothingSpeed"].as_double(5.0);
            camera.deadZoneEnabled = data["deadZoneEnabled"].as_bool(false);
            camera.deadZone = ReadVec4(data["deadZone"]);
            camera.targetEnabled = data["targetEnabled"].as_bool(false);
            camera.target = ReadVec2(data["target"]);
        }

        ct::Json WriteParticlePrefab(const ParticlePrefab &prefab)
        {
            ct::Json j = ct::Json::object();
            j.set("direction", WriteVec2(prefab.direction));
            j.set("spreadDegrees", ct::Json((double)prefab.spreadDegrees));
            j.set("speedMin", ct::Json((double)prefab.speedMin));
            j.set("speedMax", ct::Json((double)prefab.speedMax));
            j.set("lifeMin", ct::Json((double)prefab.lifeMin));
            j.set("lifeMax", ct::Json((double)prefab.lifeMax));
            j.set("sizeMin", ct::Json((double)prefab.sizeMin));
            j.set("sizeMax", ct::Json((double)prefab.sizeMax));
            j.set("endSize", ct::Json((double)prefab.endSize));
            j.set("rotationMin", ct::Json((double)prefab.rotationMin));
            j.set("rotationMax", ct::Json((double)prefab.rotationMax));
            j.set("angularVelocityMin", ct::Json((double)prefab.angularVelocityMin));
            j.set("angularVelocityMax", ct::Json((double)prefab.angularVelocityMax));
            j.set("drag", ct::Json((double)prefab.drag));
            j.set("faceDirection", ct::Json(prefab.faceDirection));
            j.set("faceDirectionOffsetDegrees", ct::Json((double)prefab.faceDirectionOffsetDegrees));
            j.set("fadeIn", ct::Json((double)prefab.fadeIn));
            j.set("fadeOut", ct::Json((double)prefab.fadeOut));
            j.set("colorStart", WriteColor(prefab.colorStart));
            j.set("colorEnd", WriteColor(prefab.colorEnd));
            j.set("atlasBounds", WriteVec4(prefab.atlasBounds));
            return j;
        }

        ParticlePrefab ReadParticlePrefab(const ct::Json &j)
        {
            ParticlePrefab prefab;
            prefab.direction = ReadVec2(j["direction"], Math::Vec2(0.0f, -1.0f));
            prefab.spreadDegrees = (float)j["spreadDegrees"].as_double(0.0);
            prefab.speedMin = (float)j["speedMin"].as_double(0.0);
            prefab.speedMax = (float)j["speedMax"].as_double(0.0);
            prefab.lifeMin = (float)j["lifeMin"].as_double(1.0);
            prefab.lifeMax = (float)j["lifeMax"].as_double(1.0);
            prefab.sizeMin = (float)j["sizeMin"].as_double(1.0);
            prefab.sizeMax = (float)j["sizeMax"].as_double(1.0);
            prefab.endSize = (float)j["endSize"].as_double(0.0);
            prefab.rotationMin = (float)j["rotationMin"].as_double(0.0);
            prefab.rotationMax = (float)j["rotationMax"].as_double(0.0);
            prefab.angularVelocityMin = (float)j["angularVelocityMin"].as_double(0.0);
            prefab.angularVelocityMax = (float)j["angularVelocityMax"].as_double(0.0);
            prefab.drag = (float)j["drag"].as_double(0.0);
            prefab.faceDirection = j["faceDirection"].as_bool(false);
            prefab.faceDirectionOffsetDegrees = (float)j["faceDirectionOffsetDegrees"].as_double(0.0);
            prefab.fadeIn = (float)j["fadeIn"].as_double(0.0);
            prefab.fadeOut = (float)j["fadeOut"].as_double(0.0);
            prefab.colorStart = ReadColor(j["colorStart"], Color(1.0f));
            prefab.colorEnd = ReadColor(j["colorEnd"], Color(1.0f));
            prefab.atlasBounds = ReadVec4(j["atlasBounds"], Math::Vec4(0.0f));
            return prefab;
        }

        Component *CreateParticle(GameObject &owner)
        {
            return owner.addComponent<ParticleComponent>();
        }

        void WriteParticle(const Component &component, ct::Json &data, Assets *assets)
        {
            const ParticleComponent &particleComponent = static_cast<const ParticleComponent &>(component);
            const ParticleSystem &system = particleComponent.system();

            if (assets)
                if (const char *texName = assets->FindTextureName(system.GetTexture()))
                    data.set("texture", ct::Json(texName));

            data.set("capacity", ct::Json(system.Capacity()));
            data.set("mode", ct::Json((int)system.GetMode()));
            data.set("emitterShape", ct::Json((int)system.GetEmitterShape()));
            data.set("emitterSize", WriteVec2(system.EmitterSize()));
            data.set("emitterPosition", WriteVec2(system.EmitterPosition()));
            data.set("emissionRate", ct::Json((double)system.EmissionRate()));
            data.set("oneShotCount", ct::Json(system.OneShotCount()));
            data.set("gravity", WriteVec2(system.Gravity()));
            data.set("playing", ct::Json(system.IsPlaying()));
            data.set("blendMode", ct::Json((int)particleComponent.blendMode()));
            data.set("ySort", ct::Json(particleComponent.ySort()));
            data.set("followOwner", ct::Json(particleComponent.followOwner()));
            data.set("prefab", WriteParticlePrefab(system.GetPrefab()));
        }

        void ReadParticle(Component &component, const ct::Json &data, Assets *assets)
        {
            ParticleComponent &particleComponent = static_cast<ParticleComponent &>(component);
            ParticleSystem &system = particleComponent.system();

            if (assets)
                if (const ct::Json *tex = data.find("texture"))
                    system.SetTexture(assets->GetTexture(tex->as_cstr()));

            system.SetCapacity((size_t)data["capacity"].as_uint(256));
            system.SetMode((ParticleMode)data["mode"].as_int((int)ParticleMode::Persistent));
            system.SetEmitterShape((ParticleEmitterShape)data["emitterShape"].as_int((int)ParticleEmitterShape::Point));
            system.SetEmitterSize(ReadVec2(data["emitterSize"]));
            system.SetEmitterPosition(ReadVec2(data["emitterPosition"]));
            system.SetEmissionRate((float)data["emissionRate"].as_double(0.0));
            system.SetOneShotCount((size_t)data["oneShotCount"].as_uint(1));
            system.SetGravity(ReadVec2(data["gravity"]));
            system.SetPrefab(ReadParticlePrefab(data["prefab"]));

            particleComponent.setBlendMode((BlendMode)data["blendMode"].as_int(BLEND_MIX));
            particleComponent.setYSort(data["ySort"].as_bool(false));
            particleComponent.setFollowOwner(data["followOwner"].as_bool(true));

            if (data["playing"].as_bool(false))
                system.Start();
            else
                system.Stop();
        }

        const TypeEntry *AllEntries(std::size_t &count)
        {
            static const TypeEntry kEntries[] = {
                {ComponentType::Sprite, "Sprite", &CreateSprite, &WriteSprite, &ReadSprite, nullptr},
                {ComponentType::TileMap, "TileMap", &CreateTileMap, &WriteTileMap, &ReadTileMap, nullptr},
                {ComponentType::Polygon2D, "Polygon2D", &CreatePolygon, &WritePolygon, &ReadPolygon, nullptr},
                {ComponentType::LinePath, "Line2D", &CreateLine, &WriteLine, &ReadLine, nullptr},
                {ComponentType::NinePatch, "NinePatch", &CreateNinePatch, &WriteNinePatch, &ReadNinePatch, nullptr},
                {ComponentType::SpriteBatch, "SpriteBatch", &CreateSpriteBatch, &WriteSpriteBatch, &ReadSpriteBatch, nullptr},
                {ComponentType::Animation, "Animation2D", &CreateAnimation, &WriteAnimation, &ReadAnimation, nullptr},
                {ComponentType::Light, "Light2D", &CreatePointLight, &WritePointLight, &ReadPointLight, &IsPointLight},
                {ComponentType::Light, "DirectionalLight2D", &CreateDirectionalLight, &WriteDirectionalLight, &ReadDirectionalLight, &IsDirectionalLight},
                {ComponentType::Occluder, "LightOccluder2D", &CreateOccluder, &WriteOccluder, &ReadOccluder, nullptr},
                {ComponentType::Camera, "Camera", &CreateCamera, &WriteCamera, &ReadCamera, nullptr},
                {ComponentType::Particle, "Particle", &CreateParticle, &WriteParticle, &ReadParticle, nullptr},
            };
            count = sizeof(kEntries) / sizeof(kEntries[0]);
            return kEntries;
        }

        bool AnyEntryForType(ComponentType type)
        {
            std::size_t count = 0;
            const TypeEntry *entries = AllEntries(count);
            for (std::size_t i = 0; i < count; ++i)
                if (entries[i].type == type)
                    return true;
            return false;
        }

        const TypeEntry *FindEntryForComponent(const Component &component)
        {
            std::size_t count = 0;
            const TypeEntry *entries = AllEntries(count);
            const TypeEntry *fallback = nullptr;
            for (std::size_t i = 0; i < count; ++i)
            {
                if (entries[i].type != component.type())
                    continue;
                if (entries[i].matches)
                {
                    if (entries[i].matches(component))
                        return &entries[i];
                }
                else
                {
                    fallback = &entries[i];
                }
            }
            return fallback;
        }

        const TypeEntry *FindByName(const char *name)
        {
            if (!name)
                return nullptr;
            std::size_t count = 0;
            const TypeEntry *entries = AllEntries(count);
            for (std::size_t i = 0; i < count; ++i)
                if (std::strcmp(entries[i].name, name) == 0)
                    return &entries[i];
            return nullptr;
        }

        ct::Json WriteComponent(const Component &component, Assets *assets)
        {
            const TypeEntry *entry = FindEntryForComponent(component);
            ct::Json result = ct::Json::object();
            if (!entry)
                return result;
            result.set("type", ct::Json(entry->name));
            ct::Json data = ct::Json::object();
            entry->write(component, data, assets);
            result.set("data", data);
            return result;
        }

        void ReadComponent(GameObject &owner, const ct::Json &entryJson, Assets *assets)
        {
            const TypeEntry *entry = FindByName(entryJson["type"].as_cstr(nullptr));
            if (!entry)
                return;
            Component *component = entry->create(owner);
            if (!component)
                return;
            entry->read(*component, entryJson["data"], assets);
        }
    }

    bool Serializer::IsRegistered(ComponentType type)
    {
        return AnyEntryForType(type);
    }

    ct::Json Serializer::WriteObject(const GameObject &object, Assets *assets)
    {
        ct::Json j = ct::Json::object();
        j.set("name", ct::Json(object.name().c_str()));
        j.set("tag", ct::Json(object.tag().c_str()));
        j.set("active", ct::Json(object.active()));
        j.set("visible", ct::Json(object.visible()));
        j.set("zIndex", ct::Json(object.zIndex()));
        j.set("position", WriteVec2(object.position()));
        j.set("rotation", ct::Json((double)object.rotationDegrees()));
        j.set("scale", WriteVec2(object.scale()));

        ct::Json components = ct::Json::array();
        for (uint8_t t = 0; t < (uint8_t)ComponentType::Count; ++t)
        {
            const std::size_t count = object.rawComponentCount((ComponentType)t);
            for (std::size_t i = 0; i < count; ++i)
            {
                const Component *component = object.rawComponent((ComponentType)t, i);
                ct::Json entryJson = WriteComponent(*component, assets);
                if (!entryJson.contains("type"))
                    continue;
                components.push_back(entryJson);
            }
        }
        j.set("components", components);

        ct::Json children = ct::Json::array();
        for (std::size_t i = 0; i < object.childCount(); ++i)
            children.push_back(WriteObject(*object.child(i), assets));
        j.set("children", children);

        return j;
    }

    GameObject *Serializer::ReadObject(Scene &scene, const ct::Json &json, GameObject *parent, Assets *assets)
    {
        GameObject *object = scene.createObject(json["name"].as_cstr(""), parent);
        if (!object)
            return nullptr;

        object->setTag(json["tag"].as_cstr(""));
        object->setActive(json["active"].as_bool(true));
        object->setVisible(json["visible"].as_bool(true));
        object->setZIndex((int)json["zIndex"].as_int(0));
        object->setPosition(ReadVec2(json["position"]));
        object->setRotationDegrees((float)json["rotation"].as_double(0.0));
        object->setScale(ReadVec2(json["scale"], Math::Vec2(1.0f, 1.0f)));

        const ct::Json &components = json["components"];
        if (components.is_array())
            for (std::size_t i = 0; i < components.size(); ++i)
                ReadComponent(*object, components[i], assets);

        const ct::Json &children = json["children"];
        if (children.is_array())
            for (std::size_t i = 0; i < children.size(); ++i)
                ReadObject(scene, children[i], object, assets);

        return object;
    }

}