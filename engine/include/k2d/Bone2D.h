#pragma once

#include "k2d/Component.h"

#include <mathc.h>

namespace k2d
{

class Bone2D : public Component
{
public:
    static const ComponentType Type = ComponentType::Bone;

    Bone2D();

    float length() const { return mLength; }
    void setLength(float length) { mLength = length > 0.0f ? length : 0.0f; }

    const Math::Vec2& restPosition() const { return mRestPosition; }
    float restRotationDegrees() const { return mRestRotationDegrees; }
    const Math::Vec2& restScale() const { return mRestScale; }

    void saveRestPose();
    void resetToRest();

protected:
    void onAwake() override;

private:
    float mLength = 50.0f;
    Math::Vec2 mRestPosition = Math::Vec2(0.0f);
    float mRestRotationDegrees = 0.0f;
    Math::Vec2 mRestScale = Math::Vec2(1.0f);
};

} // namespace k2d