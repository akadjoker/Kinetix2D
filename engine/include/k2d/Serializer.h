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
        static ct::Json WriteObject(const GameObject &object, Assets *assets = nullptr);
        static GameObject *ReadObject(Scene &scene, const ct::Json &json,
                                       GameObject *parent = nullptr, Assets *assets = nullptr);
        static bool IsRegistered(ComponentType type);
    };

}
