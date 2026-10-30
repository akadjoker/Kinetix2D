#include "k2d/CameraComponent.h"

#include "k2d/GameObject.h"
#include "k2d/Scene.h"

namespace k2d
{

    CameraComponent::CameraComponent()
        : Component(Type, ComponentEventUpdate), mCamera(), mViewportWidth(0.0f), mViewportHeight(0.0f),
          mRenderViewportWidth(0.0f), mRenderViewportHeight(0.0f), mViewportScaleMode(ViewportScaleMode::Fit),
          mIntegerScale(false), mRenderPriority(0)
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
        mCamera.rotationDegrees = object->globalRotationDegrees();
    }

    void CameraComponent::writeBackToOwner()
    {
        GameObject *object = owner();
        if (!object || !mCamera.targetEnabled)
            return;
        GameObject *parent = object->parent();
        if (parent && parent->parent())
            object->setPosition(parent->globalTransform().AffineInverse().Transform(mCamera.position));
        else
            object->setPosition(mCamera.position);
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
        mCamera.update(deltaTime, renderViewportWidth(), renderViewportHeight());
        writeBackToOwner();
    }

    void CameraComponent::setViewport(float width, float height)
    {
        mViewportWidth = width > 0.0f ? width : 0.0f;
        mViewportHeight = height > 0.0f ? height : 0.0f;
        mRenderViewportWidth = mViewportWidth;
        mRenderViewportHeight = mViewportHeight;
    }

    void CameraComponent::setRenderViewport(float width, float height)
    {
        mRenderViewportWidth = width > 0.0f ? width : 0.0f;
        mRenderViewportHeight = height > 0.0f ? height : 0.0f;
    }

    float CameraComponent::renderViewportWidth() const
    {
        return mRenderViewportWidth > 0.0f ? mRenderViewportWidth : mViewportWidth;
    }

    float CameraComponent::renderViewportHeight() const
    {
        return mRenderViewportHeight > 0.0f ? mRenderViewportHeight : mViewportHeight;
    }

    Math::Mat4 CameraComponent::projection() const
    {
        return mCamera.Projection(renderViewportWidth(), renderViewportHeight());
    }

    void CameraComponent::visibleRect(float &minX, float &minY,
                                      float &maxX, float &maxY) const
    {
        mCamera.VisibleRect(minX, minY, maxX, maxY,
                            renderViewportWidth(), renderViewportHeight());
    }

    Math::Vec2 CameraComponent::screenToWorld(float x, float y) const
    {
        return mCamera.ScreenToWorld(x, y, renderViewportWidth(), renderViewportHeight());
    }

}
