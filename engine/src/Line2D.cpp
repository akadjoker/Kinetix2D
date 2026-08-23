#include "k2d/Line2D.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

namespace k2d
{

    Line2D::Line2D()
        : Component(Type, ComponentEventRender), mPoints(), mTriangles(),
          mWidth(4.0f), mColor(0xFFFFFFFFu), mTexture(nullptr), mClosed(false),
          mBlendMode(BLEND_MIX)
    {
    }

    void Line2D::setPoints(const glm::vec2 *points, int count)
    {
        mPoints.clear();
        if (points && count > 0)
            for (int i = 0; i < count; ++i)
                mPoints.push_back(points[i]);
        rebuild();
    }

    void Line2D::setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        mColor = (unsigned int)r | ((unsigned int)g << 8) |
                 ((unsigned int)b << 16) | ((unsigned int)a << 24);
    }

    void Line2D::setWidth(float width)
    {
        mWidth = width;
        rebuild();
    }

    void Line2D::setClosed(bool closed)
    {
        mClosed = closed;
        rebuild();
    }

    void Line2D::rebuild()
    {
        mTriangles.clear();
        int n = (int)mPoints.size();
        int segmentCount = mClosed ? n : n - 1;
        if (n < 2 || segmentCount < 1)
            return;

        float half = mWidth * 0.5f;
        for (int i = 0; i < segmentCount; ++i)
        {
            const glm::vec2 &p0 = mPoints[i];
            const glm::vec2 &p1 = mPoints[(i + 1) % n];
            glm::vec2 dir = p1 - p0;
            float len = glm::length(dir);
            if (len < 0.0001f)
                continue;
            dir /= len;
            glm::vec2 normal(-dir.y, dir.x);

            glm::vec2 a0 = p0 - dir * half + normal * half;
            glm::vec2 a1 = p0 - dir * half - normal * half;
            glm::vec2 b0 = p1 + dir * half + normal * half;
            glm::vec2 b1 = p1 + dir * half - normal * half;

            mTriangles.push_back(a1);
            mTriangles.push_back(a0);
            mTriangles.push_back(b0);
            mTriangles.push_back(a1);
            mTriangles.push_back(b0);
            mTriangles.push_back(b1);
        }
    }

    void Line2D::onRender(RenderQueue &queue)
    {
        if (mTriangles.empty())
            return;

        RenderItem &item = queue.AddItem(owner()->zIndex());
        item.xform = owner()->globalTransform();
        item.blendMode = mBlendMode;
        RenderCommand command;
        command.type = RenderCommand::kPolygon;
        command.textureId = mTexture ? mTexture->Id() : 0;
        if (mTexture)
        {
            command.texWidth = mTexture->Width();
            command.texHeight = mTexture->Height();
        }
        command.color = mColor;
        command.polygonPoints = &mTriangles;
        command.polygonPointCount = (unsigned int)mTriangles.size();
        item.commands.push_back(command);
    }

} 