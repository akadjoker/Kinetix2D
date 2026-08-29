#include "k2d/GearJoint2D.h"

#include "k2d/GameObject.h"
#include "k2d/RevoluteJoint2D.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Scene.h"

#include <cmath>

namespace k2d
{

GearJoint2D::GearJoint2D()
    : mJoint1Index(0), mJoint2Index(0), mJoint1(nullptr), mJoint2(nullptr), mBodyC(nullptr), mBodyD(nullptr),
      mLocalAnchorA(0.0f, 0.0f), mLocalAnchorB(0.0f, 0.0f), mReferenceAngleA(0.0f), mReferenceAngleB(0.0f),
      mConstant(0.0f), mRatio(1.0f), mImpulse(0.0f), mMass(0.0f)
{
}

void GearJoint2D::setJointA(const char *objectName, int jointIndex)
{
    mJoint1TargetName = objectName ? objectName : "";
    mJoint1Index = jointIndex >= 0 ? jointIndex : 0;
}

void GearJoint2D::setJointB(const char *objectName, int jointIndex)
{
    mJoint2TargetName = objectName ? objectName : "";
    mJoint2Index = jointIndex >= 0 ? jointIndex : 0;
}

Math::Vec2 GearJoint2D::anchorA() const
{
    return mBodyA->GetTransform().Transform(mLocalAnchorA);
}

Math::Vec2 GearJoint2D::anchorB() const
{
    return mBodyB->GetTransform().Transform(mLocalAnchorB);
}

void GearJoint2D::resolve()
{
    GameObject *object = owner();
    Scene *scene = object ? object->scene() : nullptr;
    if (!scene || mResolvedVersion == scene->topologyVersion())
        return;

    mBodyA = mBodyB = nullptr;
    mBodyC = mBodyD = nullptr;
    mJoint1 = mJoint2 = nullptr;

    GameObject *object1 = mJoint1TargetName.empty() ? nullptr : scene->find(mJoint1TargetName.c_str());
    GameObject *object2 = mJoint2TargetName.empty() ? nullptr : scene->find(mJoint2TargetName.c_str());
    RevoluteJoint2D *joint1 = object1 ? object1->getComponentAt<RevoluteJoint2D>((std::size_t)mJoint1Index) : nullptr;
    RevoluteJoint2D *joint2 = object2 ? object2->getComponentAt<RevoluteJoint2D>((std::size_t)mJoint2Index) : nullptr;

    // Not yet resolvable (e.g. the referenced joints have not resolved their
    // own bodies in this same pass): leave mResolvedVersion alone so the next
    // call retries instead of getting stuck unconnected.
    if (!joint1 || !joint2 || !joint1->bodyA() || !joint1->bodyB() || !joint2->bodyA() || !joint2->bodyB())
        return;

    mResolvedVersion = scene->topologyVersion();
    mJoint1 = joint1;
    mJoint2 = joint2;
    mBodyA = joint1->bodyB();
    mBodyC = joint1->bodyA();
    mBodyB = joint2->bodyB();
    mBodyD = joint2->bodyA();

    mLocalAnchorA = joint1->localAnchorB();
    mLocalAnchorB = joint2->localAnchorB();
    mReferenceAngleA = joint1->referenceAngle();
    mReferenceAngleB = joint2->referenceAngle();

    float coordinateA = mBodyA->Angle() - mBodyC->Angle() - mReferenceAngleA;
    float coordinateB = mBodyB->Angle() - mBodyD->Angle() - mReferenceAngleB;
    mConstant = coordinateA + mRatio * coordinateB;
}

void GearJoint2D::initVelocity(float dt)
{
    (void)dt;

    float iA = mBodyA->InvI(), iB = mBodyB->InvI(), iC = mBodyC->InvI(), iD = mBodyD->InvI();
    float massInv = iA + iC + mRatio * mRatio * (iB + iD);
    mMass = massInv > 0.0f ? 1.0f / massInv : 0.0f;

    mBodyA->SetAngularVelocityRadians(mBodyA->AngularVelocityRadians() + iA * mImpulse);
    mBodyC->SetAngularVelocityRadians(mBodyC->AngularVelocityRadians() - iC * mImpulse);
    mBodyB->SetAngularVelocityRadians(mBodyB->AngularVelocityRadians() + iB * mImpulse * mRatio);
    mBodyD->SetAngularVelocityRadians(mBodyD->AngularVelocityRadians() - iD * mImpulse * mRatio);
}

void GearJoint2D::solveVelocity(float dt)
{
    (void)dt;

    float iA = mBodyA->InvI(), iB = mBodyB->InvI(), iC = mBodyC->InvI(), iD = mBodyD->InvI();
    float wA = mBodyA->AngularVelocityRadians(), wB = mBodyB->AngularVelocityRadians();
    float wC = mBodyC->AngularVelocityRadians(), wD = mBodyD->AngularVelocityRadians();

    float Cdot = (wA - wC) + mRatio * (wB - wD);
    float impulse = -mMass * Cdot;
    mImpulse += impulse;

    mBodyA->SetAngularVelocityRadians(wA + iA * impulse);
    mBodyC->SetAngularVelocityRadians(wC - iC * impulse);
    mBodyB->SetAngularVelocityRadians(wB + iB * impulse * mRatio);
    mBodyD->SetAngularVelocityRadians(wD - iD * impulse * mRatio);
}

bool GearJoint2D::solvePosition()
{
    float iA = mBodyA->InvI(), iB = mBodyB->InvI(), iC = mBodyC->InvI(), iD = mBodyD->InvI();
    float mass = iA + iC + mRatio * mRatio * (iB + iD);

    float coordinateA = mBodyA->Angle() - mBodyC->Angle() - mReferenceAngleA;
    float coordinateB = mBodyB->Angle() - mBodyD->Angle() - mReferenceAngleB;
    float C = (coordinateA + mRatio * coordinateB) - mConstant;

    float impulse = mass > 0.0f ? -C / mass : 0.0f;

    mBodyA->ShiftCenter(Math::Vec2(0.0f, 0.0f), iA * impulse);
    mBodyC->ShiftCenter(Math::Vec2(0.0f, 0.0f), -iC * impulse);
    mBodyB->ShiftCenter(Math::Vec2(0.0f, 0.0f), iB * impulse * mRatio);
    mBodyD->ShiftCenter(Math::Vec2(0.0f, 0.0f), -iD * impulse * mRatio);

    return std::fabs(C) < kAngularSlop;
}

}
