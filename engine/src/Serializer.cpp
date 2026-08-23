#include "k2d/Serializer.h"

#include "k2d/GameObject.h"
#include "k2d/Scene.h"
#include "k2d/Assets.h"
#include "k2d/SpriteComponent.h"

#include <cmath>
#include <cstring>

namespace k2d
{

    namespace
    {
        // ---- small json <-> glm helpers, shared by every component ----------

        ct::Json WriteVec2(const glm::vec2 &v)
        {
            ct::Json a = ct::Json::array();
            a.push_back(ct::Json(v.x));
            a.push_back(ct::Json(v.y));
            return a;
        }

        glm::vec2 ReadVec2(const ct::Json &j, glm::vec2 def = glm::vec2(0.0f))
        {
            if (!j.is_array() || j.size() < 2)
                return def;
            return glm::vec2((float)j[0].as_double(def.x), (float)j[1].as_double(def.y));
        }

        ct::Json WriteVec4(const glm::vec4 &v)
        {
            ct::Json a = ct::Json::array();
            a.push_back(ct::Json(v.x));
            a.push_back(ct::Json(v.y));
            a.push_back(ct::Json(v.z));
            a.push_back(ct::Json(v.w));
            return a;
        }

        glm::vec4 ReadVec4(const ct::Json &j, glm::vec4 def = glm::vec4(0.0f))
        {
            if (!j.is_array() || j.size() < 4)
                return def;
            return glm::vec4((float)j[0].as_double(def.x), (float)j[1].as_double(def.y),
                              (float)j[2].as_double(def.z), (float)j[3].as_double(def.w));
        }

        // ---- the factory/registry table --------------------------------------
        //
        // One row per serializable Component type. Serializer's public methods
        // never mention a concrete component class or switch on ComponentType --
        // they just look a type up here and call through the function pointers.
        // This is the ONLY place that does; extending coverage to a new
        // component means adding one row (and its two small Write*/Read*
        // functions right above it), not touching Prefab/Scene/anywhere else.

        using CreateFn = Component *(*)(GameObject &owner);
        using WriteFn = void (*)(const Component &component, ct::Json &data, Assets *assets);
        using ReadFn = void (*)(Component &component, const ct::Json &data, Assets *assets);

        struct TypeEntry
        {
            ComponentType type;
            const char *name;
            CreateFn create;
            WriteFn write;
            ReadFn read;
        };

        // ---- SpriteComponent ---------------------------------------------

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
            data.set("color", WriteVec4(material.color()));
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
            const glm::vec2 tiling = ReadVec2(data["tiling"], glm::vec2(1.0f, 1.0f));
            sprite.setTiling(tiling.x, tiling.y);

            // Material2D::color() is normalized (0..1) -- setColor() takes 0..255
            // bytes and re-normalizes internally, so convert back on the way in.
            const glm::vec4 color = ReadVec4(data["color"], glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            sprite.setColor((unsigned char)std::lround(color.r * 255.0f),
                             (unsigned char)std::lround(color.g * 255.0f),
                             (unsigned char)std::lround(color.b * 255.0f),
                             (unsigned char)std::lround(color.a * 255.0f));

            if (const ct::Json *rect = data.find("sourceRect"))
            {
                const glm::vec4 r = ReadVec4(*rect);
                sprite.setSourceRect(r.x, r.y, r.z, r.w);
            }

            sprite.setLightMask((unsigned int)data["lightMask"].as_uint(1));
            sprite.setFlip(data["flipX"].as_bool(false), data["flipY"].as_bool(false));
            sprite.setYSort(data["ySort"].as_bool(false));
            sprite.setBlendMode((BlendMode)data["blendMode"].as_int(BLEND_MIX));
        }

        // ---- registry ------------------------------------------------------

        const TypeEntry *AllEntries(std::size_t &count)
        {
            static const TypeEntry kEntries[] = {
                {ComponentType::Sprite, "Sprite", &CreateSprite, &WriteSprite, &ReadSprite},
            };
            count = sizeof(kEntries) / sizeof(kEntries[0]);
            return kEntries;
        }

        const TypeEntry *FindByType(ComponentType type)
        {
            std::size_t count = 0;
            const TypeEntry *entries = AllEntries(count);
            for (std::size_t i = 0; i < count; ++i)
                if (entries[i].type == type)
                    return &entries[i];
            return nullptr;
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
            ct::Json result = ct::Json::object();
            const TypeEntry *entry = FindByType(component.type());
            if (!entry)
                return result; // unregistered type: caller skips empty entries
            result.set("type", ct::Json(entry->name));
            ct::Json data = ct::Json::object();
            entry->write(component, data, assets);
            result.set("data", data);
            return result;
        }

        // Returns false only when the entry names a component type we don't
        // know how to create (e.g. an older/foreign file) -- not an error for
        // the caller to abort over, just one component silently skipped.
        void ReadComponent(GameObject &owner, const ct::Json &entryJson, Assets *assets)
        {
            const TypeEntry *entry = FindByName(entryJson["type"].as_cstr(nullptr));
            if (!entry)
                return;
            Component *component = entry->create(owner);
            if (!component)
                return; // owner already has one of this type
            entry->read(*component, entryJson["data"], assets);
        }
    }

    bool Serializer::IsRegistered(ComponentType type)
    {
        return FindByType(type) != nullptr;
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
            const Component *component = object.rawComponent((ComponentType)t);
            if (!component)
                continue;
            if (!IsRegistered(component->type()))
                continue; // not (yet) serializable -- skip rather than drop garbage
            components.push_back(WriteComponent(*component, assets));
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
        object->setScale(ReadVec2(json["scale"], glm::vec2(1.0f, 1.0f)));

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
