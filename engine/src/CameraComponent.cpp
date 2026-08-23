#include "k2d/CameraComponent.h"

namespace k2d
{

    CameraComponent::CameraComponent()
        : Component(Type), mCamera(), mViewportWidth(0.0f), mViewportHeight(0.0f)
    {
    }

    void CameraComponent::setViewport(float width, float height)
    {
        mViewportWidth = width > 0.0f ? width : 0.0f;
        mViewportHeight = height > 0.0f ? height : 0.0f;
    }

    glm::mat4 CameraComponent::projection() const
    {
        return mCamera.Projection(mViewportWidth, mViewportHeight);
    }

    void CameraComponent::visibleRect(float &minX, float &minY,
                                      float &maxX, float &maxY) const
    {
        mCamera.VisibleRect(minX, minY, maxX, maxY,
                            mViewportWidth, mViewportHeight);
    }

    glm::vec2 CameraComponent::screenToWorld(float x, float y) const
    {
        return mCamera.ScreenToWorld(x, y, mViewportWidth, mViewportHeight);
    }

}