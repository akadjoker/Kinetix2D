#include "k2d/CameraComponent.h"

#include "k2d/GameObject.h"
#include "k2d/Scene.h"

namespace k2d
{

    CameraComponent::CameraComponent()
        : Component(Type, ComponentEventUpdate), mCamera(), mViewportWidth(0.0f), mViewportHeight(0.0f),
          mRenderPriority(0)
    {
    }

    void CameraComponent::setFollowTarget(const GameObject *target)
    {
        mFollowTargetName = target ? target->name() : ct::String();
        mFollowTarget = nullptr;
    }

    void CameraComponent::onUpdate(float deltaTime)
    {
        if (!mFollowTargetName.empty() && owner() && owner()->scene())
        {
            Scene *scene = owner()->scene();
            if (!mFollowTarget || mFollowVersion != scene->topologyVersion())
            {
                mFollowTarget = scene->find(mFollowTargetName.c_str());
                mFollowVersion = scene->topologyVersion();
            }
            if (mFollowTarget)
                mCamera.setTarget(mFollowTarget->globalPosition());
        }
        mCamera.update(deltaTime, mViewportWidth, mViewportHeight);
    }

    void CameraComponent::setViewport(float width, float height)
    {
        mViewportWidth = width > 0.0f ? width : 0.0f;
        mViewportHeight = height > 0.0f ? height : 0.0f;
    }

    Math::Mat4 CameraComponent::projection() const
    {
        return mCamera.Projection(mViewportWidth, mViewportHeight);
    }

    void CameraComponent::visibleRect(float &minX, float &minY,
                                      float &maxX, float &maxY) const
    {
        mCamera.VisibleRect(minX, minY, maxX, maxY,
                            mViewportWidth, mViewportHeight);
    }

    Math::Vec2 CameraComponent::screenToWorld(float x, float y) const
    {
        return mCamera.ScreenToWorld(x, y, mViewportWidth, mViewportHeight);
    }

}
