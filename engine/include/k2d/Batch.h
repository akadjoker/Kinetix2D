#pragma once

#include <glm/glm.hpp>
#include <ct/vector.hpp>

#include "Matrix2D.h"

namespace k2d
{

    struct FloatRect
    {
        float x, y, width, height;

        FloatRect() : x(0), y(0), width(0), height(0) {}
        FloatRect(float x_, float y_, float w_, float h_) : x(x_), y(y_), width(w_), height(h_) {}
        void Set(float x_, float y_, float w_, float h_)
        {
            x = x_;
            y = y_;
            width = w_;
            height = h_;
        }
    };

    class BatchRenderer
    {
    public:
        struct Config
        {
            size_t maxVertices;
            size_t maxDrawCalls;
            size_t stackDepth;
            bool enableProfiling;

            Config()
                : maxVertices(32768), maxDrawCalls(256), stackDepth(32), enableProfiling(true)
            {
            }
        };

        struct Stats
        {
            size_t drawCalls;
            size_t verticesDrawn;
            size_t indicesDrawn;
            size_t textureSwitches;
            double batchTime;
            double renderTime;
            double totalTime;
            size_t batchesFlushed;
            size_t frameCount;
        };

        enum BlendMode
        {
            BLEND_ALPHA = 0,
            BLEND_ADDITIVE,
            BLEND_MULTIPLIED,
            BLEND_ADD_COLORS,
            BLEND_SUBTRACT_COLORS
        };

        BatchRenderer();
        ~BatchRenderer();

        bool Init(const Config &config = Config());
        void Shutdown();

        bool Resize(int width, int height);
        void GetWindowSize(int &width, int &height) const;

        void PushMatrix();
        void PopMatrix();
        void LoadIdentity();
        void Translate(float x, float y, float z = 0.0f);
        void Rotate(float angleDeg, float axisX, float axisY, float axisZ);
        void Scale(float x, float y, float z = 1.0f);
        void MultMatrix(const glm::mat4 &m);

        void SetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        void SetColor(float r, float g, float b, float a = 1.0f);
        void SetTextureId(unsigned int textureId);
        void SetBlendMode(BlendMode mode);
        void SetTexcoord(float u, float v);
        void SetDepthTest(bool enabled) { mDepthTestEnabled = enabled; }
        void SetDepthWrite(bool enabled) { mDepthWriteEnabled = enabled; }
        void SetBlend(bool enabled) { mBlendEnabled = enabled; }
        void SetCullFace(bool enabled) { mCullFaceEnabled = enabled; }
        void SetDefault3DState();

        void SetClipRect(float x, float y, float width, float height);
        void SetClipRect(const FloatRect &rect);
        void ClearClipRect();
        bool IsClipEnabled() const { return mClipEnabled; }
        const FloatRect &GetClipRect() const { return mClipRect; }

        void Begin(int mode);
        void End();
        void Vertex2(float x, float y);
        void Vertex3(float x, float y, float z);

        void DrawLine(float x0, float y0, float x1, float y1);
        void DrawThickLine(float x0, float y0, float x1, float y1, float thickness);
        void DrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
        void DrawRect(float x, float y, float width, float height, bool fill = true);
        void DrawQuad(float x, float y, float width, float height);
        void DrawCircle(float cx, float cy, float radius, int segments = 32);
        void DrawEllipse(float cx, float cy, float rx, float ry, bool fill = true, int segments = 32);
        void DrawArc(float cx, float cy, float radius, float startDeg, float endDeg, int segments = 32);
        void DrawRing(float cx, float cy, float rInner, float rOuter, bool fill = true, int segments = 32);
        void DrawPolygon(float cx, float cy, int sides, float radius, float rotDeg = 0.0f, bool fill = true);
        void DrawPolyline(const float *xyPairs, int pointCount);

        void DrawLine3D(float x0, float y0, float z0, float x1, float y1, float z1);
        void DrawWireBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
        void DrawWireSphere(float cx, float cy, float cz, float radius, int segments = 24);
        void DrawWireCylinder(float cx, float cy, float cz, float radius, float height, int segments = 24);
        void DrawWireCapsule(float cx, float cy, float cz, float radius, float height, int segments = 24);
        void DrawSolidBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
        void DrawSolidSphere(float cx, float cy, float cz, float radius, int rings = 12, int segments = 24);
        void DrawSolidCylinder(float cx, float cy, float cz, float radius, float height, int segments = 24);
        void DrawSolidCapsule(float cx, float cy, float cz, float radius, float height, int rings = 8, int segments = 24);
        void DrawAxis(float x, float y, float z, float size = 1.0f);
        void DrawWireGrid(float y, int slices, float spacing, bool axes = true);

        void DrawTexture(unsigned int textureId,
                         float dstX, float dstY, float dstW, float dstH,
                         float srcX = 0.0f, float srcY = 0.0f, float srcW = 0.0f, float srcH = 0.0f,
                         float pivotX = 0.0f, float pivotY = 0.0f, float rotationDeg = 0.0f);

        void DrawSprite(unsigned int textureId, float x, float y, float width, float height);

        void BeginSpriteRun(unsigned int textureId);
        void SubmitSprite(float x, float y, float width, float height);
        void EndSpriteRun();

        void DrawTexture(unsigned int textureId, const Matrix2D &matrix,
                         float width, float height, int texWidth, int texHeight,
                         float pivotX = 0.5f, float pivotY = 0.5f,
                         float srcX = 0.0f, float srcY = 0.0f, float srcW = 0.0f, float srcH = 0.0f,
                         bool flipX = false, bool flipY = false);

        void DrawRenderBatch();
        void BeginFrame();
        void EndFrame();

        void DrawText(float x, float y, float size, const char *text);
        float TextWidth(float size, const char *text) const;

        void SetProjection(const glm::mat4 &matrix);
        const glm::mat4 &GetProjection() const { return mProjection; }

        void ResetStats();
        void PrintStats() const;
        const Stats &GetStats() const { return mStats; }

        static unsigned int PackColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
        static void UnpackColor(unsigned int packed, unsigned char &r, unsigned char &g,
                                unsigned char &b, unsigned char &a);

    private:
#pragma pack(push, 1)
        struct Vertex
        {
            float x, y, z;
            float u, v;
            unsigned char r, g, b, a;
        };
#pragma pack(pop)

        struct DrawCall
        {
            int mode;
            int vertexCount;
            size_t vertexAlignment;
            unsigned int textureId;
            bool depthTest;
            bool depthWrite;
            bool blend;
            bool cullFace;
        };

        void UpdateProjection();
        void FlushBatch();
        void ApplyDrawCalls();
        bool SetupShaders();
        void SetupBuffers();
        void SetupTexture();
        void SetupFontTexture();
        void SubmitVertex(float x, float y, float z);
        void SubmitVertex(float x, float y, float z, float u, float v);
        void ApplyTransform(float &x, float &y, float &z);

        struct ClipVertex
        {
            float x, y, u, v;
        };
        int ClipPolygonToRect(const ClipVertex *in, int inCount, ClipVertex *out) const;
        bool ClipSegmentToRect(float &x0, float &y0, float &x1, float &y1) const;
        void SubmitClippedQuad(float x0, float y0, float x1, float y1,
                               float x2, float y2, float x3, float y3);
        void SubmitClippedLine(float x0, float y0, float x1, float y1);
        void EmitLine(float x0, float y0, float x1, float y1);
        void EmitTriangle(float x0, float y0, float x1, float y1, float x2, float y2);
        void EmitTexturedTriangle(float x0, float y0, float u0, float v0,
                                  float x1, float y1, float u1, float v1,
                                  float x2, float y2, float u2, float v2);

        static const int MODE_LINES = 0x0001;
        static const int MODE_TRIANGLES = 0x0004;
        static const int MODE_QUADS = 0x0007;

        Config mConfig;
        Stats mStats;

        ct::Vector<Vertex> mVertices;
        ct::Vector<unsigned short> mIndices;
        ct::Vector<DrawCall> mDrawCalls;
        ct::Vector<glm::mat4> mMatrixStack;
        glm::mat4 mCurrentMatrix;

        unsigned int mCurrentColor;
        unsigned int mCurrentTextureId;
        float mCurrentTexcoord[2];
        int mCurrentMode;
        bool mInBeginEnd;
        BlendMode mCurrentBlendMode;

        int mWindowWidth;
        int mWindowHeight;
        glm::mat4 mProjection;
        bool mDepthTestEnabled;
        bool mDepthWriteEnabled;
        bool mBlendEnabled;
        bool mCullFaceEnabled;

        bool mClipEnabled;
        FloatRect mClipRect;

        unsigned int mVAO;
        unsigned int mVBO;
        unsigned int mIBO;
        unsigned int mProgram;
        unsigned int mWhiteTexture;
        unsigned int mFontTexture;
        int mMvpLoc;
        int mTexLoc;

        long mFrameStartTime;
        unsigned char mRunR, mRunG, mRunB, mRunA;
    };

} // namespace k2d
