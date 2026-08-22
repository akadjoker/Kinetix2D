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

        // Most joints only ever reference BodyA()/BodyB(), so World can find every
        // joint attached to a body by comparing against those two. GearJoint is the
        // exception: it also holds raw pointers to two other Body*s (the anchors of
        // its underlying revolute joints) and to those two RevoluteJoint*s themselves.
        // Override these so World::Destroy(Body*) / World::DestroyJoint(Joint*) can
        // cascade-destroy dependents instead of leaving a dangling pointer behind.
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

} // namespace kx
