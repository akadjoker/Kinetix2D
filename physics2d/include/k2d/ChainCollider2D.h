#pragma once

#include "k2d/Collider2D.h"

#include <ct/vector.hpp>

namespace k2d
{

    class ChainCollider2D : public Collider2D
    {
    public:
        ChainCollider2D();

        int addTo(RigidBody2D &body, float density, float scaleX, float scaleY) const override;

        const ct::Vector<Math::Vec2> &points() const { return mPoints; }
        void setPoints(const Math::Vec2 *points, int count);
        bool loop() const { return mLoop; }
        void setLoop(bool loop);
        // Off means the chain blocks from both sides, which is what a traced
        // outline needs; on makes it a jump-through platform.
        bool oneWay() const { return mOneWay; }
        void setOneWay(bool oneWay);

    private:
        ct::Vector<Math::Vec2> mPoints;
        bool mLoop;
        bool mOneWay;
    };

    template <> struct ComponentMatch<ChainCollider2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const ChainCollider2D *>(component) != nullptr;
        }
    };

}
