#pragma once

namespace kx
{

    class Body;

    enum class JointType : unsigned char
    {
        Distance,
        Wheel,
        Mouse
    };

    class Joint
    {
    public:
        virtual ~Joint();

        JointType Type() const { return mType; }
        Body *BodyA() const { return mBodyA; }
        Body *BodyB() const { return mBodyB; }

    protected:
        Joint(JointType type, Body *a, Body *b);

        virtual void InitVelocity(float dt) = 0;
        virtual void SolveVelocity(float dt) = 0;

        JointType mType;
        Body *mBodyA;
        Body *mBodyB;

        friend class World;
    };

} // namespace kx
