#pragma once

#include "k2d/Camera2D.h"
#include "k2d/Component.h"

#include <ct/string.hpp>

namespace k2d
{

    class GameObject;

    class CameraComponent : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Camera;

        CameraComponent();

        Camera2D &camera() { return mCamera; }
        const Camera2D &camera() const { return mCamera; }

        void setViewport(float width, float height);
        float viewportWidth() const { return mViewportWidth; }
        float viewportHeight() const { return mViewportHeight; }
        void setRenderPriority(int value) { mRenderPriority = value; }
        int renderPriority() const { return mRenderPriority; }
        Math::Mat4 projection() const;
        void visibleRect(float &minX, float &minY, float &maxX, float &maxY) const;
        Math::Vec2 screenToWorld(float x, float y) const;

        void setFollowTarget(const GameObject *target);
        void setFollowTargetName(const char *name)
        {
            mFollowTargetName = name ? name : "";
            mFollowTarget = nullptr;
        }
        const ct::String &followTargetName() const { return mFollowTargetName; }
        bool hasFollowTarget() const { return !mFollowTargetName.empty(); }

        // The node's transform is the camera's position/rotation, exactly like
        // any other component. Called every Play frame from onUpdate and every
        // edit-time preview tick, so dragging the node moves the camera view in
        // both modes. Position is left alone once a target is being followed -
        // Camera2D::update() then owns it (smoothing/dead zone) and the result
        // is written back onto the node below, the same way physics writes its
        // result back onto a dynamic body's transform.
        void syncFromOwner();

    protected:
        void onUpdate(float deltaTime) override;

    private:
        void writeBackToOwner();

        Camera2D mCamera;
        float mViewportWidth;
        float mViewportHeight;
        int mRenderPriority;
        ct::String mFollowTargetName;
        GameObject *mFollowTarget = nullptr;
        uint32_t mFollowVersion = 0;
        bool mPositionSeeded = false;
    };

}
