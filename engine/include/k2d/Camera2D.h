#pragma once

#include "k2d/Matrix2D.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace k2d
{

    struct Camera2D
    {
        glm::vec2 position;      
        float rotationDegrees;   
        glm::vec2 zoom;          
        glm::vec2 offset;        
        bool limitEnabled;
        glm::vec4 limits;        
        bool smoothingEnabled;
        float smoothingSpeed;
        bool deadZoneEnabled;
        glm::vec4 deadZone;      
        bool targetEnabled;
        glm::vec2 target;

        Camera2D()
            : position(0.0f), rotationDegrees(0.0f), zoom(1.0f, 1.0f), offset(0.0f),
              limitEnabled(false), limits(0.0f), smoothingEnabled(false), smoothingSpeed(5.0f),
              deadZoneEnabled(false), deadZone(0.0f), targetEnabled(false), target(0.0f) {}

        void setLimits(float left, float top, float right, float bottom)
        {
            limits = glm::vec4(left, top, right, bottom);
            limitEnabled = right > left && bottom > top;
        }

        void clearLimits() { limitEnabled = false; }

        void setSmoothing(bool enabled, float speed = 5.0f)
        {
            smoothingEnabled = enabled;
            smoothingSpeed = speed > 0.0f ? speed : 0.0f;
        }

        void setDeadZonePixels(float left, float top, float right, float bottom)
        {
            deadZone = glm::vec4(left, top, right, bottom);
            deadZoneEnabled = right > left && bottom > top;
        }

        void clearDeadZone() { deadZoneEnabled = false; }
        void setTarget(const glm::vec2 &value) { target = value; targetEnabled = true; }
        void clearTarget() { targetEnabled = false; }

        void update(float deltaTime, float screenW, float screenH)
        {
            if (!targetEnabled)
                return;

            glm::vec2 desired = position;
            glm::vec2 worldDeadZoneMin = target;
            glm::vec2 worldDeadZoneMax = target;
            if (deadZoneEnabled)
            {
                worldDeadZoneMin += glm::vec2((deadZone.x - screenW * 0.5f) / zoom.x,
                                              (deadZone.y - screenH * 0.5f) / zoom.y);
                worldDeadZoneMax += glm::vec2((deadZone.z - screenW * 0.5f) / zoom.x,
                                              (deadZone.w - screenH * 0.5f) / zoom.y);
            }
            else
            {
                worldDeadZoneMin = target;
                worldDeadZoneMax = target;
            }

            if (target.x < desired.x + worldDeadZoneMin.x - target.x)
                desired.x = target.x - (deadZoneEnabled ? (deadZone.x - screenW * 0.5f) / zoom.x : 0.0f);
            if (target.x > desired.x + worldDeadZoneMax.x - target.x)
                desired.x = target.x - (deadZoneEnabled ? (deadZone.z - screenW * 0.5f) / zoom.x : 0.0f);
            if (target.y < desired.y + worldDeadZoneMin.y - target.y)
                desired.y = target.y - (deadZoneEnabled ? (deadZone.y - screenH * 0.5f) / zoom.y : 0.0f);
            if (target.y > desired.y + worldDeadZoneMax.y - target.y)
                desired.y = target.y - (deadZoneEnabled ? (deadZone.w - screenH * 0.5f) / zoom.y : 0.0f);

            if (!smoothingEnabled || smoothingSpeed <= 0.0f || deltaTime <= 0.0f)
                position = desired;
            else
            {
                float factor = 1.0f - expf(-smoothingSpeed * deltaTime);
                position += (desired - position) * factor;
            }

            clampToLimits(screenW, screenH);
        }

        void clampToLimits(float screenW, float screenH)
        {
            if (!limitEnabled)
                return;
            float halfW = screenW * 0.5f / zoom.x;
            float halfH = screenH * 0.5f / zoom.y;
            float minCenterX = limits.x + halfW;
            float maxCenterX = limits.z - halfW;
            float minCenterY = limits.y + halfH;
            float maxCenterY = limits.w - halfH;
            position.x = minCenterX <= maxCenterX
                             ? (position.x < minCenterX ? minCenterX : (position.x > maxCenterX ? maxCenterX : position.x))
                             : (limits.x + limits.z) * 0.5f;
            position.y = minCenterY <= maxCenterY
                             ? (position.y < minCenterY ? minCenterY : (position.y > maxCenterY ? maxCenterY : position.y))
                             : (limits.y + limits.w) * 0.5f;
        }

        Matrix2D CameraXform(float screenW, float screenH) const
        {
            glm::vec2 zoomScale(1.0f / zoom.x, 1.0f / zoom.y);
            float angle = rotationDegrees * 0.01745329251f;
            glm::vec2 screenOffset = glm::vec2(screenW, screenH) * 0.5f * zoomScale;
            glm::vec2 rotated(screenOffset.x * cosf(angle) - screenOffset.y * sinf(angle),
                              screenOffset.x * sinf(angle) + screenOffset.y * cosf(angle));
            glm::vec2 origin = -rotated + position + offset;
            return Matrix2D::Translation(origin.x, origin.y) *
                   Matrix2D::Rotation(rotationDegrees) *
                   Matrix2D::Scaling(zoomScale.x, zoomScale.y);
        }

        Matrix2D ViewXform(float screenW, float screenH) const
        {
            glm::vec2 zoomScale(1.0f / zoom.x, 1.0f / zoom.y);
            float angle = rotationDegrees * 0.01745329251f;
            glm::vec2 screenOffset = glm::vec2(screenW, screenH) * 0.5f * zoomScale;
            glm::vec2 rotated(screenOffset.x * cosf(angle) - screenOffset.y * sinf(angle),
                              screenOffset.x * sinf(angle) + screenOffset.y * cosf(angle));
            glm::vec2 origin = -rotated + position + offset;
            return Matrix2D::Scaling(zoom.x, zoom.y) *
                   Matrix2D::Rotation(-rotationDegrees) *
                   Matrix2D::Translation(-origin.x, -origin.y);
        }

        glm::mat4 Projection(float screenW, float screenH) const
        {
            glm::mat4 ndc = glm::ortho(0.0f, screenW, screenH, 0.0f, -1.0f, 1.0f);
            return ndc * ViewXform(screenW, screenH).ToMat4();
        }

        glm::vec2 ScreenToWorld(float sx, float sy, float screenW, float screenH) const
        {
            return CameraXform(screenW, screenH).Transform(sx, sy);
        }

        void VisibleRect(float &minX, float &minY, float &maxX, float &maxY,
                         float screenW, float screenH) const
        {
            glm::vec2 corners[4] = {
                ScreenToWorld(0.0f, 0.0f, screenW, screenH),
                ScreenToWorld(screenW, 0.0f, screenW, screenH),
                ScreenToWorld(screenW, screenH, screenW, screenH),
                ScreenToWorld(0.0f, screenH, screenW, screenH)};
            minX = maxX = corners[0].x;
            minY = maxY = corners[0].y;
            for (int i = 1; i < 4; ++i)
            {
                if (corners[i].x < minX)
                    minX = corners[i].x;
                if (corners[i].x > maxX)
                    maxX = corners[i].x;
                if (corners[i].y < minY)
                    minY = corners[i].y;
                if (corners[i].y > maxY)
                    maxY = corners[i].y;
            }
        }
    };

}