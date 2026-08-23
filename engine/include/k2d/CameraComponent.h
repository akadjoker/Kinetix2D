#pragma once

#include "k2d/Camera2D.h"
#include "k2d/Component.h"

namespace k2d
{

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
        glm::mat4 projection() const;
        void visibleRect(float &minX, float &minY, float &maxX, float &maxY) const;
        glm::vec2 screenToWorld(float x, float y) const;

    private:
        Camera2D mCamera;
        float mViewportWidth;
        float mViewportHeight;
    };

}