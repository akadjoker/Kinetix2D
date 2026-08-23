#pragma once

#include "k2d/Component.h"

namespace k2d
{

    class ScriptComponent : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Script;

    protected:
        explicit ScriptComponent(uint8_t events = ComponentEventUpdate | ComponentEventLateUpdate)
            : Component(Type, events)
        {
        }
    };

}