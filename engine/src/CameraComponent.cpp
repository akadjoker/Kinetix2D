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

    void CameraComponent::syncFromOwner()
    {
        GameObject *object = owner();
        if (!object)
            return;
        if (!mCamera.targetEnabled || !mPositionSeeded)
        {
            mCamera.position = object->globalPosition();
            mPositionSeeded = true;
        }
        mCamera.rotationDegrees = object->rotationDegrees();
    }

    void CameraComponent::writeBackToOwner()
    {
        GameObject *object = owner();
        if (!object || !mCamera.targetEnabled)
            return;
        GameObject *parent = object->parent();
        if (parent && parent->parent())
        {
            const Math::Vec2 parentPosition = parent->globalPosition();
            object->setPosition(
                Math::Vec2(mCamera.position.x - parentPosition.x, mCamera.position.y - parentPosition.y));
        }
        else
        {
            object->setPosition(mCamera.position);
        }
    }

    void CameraComponent::onUpdate(float deltaTime)
    {
        syncFromOwner();
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
        writeBackToOwner();
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
