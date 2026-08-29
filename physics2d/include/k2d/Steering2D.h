#pragma once

#include "k2d/Component.h"

#include <ct/string.hpp>
#include <mathc.h>

namespace k2d
{

    class GameObject;
    class Scene;

    constexpr float kSteeringEpsilon = 0.0001f;

    // Every steering behaviour in one place. Seek, flee, arrive and wander are
    // arithmetic a script can ask for at any moment; separation and obstacle
    // avoidance need the scene's neighbour and collider queries, so they are
    // also the two that can be left running on their own to feed the object's
    // NavigationAgent2D.
    //
    // Behaviours are ported from OpenSteer (Craig Reynolds) except separation's
    // group filter, which follows the Game Institute AI demo: a behaviour looks
    // at its own group, not at everything with a collider.
    class Steering2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Steering;

        Steering2D();
        ~Steering2D() override;

        // All of these answer in world units per second, as a velocity the
        // caller can steer towards, and all subtract the current velocity the
        // way OpenSteer's steerForSeek does - the difference between where you
        // want to be going and where you are going is the whole behaviour.
        Math::Vec2 seek(const Math::Vec2 &target, const Math::Vec2 &velocity, float maxSpeed) const;
        Math::Vec2 flee(const Math::Vec2 &target, const Math::Vec2 &velocity, float maxSpeed) const;
        Math::Vec2 arrive(const Math::Vec2 &target, const Math::Vec2 &velocity, float maxSpeed) const;
        Math::Vec2 wander(float deltaTime, const Math::Vec2 &velocity, float maxSpeed) const;
        // Away from crowding group mates, and the lateral turn that clears
        // whatever the body is about to walk into. Both are zero when there is
        // nothing to react to.
        Math::Vec2 separation(const Math::Vec2 &velocity, float maxSpeed) const;
        Math::Vec2 avoidance(const Math::Vec2 &velocity, float maxSpeed) const;

        // What the NavigationAgent2D adds to its own path following, in
        // fractions of max speed. Avoidance is a veto and not one vote among
        // many: OpenSteer's Pedestrian runs the rest only when it returned
        // nothing, because summing lets a seek drag the agent back into the
        // wall it was turning away from.
        Math::Vec2 force(float deltaTime, const Math::Vec2 &position, const Math::Vec2 &velocity,
                         bool &outVetoed) const;

        float weight() const { return mWeight; }
        void setWeight(float weight);

        bool separationEnabled() const { return mSeparationEnabled; }
        void setSeparationEnabled(bool enabled) { mSeparationEnabled = enabled; }
        float separationRadius() const { return mSeparationRadius; }
        void setSeparationRadius(float radius);

        bool avoidanceEnabled() const { return mAvoidanceEnabled; }
        void setAvoidanceEnabled(bool enabled) { mAvoidanceEnabled = enabled; }
        // OpenSteer's minTimeToCollision: seconds of travel swept ahead.
        float lookAhead() const { return mLookAhead; }
        void setLookAhead(float seconds);

        // Empty means every collider in the mask counts as crowd. A tag names
        // the group this one belongs to, and then only its own kind do.
        const ct::String &groupTag() const { return mGroupTag; }
        void setGroupTag(const char *tag);

        uint16_t mask() const { return mMask; }
        void setMask(uint16_t mask);

        float slowRadius() const { return mSlowRadius; }
        void setSlowRadius(float radius);
        float wanderRadius() const { return mWanderRadius; }
        void setWanderRadius(float radius);
        float wanderDistance() const { return mWanderDistance; }
        void setWanderDistance(float distance);
        float wanderJitter() const { return mWanderJitter; }
        void setWanderJitter(float jitter);

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
        float probeOffset() const;

        ct::String mTargetName;
        ct::String mGroupTag;
        Math::Vec2 mTargetPosition;
        mutable GameObject *mTargetObject;
        mutable uint32_t mResolvedVersion;
        mutable uint32_t mRandom;
        mutable float mWanderAngle;
        float mWeight;
        float mSeparationRadius;
        float mLookAhead;
        float mSlowRadius;
        float mWanderRadius;
        float mWanderDistance;
        float mWanderJitter;
        uint16_t mMask;
        bool mSeparationEnabled;
        bool mAvoidanceEnabled;
    };

    template <> struct ComponentMatch<Steering2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const Steering2D *>(component) != nullptr;
        }
    };

}
