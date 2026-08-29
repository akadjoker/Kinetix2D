#pragma once

#include "k2d/Component.h"

#include <ct/string.hpp>
#include <mathc.h>

namespace k2d
{

    class GameObject;
    class Scene;

    constexpr float kSteeringEpsilon = 0.0001f;

    class Steering2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Steering;

        Steering2D();
        ~Steering2D() override;

        // Returned in fractions of the agent's own maximum speed, so a
        // behaviour never has to know how fast the thing it steers can move.
        virtual Math::Vec2 force(float deltaTime, const Math::Vec2 &position, const Math::Vec2 &velocity) const = 0;

        // OpenSteer's obstacle avoidance is a veto, not one vote among many:
        // the Pedestrian plugin runs the other behaviours only when avoidance
        // returned nothing. Summing them instead lets a seek drag the agent
        // straight back into the wall it was turning away from.
        virtual bool vetoes() const { return false; }

        float weight() const { return mWeight; }
        void setWeight(float weight);

        void setTargetName(const char *name);
        const ct::String &targetName() const { return mTargetName; }
        bool hasTargetName() const { return !mTargetName.empty(); }

        void setTargetPosition(const Math::Vec2 &position) { mTargetPosition = position; }
        const Math::Vec2 &targetPosition() const { return mTargetPosition; }

        bool target(Math::Vec2 &out) const;

    private:
        // Target names are set one statement after addComponent, so the lookup
        // cannot happen on attach; the topology gate keeps it to one find per
        // topology change, whoever asks first.
        void resolve() const;

        ct::String mTargetName;
        Math::Vec2 mTargetPosition;
        mutable GameObject *mTargetObject;
        mutable uint32_t mResolvedVersion;
        float mWeight;
    };

    template <> struct ComponentMatch<Steering2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const Steering2D *>(component) != nullptr;
        }
    };

}
