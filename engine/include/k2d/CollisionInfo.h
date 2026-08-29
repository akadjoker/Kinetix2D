#pragma once

#include <mathc.h>

namespace k2d
{

class GameObject;

struct CollisionInfo
{
    GameObject* self = nullptr;
    GameObject* other = nullptr;
    Math::Vec2 point = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 normal = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 travel = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 remainder = Math::Vec2(0.0f, 0.0f);
    float fraction = 1.0f;
    bool sensor = false;
    bool began = false;
    bool hit = false;
};

using CollisionCallback = void (*)(const CollisionInfo& info, void* user);

using AnimationEventCallback = void (*)(GameObject* object, const char* clip, const char* event, bool finished,
                                        void* user);

} // namespace k2d
