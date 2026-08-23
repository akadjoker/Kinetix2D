#pragma once

#include <glm/glm.hpp>

namespace kx
{

    class Body;

    enum class JointType : unsigned char
    {
        Distance,
        Wheel,
        Mouse,
        Revolute,
        Gear,
        Motor
    };

    class Joint
    {
    public:
        virtual ~Joint();

        JointType Type() const { return mType; }
        Body *BodyA() const { return mBodyA; }
        Body *BodyB() const { return mBodyB; }

        virtual glm::vec2 AnchorA() const = 0;
        virtual glm::vec2 AnchorB() const = 0;

        bool CollideConnected() const { return mCollideConnected; }
        void SetCollideConnected(bool collide) { mCollideConnected = collide; }

    protected:
        Joint(JointType type, Body *a, Body *b);

        virtual void InitVelocity(float dt) = 0;
        virtual void SolveVelocity(float dt) = 0;
        virtual bool SolvePosition() { return true; }

        virtual bool DependsOnBody(const Body *body) const
        {
            (void)body;
            return false;
        }
        virtual bool DependsOnJoint(const Joint *joint) const
        {
            (void)joint;
            return false;
        }

        JointType mType;
        Body *mBodyA;
        Body *mBodyB;
        bool mCollideConnected;

        friend class World;
    };

} 