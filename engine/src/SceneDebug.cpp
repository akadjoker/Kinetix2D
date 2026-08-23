#include "k2d/SceneDebug.h"

#include "k2d/Batch.h"
#include "k2d/GameObject.h"
#include "k2d/SpriteComponent.h"

namespace k2d
{

    namespace
    {
        void DrawNode(BatchRenderer &batch, GameObject &node, const SceneDebugOptions &options)
        {
            Math::Vec2 origin = node.globalPosition();

            if (options.showHierarchyLines && node.parent())
            {
                Math::Vec2 parentOrigin = node.parent()->globalPosition();
                batch.SetColor((unsigned char)120, (unsigned char)120, (unsigned char)120, (unsigned char)140);
                batch.DrawLine(parentOrigin.x, parentOrigin.y, origin.x, origin.y);
            }

            if (options.showSpriteBounds)
            {
                if (SpriteComponent *sprite = node.getComponent<SpriteComponent>())
                {
                    const Math::Vec2 &size = sprite->size();
                    const Math::Vec2 &pivot = sprite->pivot();
                    batch.PushMatrix();
                    batch.MultMatrix(node.globalTransform().ToMat4());
                    batch.SetColor((unsigned char)80, (unsigned char)200, (unsigned char)255, (unsigned char)200);
                    batch.DrawRect(-pivot.x * size.x, -pivot.y * size.y, size.x, size.y, false);
                    batch.PopMatrix();
                }
            }

            if (options.showAxes)
            {
                Math::Vec2 right = node.right() * options.axisLength;
                Math::Vec2 up = node.up() * options.axisLength;
                batch.SetColor((unsigned char)255, (unsigned char)60, (unsigned char)60, (unsigned char)255);
                batch.DrawLine(origin.x, origin.y, origin.x + right.x, origin.y + right.y);
                batch.SetColor((unsigned char)60, (unsigned char)255, (unsigned char)90, (unsigned char)255);
                batch.DrawLine(origin.x, origin.y, origin.x + up.x, origin.y + up.y);
            }

            if (options.showOrigins)
            {
                batch.SetColor((unsigned char)255, (unsigned char)230, (unsigned char)60, (unsigned char)255);
                batch.DrawCircle(origin.x, origin.y, options.originRadius, 10);
            }

            if (options.showNames && !node.name().empty())
            {
                batch.SetColor((unsigned char)255, (unsigned char)255, (unsigned char)255, (unsigned char)255);
                batch.DrawText(origin.x + options.originRadius + 2.0f,
                               origin.y - options.textSize, options.textSize, node.name().c_str());
            }

            for (std::size_t i = 0; i < node.childCount(); ++i)
                DrawNode(batch, *node.child(i), options);
        }
    }

    void DrawSceneDebug(BatchRenderer &batch, GameObject &root, const SceneDebugOptions &options)
    {
        DrawNode(batch, root, options);
    }

}