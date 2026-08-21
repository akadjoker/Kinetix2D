#include "k2d/Polygon2D.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

#include "kx/internal/triangulate.h"

namespace k2d
{

    Polygon2D::Polygon2D()
        : Component(Type, ComponentEventRender), mPolygon(), mTriangles(),
          mTexture(nullptr), mColor(0xFFFFFFFFu)
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

        glm::vec2 triangles[3 * 256];
        int triangleCount = kx::Triangulate(mPolygon.data(), count, triangles, 256);
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
        RenderCommand command;
        command.type = RenderCommand::kPolygon;
        command.textureId = mTexture ? mTexture->Id() : 0;
        command.color = mColor;
        command.polygonPoints = &mTriangles;
        command.polygonPointCount = (unsigned int)mTriangles.size();
        item.commands.push_back(command);
    }

} // namespace k2d
