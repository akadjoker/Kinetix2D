#pragma once

#include "k2d/Component.h"

#include <ct/json.hpp>

namespace k2d
{

    class GameObject;
    class Scene;
    class Assets;

    class Serializer
    {
    public:
        using CreateFn = Component *(*)(GameObject &owner);
        using WriteFn = void (*)(const Component &component, ct::Json &data, Assets *assets);
        using ReadFn = void (*)(Component &component, const ct::Json &data, Assets *assets);
        using MatchFn = bool (*)(const Component &component);

        static ct::Json WriteObject(const GameObject &object, Assets *assets = nullptr);
        static GameObject *ReadObject(Scene &scene, const ct::Json &json,
                                       GameObject *parent = nullptr, Assets *assets = nullptr);
        // Reads json into an object that already exists, instead of creating
        // one. A saved scene's "root" describes the scene root itself, so
        // loading it with ReadObject would nest a second root under the real
        // one and leave script-spawned objects outside it.
        static void ReadInto(Scene &scene, GameObject &object, const ct::Json &json,
                             Assets *assets = nullptr);
        static bool IsRegistered(ComponentType type);
        // The registered component types by name, built-in first: what an editor
        // offers to add, and Create adds one with its defaults.
        static std::size_t TypeCount();
        static const char *TypeName(std::size_t index);
        static Component *Create(GameObject &owner, const char *typeName);
        // One component's data as the scene file holds it, and read back into it.
        static ct::Json WriteComponentData(const Component &component, Assets *assets = nullptr);
        static bool ReadComponentData(Component &component, const ct::Json &data, Assets *assets = nullptr);
        static bool RegisterType(ComponentType type, const char *name, CreateFn create,
                                 WriteFn write, ReadFn read, MatchFn matches = nullptr);
    };

}