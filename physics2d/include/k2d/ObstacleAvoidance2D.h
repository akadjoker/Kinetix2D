#pragma once

#include "k2d/Steering2D.h"

namespace k2d
{

    class ObstacleAvoidance2D : public Steering2D
    {
    public:
        ObstacleAvoidance2D();

        // Seconds of travel at the current speed that are probed ahead.
        float lookAhead() const { return mLookAhead; }
        void setLookAhead(float seconds);

        // Which collision categories count as obstacles.
        uint16_t mask() const { return mMask; }
        void setMask(uint16_t mask);

        Math::Vec2 force(float deltaTime, const Math::Vec2 &position, const Math::Vec2 &velocity) const override;

    private:
        float mLookAhead;
        uint16_t mMask;
    };

    template <> struct ComponentMatch<ObstacleAvoidance2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const ObstacleAvoidance2D *>(component) != nullptr;
        }
    };

}
