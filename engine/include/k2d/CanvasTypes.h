#pragma once

#include "k2d/Matrix2D.h"
#include "k2d/Color.h"

#include <ct/string.hpp>
#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{

    enum BlendMode
    {
        BLEND_MIX = 0,
        BLEND_ADD,
        BLEND_SUB,
        BLEND_MUL
    };

    enum ShadowFilter
    {
        SHADOW_FILTER_NEAREST = 0,
        SHADOW_FILTER_PCF5 = 1,
        SHADOW_FILTER_PCF13 = 2
    };

    struct PointLight
    {
        Math::Vec2 position;
        Color color;
        float radius;
        bool useShadow;
        ShadowFilter shadowFilter;
        Matrix2D shadowMatrix;
        Color shadowColor;

        unsigned int cullMask = 1;

        float height = 0.0f;
    };

    struct DirectionalLight
    {
        Math::Vec2 direction;
        Color color;
        bool useShadow;
        ShadowFilter shadowFilter;
        Color shadowColor;

        float height = 0.0f;
        unsigned int cullMask = 1;
    };

    struct Occluder
    {
        Matrix2D xform;                        
        const ct::Vector<Math::Vec2> *points;   
        unsigned int version;                  
    };

    struct RenderCommand
    {
        enum Type : unsigned char
        {
            kTransform, 
            kRect,      
            kPolygon,
            kText
        };

        Type type;
        unsigned int textureId;     
        unsigned int normalTextureId; 

        unsigned int customProgram;

        Matrix2D matrix;            

        float x, y, width, height;      
        float srcX, srcY, srcW, srcH;   
        int texWidth, texHeight;        
        float pivotX, pivotY;
        Color color;
        unsigned char flags;
        const ct::Vector<Math::Vec2> *polygonPoints;
        unsigned int polygonPointCount;
        // Script-created polygons own their points. Native components can keep using
        // polygonPoints to avoid copying their cached geometry every frame.
        ct::Vector<Math::Vec2> ownedPolygonPoints;
        ct::String text;

        unsigned int lightMask;

        RenderCommand()
            : type(kRect), textureId(0), normalTextureId(0), customProgram(0), matrix(),
              x(0.0f), y(0.0f), width(0.0f), height(0.0f),
              srcX(0.0f), srcY(0.0f), srcW(0.0f), srcH(0.0f),
              texWidth(0), texHeight(0), pivotX(0.5f), pivotY(0.5f),
              color(0xFFFFFFFFu), flags(0), polygonPoints(nullptr), polygonPointCount(0),
              ownedPolygonPoints(), text(),
              lightMask(1)
        {
        }

        static RenderCommand MakeTransform(const Matrix2D &m)
        {
            RenderCommand c;
            c.type = kTransform;
            c.matrix = m;
            return c;
        }

        static RenderCommand MakeRect(unsigned int texture, float x, float y, float width, float height)
        {
            RenderCommand c;
            c.type = kRect;
            c.textureId = texture;
            c.x = x;
            c.y = y;
            c.width = width;
            c.height = height;
            return c;
        }
    };

    struct RenderItem
    {
        int zIndex;              
        bool ySort;              
        float y;                 
        Matrix2D xform;          
        BlendMode blendMode;     
        ct::Vector<RenderCommand> commands;
        unsigned int seq;        

        RenderItem()
            : zIndex(0), ySort(false), y(0.0f), xform(), blendMode(BLEND_MIX), commands(), seq(0)
        {
        }
    };

    static const int kMaxPointLights = 8;
    static const int kMaxDirectionalLights = 8;

}
