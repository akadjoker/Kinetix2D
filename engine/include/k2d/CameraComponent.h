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

    protected:
        void onUpdate(float deltaTime) override;

    private:
        Camera2D mCamera;
        float mViewportWidth;
        float mViewportHeight;
        int mRenderPriority;
        ct::String mFollowTargetName;
        GameObject *mFollowTarget = nullptr;
        uint32_t mFollowVersion = 0;
    };

}
