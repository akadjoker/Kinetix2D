#pragma once

#include "k2d/Matrix2D.h"

#include <ct/vector.hpp>
#include <glm/glm.hpp>

namespace k2d
{

    // Shared canvas render data (Godot's rasterizer_canvas.h: Item/Command).
    // Consumed by the router (RenderQueue) and the low-level canvas renderer
    // (CanvasRenderer).

    // Godot's CanvasItemBlendMode (RSE::CanvasItemBlendMode): how an item's
    // color blends with the framebuffer. Applied via glBlendEquation/glBlendFunc
    // in the canvas renderer (rasterizer_canvas_gles3.cpp, _render_batch).
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

    // A 2D point light. Godot's canvas Light (renderer_canvas_render.h, Light):
    // a canvas-level entity collected by the router and passed to the canvas
    // renderer alongside the item list (canvas_render_items' p_lights). Shadow
    // fields follow the LightUniform setup (rasterizer_canvas_gles3.cpp:140).
    struct PointLight
    {
        glm::vec2 position;   // world space
        glm::vec4 color;      // rgb + energy in alpha (Godot: color * energy)
        float radius;
        bool useShadow;
        ShadowFilter shadowFilter;
        Matrix2D shadowMatrix; // world -> light-local (light global transform affine inverse)
        glm::vec4 shadowColor; // shadow tint rgb + opacity in alpha
        // Godot's Light2D range_item_cull_mask: illuminates an item only when
        // (cullMask & item's light mask) != 0. Default 1 (bit 0), matching
        // Godot's own default so nobody using a single layer notices this.
        unsigned int cullMask = 1;
        // Godot's Light2D "height": the light's Z offset above the 2D plane,
        // used only for normal-mapped lighting (canvas.glsl light_vec =
        // normalize(vec3(light.xy, height) - vec3(surface.xy, 0))). Default 0
        // matches Godot's own default -- with height 0 a perfectly flat/neutral
        // normal-mapped area gets no light at all (light_vec.z == 0), which is
        // the real, well-known Godot behavior; raise it for natural-looking
        // normal map lighting, same as Godot users are told to.
        float height = 0.0f;
    };

    struct DirectionalLight
    {
        glm::vec2 direction;
        glm::vec4 color;
        bool useShadow;
        ShadowFilter shadowFilter;
        glm::vec4 shadowColor;
        // Godot's DirectionalLight2D "height", NOT a Z offset like PointLight:
        // a 0..1 mix factor between the in-plane direction (0) and straight
        // down onto the plane, i.e. (0,0,1) (1) -- canvas.glsl light_vec =
        // normalize(mix(vec3(direction,0), vec3(0,0,1), height)).
        float height = 0.0f;
        unsigned int cullMask = 1;
    };

    // A 2D light occluder (Godot's LightOccluderInstance). A polygon in world
    // space that casts shadows into the canvas shadow atlas.
    struct Occluder
    {
        Matrix2D xform;                        // world transform
        const ct::Vector<glm::vec2> *points;   // polygon points (component-owned)
        unsigned int version;                  // bumps when points change
    };

    struct RenderCommand
    {
        enum Type : unsigned char
        {
            kTransform, // CommandTransform: matrix applied to following commands
            kRect,      // CommandRect: textured or plain quad
            kPolygon    // triangulated polygon vertices
        };

        Type type;
        unsigned int textureId;     // 0 = white 1x1 texture
        unsigned int normalTextureId; // 0 = no normal map, flat lighting (unchanged behavior)
        // 0 = the engine's built-in canvas program (unchanged behavior).
        // Otherwise a GL program from CanvasRenderer::CreateShader, sharing
        // the built-in vertex stage (v_texcoord/v_color/v_world/v_lightMask
        // all still available) with a custom fragment stage. Lighting
        // uniforms are NOT forwarded to custom programs -- see CreateShader.
        unsigned int customProgram;

        Matrix2D matrix;            // kTransform

        float x, y, width, height;      // kRect: dst rect in item-local pixels
        float srcX, srcY, srcW, srcH;   // kRect: source rect in texture pixels; 0 = full
        int texWidth, texHeight;        // kRect: texture size for source normalization
        float pivotX, pivotY;           // kRect: pivot in 0..1
        unsigned int color;             // kRect: packed RGBA
        unsigned char flags;            // kRect: bit0 flipX, bit1 flipY
        const ct::Vector<glm::vec2> *polygonPoints;
        unsigned int polygonPointCount;
        // Godot's CanvasItem light_mask: this item is lit only by lights whose
        // cullMask shares a bit with this. Default 1, matching PointLight's
        // default cullMask so nobody using a single layer notices this.
        unsigned int lightMask;

        RenderCommand()
            : type(kRect), textureId(0), normalTextureId(0), customProgram(0), matrix(),
              x(0.0f), y(0.0f), width(0.0f), height(0.0f),
              srcX(0.0f), srcY(0.0f), srcW(0.0f), srcH(0.0f),
              texWidth(0), texHeight(0), pivotX(0.5f), pivotY(0.5f),
              color(0xFFFFFFFFu), flags(0), polygonPoints(nullptr), polygonPointCount(0),
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

    // Follows Godot's canvas item (servers/rendering/renderer_canvas_render.h, Item).
    struct RenderItem
    {
        int zIndex;              // z_final
        bool ySort;              // sort_y
        float y;                 // world y used when ySort is enabled
        Matrix2D xform;          // world transform (final_transform)
        BlendMode blendMode;     // how this item blends with the framebuffer
        ct::Vector<RenderCommand> commands;
        unsigned int seq;        // submission order (stable tie-break, ysort_index)

        RenderItem()
            : zIndex(0), ySort(false), y(0.0f), xform(), blendMode(BLEND_MIX), commands(), seq(0)
        {
        }
    };

    static const int kMaxPointLights = 8;
    static const int kMaxDirectionalLights = 8;

} // namespace k2d
