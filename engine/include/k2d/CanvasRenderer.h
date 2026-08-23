#pragma once

#include "k2d/CanvasTypes.h"

#include <ct/vector.hpp>
#include <glm/glm.hpp>

namespace k2d
{

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

        void SetDefaultLightTexture(unsigned int textureId);

        void SetCanvasModulate(float r, float g, float b);

        unsigned int CreateShader(const char *fragmentSource);
        void DestroyShader(unsigned int program);

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
            unsigned int lightMask;
        };
#pragma pack(pop)

        struct DrawCall
        {
            size_t vertexCount;
            size_t indexCount;
            size_t vertexAlignment;
            unsigned int textureId;
            unsigned int normalTextureId;
            unsigned int program;
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
                         const ct::Vector<glm::vec2> &points, unsigned int color, unsigned int lightMask,
                         int texWidth = 0, int texHeight = 0);
        void FlattenOccluderEdges();

        Config mConfig;
        Stats mStats;

        ct::Vector<Vertex> mVertices;
        ct::Vector<unsigned short> mIndices;
        ct::Vector<DrawCall> mDrawCalls;

        unsigned int mCurrentTextureId;
        glm::mat4 mProjection;

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

        struct ShadowVertex
        {
            float x, y, z, w;
        };
        ct::Vector<ShadowVertex> mShadowVertices;
    };

}