#pragma once

#include "k2d/CanvasTypes.h"

#include <ct/vector.hpp>
#include <glm/glm.hpp>

namespace k2d
{

    // Low-level 2D canvas renderer. Godot's RasterizerCanvas (GLES3 backend,
    // rasterizer_canvas_gles3.cpp): owns the canvas shader and buffers and
    // draws the sorted item list the router (RenderQueue) hands it. The
    // BatchRenderer is a separate debug/immediate tool and knows nothing about
    // canvas items or lights.
    class CanvasRenderer
    {
    public:
        struct Config
        {
            size_t maxVertices;
            size_t maxDrawCalls;

            Config() : maxVertices(65532), maxDrawCalls(8192) {}
        };

        struct Stats
        {
            size_t drawCalls;
            size_t verticesDrawn;
            size_t indicesDrawn;
            size_t textureSwitches;
            size_t batchesFlushed;
        };

        CanvasRenderer();
        ~CanvasRenderer();

        bool Init(const Config &config = Config());
        void Shutdown();

        void SetProjection(const glm::mat4 &matrix);
        void SetOrtho(float width, float height);

        // Godot's Light2D "texture" property (default: a soft radial-gradient
        // cookie; renderer_canvas_render.h Light, sampled in canvas.glsl as
        // atlas_texture): shared by every point light this frame, since a real
        // per-light texture atlas is future work. Pass 0 to fall back to the
        // procedural falloff (1 - dist/radius)^2 used before this existed.
        void SetDefaultLightTexture(unsigned int textureId);

        // Godot's CanvasModulate node (canvas.glsl:716, color *=
        // canvas_modulation): darkens the UNLIT pass only -- the light loops
        // multiply by the original base color, so lights reveal true surface
        // colors inside their radius. Default white = no darkening.
        void SetCanvasModulate(float r, float g, float b);

        // Godot's per-material canvas_item shader: compiles `fragmentSource`
        // against the engine's own built-in vertex stage, so the varyings a
        // custom fragment shader can read are exactly the built-in ones --
        // `in vec2 v_texcoord; in vec4 v_color; in vec2 v_world; flat in uint
        // v_lightMask;` plus whatever uniforms it declares itself (u_texture,
        // bound at unit 0, is set up automatically same as the default
        // program; anything else -- including all lighting/shadow uniforms --
        // is NOT forwarded, unlike Godot's canvas_item shaders which inherit
        // the full light loop unless overridden. Assign the returned program
        // via Material2D::setCustomShader/SpriteComponent::setCustomShader.
        // Returns 0 on a compile/link error (logged to stdout).
        unsigned int CreateShader(const char *fragmentSource);
        void DestroyShader(unsigned int program);

        // Draws a sorted canvas item list plus the canvas lights and occluders
        // (from RenderQueue). Godot's _record_item_commands: item world xform is
        // the base, kTransform commands replace the draw transform, kRect emits
        // a textured quad. Lights are applied as uniforms in the canvas shader
        // (Godot's canvas_render_items p_lights); shadow-casting lights first
        // bake the occluders into the shadow atlas (light_update_shadow).
        void DrawItems(const RenderItem *items, size_t count,
                       const PointLight *lights, size_t lightCount,
                       const DirectionalLight *directionalLights, size_t directionalLightCount,
                       const Occluder *occluders, size_t occluderCount);

        void ResetStats();
        void PrintStats() const;
        const Stats &GetStats() const { return mStats; }

    private:
#pragma pack(push, 1)
        struct Vertex
        {
            float x, y, z;
            float u, v;
            unsigned char r, g, b, a;
            unsigned int lightMask; // Godot's CanvasItem light_mask -- see EmitQuad/EmitPolygon
        };
#pragma pack(pop)

        struct DrawCall
        {
            size_t vertexCount;
            size_t indexCount;
            size_t vertexAlignment;
            unsigned int textureId;
            unsigned int normalTextureId;
            unsigned int program; // 0 = built-in mProgram
            BlendMode blendMode;
        };

        void Flush();
        void ApplyDrawCalls();
        bool SetupShaders();
        void SetupBuffers();
        void SetupTexture();
        void SetupShadowAtlas();
        void ShutdownShadowAtlas();
        void RenderShadowAtlas();
        void EmitQuad(BlendMode blendMode, unsigned int textureId, unsigned int normalTextureId,
                      unsigned int customProgram, const Matrix2D &matrix,
                      float width, float height, int texWidth, int texHeight,
                      float pivotX, float pivotY,
                      float srcX, float srcY, float srcW, float srcH,
                      bool flipX, bool flipY, unsigned int color, unsigned int lightMask);
        void EmitPolygon(BlendMode blendMode, unsigned int textureId, unsigned int normalTextureId,
                         unsigned int customProgram, const Matrix2D &matrix,
                         const ct::Vector<glm::vec2> &points, unsigned int color, unsigned int lightMask);
        void FlattenOccluderEdges();


        Config mConfig;
        Stats mStats;

        ct::Vector<Vertex> mVertices;
        ct::Vector<unsigned short> mIndices;
        ct::Vector<DrawCall> mDrawCalls;

        unsigned int mCurrentTextureId;
        glm::mat4 mProjection;

        // World-space occluder edges flattened once per frame (FlattenOccluderEdges),
        // then rasterized into the shadow atlas per light/sector (RenderShadowAtlas).
        // Unbounded, matching Godot: it draws every LightOccluderInstance's real
        // geometry (rasterizer_canvas_gles3.cpp, light_update_shadow) rather than
        // packing edges into a fixed-size shader uniform array -- this list is
        // CPU-side only now (see u_occluderEdges removal below), so there is no
        // GLES3 uniform budget tying its size to a small constant.
        ct::Vector<glm::vec4> mOccluderEdges;

        unsigned int mVAO;
        unsigned int mVBO;
        unsigned int mIBO;
        unsigned int mProgram;
        unsigned int mShadowProgram;
        unsigned int mWhiteTexture;
        unsigned int mDefaultLightTexture;
        int mLightTextureLoc;
        int mHasLightTextureLoc;
        glm::vec4 mCanvasModulate;
        int mCanvasModulateLoc;
        int mNormalMapLoc;
        int mHasNormalMapLoc;
        int mLightHeightLoc;
        int mDirectionalHeightLoc;
        unsigned int mShadowAtlas;
        unsigned int mShadowDepth;
        unsigned int mShadowFramebuffer;
        unsigned int mShadowVAO;
        unsigned int mShadowVBO;
        int mMvpLoc;
        int mTexLoc;
        int mLightCountLoc;
        int mLightPosLoc;
        int mLightColorLoc;
        int mLightRadiusLoc;
        int mLightCullMaskLoc;
        int mDirectionalLightCountLoc;
        int mDirectionalLightDirectionLoc;
        int mDirectionalLightColorLoc;
        int mDirectionalCullMaskLoc;
        int mDirectionalLightShadowFlagsLoc;
        int mDirectionalLightShadowColorLoc;
        int mDirectionalShadowFilterLoc;
        const PointLight *mLights;
        int mLightCount;
        const DirectionalLight *mDirectionalLights;
        int mDirectionalLightCount;

        int mShadowFlagsLoc;
        int mShadowColorLoc;
        int mShadowFilterLoc;
        int mShadowAtlasLoc;
        int mCanvasSizeLoc;
        const Occluder *mOccluders;
        size_t mOccluderCount;
        float mOrthoWidth;
        float mOrthoHeight;

        // Raw, un-divided clip-space coordinates (x, y, z, w). w carries the
        // true light-local "along" distance so the GPU's own perspective
        // divide/clip does the lateral/along projection -- see RenderShadowAtlas.
        struct ShadowVertex
        {
            float x, y, z, w;
        };
        ct::Vector<ShadowVertex> mShadowVertices;
    };

} // namespace k2d
