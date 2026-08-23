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
        static bool IsRegistered(ComponentType type);
        static bool RegisterType(ComponentType type, const char *name, CreateFn create,
                                 WriteFn write, ReadFn read, MatchFn matches = nullptr);
    };

}