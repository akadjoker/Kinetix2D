#include "k2d/Bone2D.h"

#include "k2d/GameObject.h"

namespace k2d
{

Bone2D::Bone2D() : Component(Type) {}

void Bone2D::onAwake()
{
    saveRestPose();
}

void Bone2D::saveRestPose()
{
    if (!owner())
        return;
    mRestPosition = owner()->position();
    mRestRotationDegrees = owner()->rotationDegrees();
    mRestScale = owner()->scale();
}

void Bone2D::resetToRest()
{
    if (!owner())
        return;
    owner()->setPosition(mRestPosition);
    owner()->setRotationDegrees(mRestRotationDegrees);
    owner()->setScale(mRestScale);
}

} // namespace k2d
