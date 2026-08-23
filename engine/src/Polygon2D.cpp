#include "k2d/Polygon2D.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

#include "kx/internal/triangulate.h"

namespace k2d
{

    Polygon2D::Polygon2D()
        : Component(Type, ComponentEventRender), mPolygon(), mTriangles(),
          mTexture(nullptr), mColor(0xFFFFFFFFu), mBlendMode(BLEND_MIX)
    {
    }

    void Polygon2D::setPolygon(const glm::vec2 *points, int count)
    {
        mPolygon.clear();
        mTriangles.clear();
        if (!points || count < 3)
            return;

        for (int i = 0; i < count; ++i)
            mPolygon.push_back(points[i]);

        // A simple polygon triangulates into at most count-2 triangles (the
        // fan/ear-clipping bound) -- size the scratch buffer exactly to that
        // instead of a fixed cap, so a large terrain-scale outline never
        // silently loses geometry (kx::Triangulate used to be called with a
        // fixed 256-triangle ceiling and would just stop writing past it).
        const int maxTriangles = count - 2;
        ct::Vector<glm::vec2> triangles;
        triangles.resize((size_t)maxTriangles * 3);
        int triangleCount = kx::Triangulate(mPolygon.data(), count, triangles.data(), maxTriangles);
        for (int i = 0; i < triangleCount * 3; ++i)
            mTriangles.push_back(triangles[i]);
    }

    void Polygon2D::setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        mColor = (unsigned int)r | ((unsigned int)g << 8) |
                 ((unsigned int)b << 16) | ((unsigned int)a << 24);
    }

    void Polygon2D::setTexture(Texture *texture)
    {
        mTexture = texture;
    }

    void Polygon2D::onRender(RenderQueue &queue)
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

} // namespace k2d
