#pragma once

#include "k2d/Matrix2D.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace k2d
{

    // 2D camera as a transform, ported from Godot's Camera2D::get_camera_transform
    // (scene/2d/camera_2d.cpp): the canvas transform is the inverse of
    // T(origin) * R(angle) * S(1/zoom), anchor = drag-center, plus a screen-space
    // offset applied after rotation. Y-down screen convention.
    struct Camera2D
    {
        glm::vec2 position;      // world position the camera looks at (screen center)
        float rotationDegrees;   // camera rotation around the center
        glm::vec2 zoom;          // > 1 = zoom in (Godot semantics)
        glm::vec2 offset;        // screen-space offset applied after rotation

        Camera2D() : position(0.0f, 0.0f), rotationDegrees(0.0f), zoom(1.0f, 1.0f), offset(0.0f, 0.0f) {}

        // screen pixels -> world (the camera xform, Godot's transform before inverse)
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

        // world -> screen pixels (inverse of CameraXform)
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

        // World-space AABB of the visible area (Godot transforms the screen corners
        // through the inverse camera transform).
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
