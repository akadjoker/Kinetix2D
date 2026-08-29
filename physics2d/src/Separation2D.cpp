#include "k2d/Separation2D.h"

#include "k2d/GameObject.h"
#include "k2d/Scene.h"
#include "k2d/Utils.h"

namespace k2d
{

Separation2D::Separation2D() : mRadius(48.0f), mMask(0xFFFF)
{
}

void Separation2D::setRadius(float radius)
{
    mRadius = Max(0.0f, radius);
}

void Separation2D::setMask(uint16_t mask)
{
    mMask = mask;
}

Math::Vec2 Separation2D::force(float, const Math::Vec2 &position, const Math::Vec2 &) const
{
    const GameObject *object = owner();
    Scene *scene = object ? object->scene() : nullptr;
    if (!scene || mRadius <= 0.0f)
        return Math::Vec2(0.0f, 0.0f);

    Math::Vec2 push(0.0f, 0.0f);
    const std::size_t count = scene->queryNeighbours(*object, position, mRadius, mMask);
    for (std::size_t i = 0; i < count; ++i)
    {
        const GameObject *neighbour = scene->neighbourAt(i);
        if (!neighbour || !neighbour->isActiveInHierarchy())
            continue;

        const Math::Vec2 away = position - neighbour->globalPosition();
        const float distance = away.Length();
        if (!(distance < mRadius))
            continue;

        // Repel from every neighbour inside the radius, summed with a falloff.
        // Reacting only to the closest one lets a group collapse into a dense
        // ball where the pushes cancel out and fight each other every frame.
        if (!(distance > kSteeringEpsilon))
        {
            push += Math::Vec2(object->id() < neighbour->id() ? -1.0f : 1.0f, 0.0f);
            continue;
        }
        push += away * ((1.0f - distance / mRadius) / distance);
    }

    const float strength = push.Length();
    if (!(strength > kSteeringEpsilon))
        return Math::Vec2(0.0f, 0.0f);
    return strength > 1.0f ? push * (1.0f / strength) : push;
}

}
