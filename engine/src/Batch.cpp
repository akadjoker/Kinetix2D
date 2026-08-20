#include "k2d/Batch.h"
#include "font_data.h"

#include <glad/glad.h>
#include <ct/detail/utils.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <ctime>

namespace k2d
{

    static const int FONT_COLS = 16;
    static const int FONT_ATLAS_W = 128;
    static const int FONT_ATLAS_H = 48;

    static const float PI = 3.14159265359f;
    static const float DEG2RAD = PI / 180.0f;

    static float MinF(float a, float b) { return a < b ? a : b; }
    static float MaxF(float a, float b) { return a > b ? a : b; }

    static const char *VERTEX_SHADER_SOURCE = R"(#version 300 es
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoord;
layout(location = 2) in vec4 a_color;

uniform mat4 u_mvp;

out vec2 v_texcoord;
out vec4 v_color;

void main()
{
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
}
)";

    static const char *FRAGMENT_SHADER_SOURCE = R"(#version 300 es
precision mediump float;

in vec2 v_texcoord;
in vec4 v_color;

uniform sampler2D u_texture;

out vec4 FragColor;

void main()
{
    FragColor = texture(u_texture, v_texcoord) * v_color;
}
)";

    static GLuint CompileShader(GLenum type, const char *src)
    {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[1024];
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            std::printf("shader compile error: %s\n", log);
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    BatchRenderer::BatchRenderer()
        : mCurrentMatrix(1.0f),
          mCurrentColor(0xFFFFFFFF),
          mCurrentTextureId(0),
          mCurrentMode(MODE_TRIANGLES),
          mInBeginEnd(false),
          mCurrentBlendMode(BLEND_ALPHA),
          mWindowWidth(800),
          mWindowHeight(600),
          mProjection(1.0f),
          mDepthTestEnabled(false),
          mDepthWriteEnabled(false),
          mBlendEnabled(true),
          mCullFaceEnabled(false),
          mClipEnabled(false),
          mVAO(0),
          mVBO(0),
          mIBO(0),
          mProgram(0),
          mWhiteTexture(0),
          mFontTexture(0),
          mMvpLoc(-1),
          mTexLoc(-1),
          mFrameStartTime(0)
    {
        std::memset(&mStats, 0, sizeof(mStats));
        std::memset(mCurrentTexcoord, 0, sizeof(mCurrentTexcoord));
    }

    BatchRenderer::~BatchRenderer()
    {
        Shutdown();
    }

    bool BatchRenderer::Init(const Config &config)
    {
        mConfig = config;

        mVertices.reserve(mConfig.maxVertices);
        mIndices.reserve(mConfig.maxVertices * 2);
        mDrawCalls.reserve(mConfig.maxDrawCalls);
        mMatrixStack.reserve(mConfig.stackDepth);

        if (!SetupShaders())
            return false;
        SetupBuffers();
        SetupTexture();
        SetupFontTexture();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        UpdateProjection();
        ResetStats();

        std::printf("BatchRenderer initialized: maxVertices=%zu maxDrawCalls=%zu\n",
                    mConfig.maxVertices, mConfig.maxDrawCalls);
        return true;
    }

    void BatchRenderer::Shutdown()
    {
        if (mVBO)
        {
            glDeleteBuffers(1, &mVBO);
            mVBO = 0;
        }
        if (mIBO)
        {
            glDeleteBuffers(1, &mIBO);
            mIBO = 0;
        }
        if (mProgram)
        {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }
        if (mVAO)
        {
            glDeleteVertexArrays(1, &mVAO);
            mVAO = 0;
        }
        if (mWhiteTexture)
        {
            glDeleteTextures(1, &mWhiteTexture);
            mWhiteTexture = 0;
        }
        if (mFontTexture)
        {
            glDeleteTextures(1, &mFontTexture);
            mFontTexture = 0;
        }

        mVertices.clear();
        mIndices.clear();
        mDrawCalls.clear();
        mMatrixStack.clear();
    }

    bool BatchRenderer::Resize(int width, int height)
    {
        if (width <= 0 || height <= 0)
            return false;
        mWindowWidth = width;
        mWindowHeight = height;
        UpdateProjection();
        return true;
    }

    void BatchRenderer::GetWindowSize(int &width, int &height) const
    {
        width = mWindowWidth;
        height = mWindowHeight;
    }

    void BatchRenderer::PushMatrix()
    {
        if (mMatrixStack.size() < mConfig.stackDepth)
            mMatrixStack.push_back(mCurrentMatrix);
    }

    void BatchRenderer::PopMatrix()
    {
        if (!mMatrixStack.empty())
        {
            mCurrentMatrix = mMatrixStack.back();
            mMatrixStack.pop_back();
        }
        else
        {
            mCurrentMatrix = glm::mat4(1.0f);
        }
    }

    void BatchRenderer::LoadIdentity()
    {
        mCurrentMatrix = glm::mat4(1.0f);
    }

    void BatchRenderer::Translate(float x, float y, float z)
    {
        mCurrentMatrix = glm::translate(mCurrentMatrix, glm::vec3(x, y, z));
    }

    void BatchRenderer::Rotate(float angleDeg, float axisX, float axisY, float axisZ)
    {
        mCurrentMatrix = glm::rotate(mCurrentMatrix, angleDeg * DEG2RAD,
                                     glm::vec3(axisX, axisY, axisZ));
    }

    void BatchRenderer::Scale(float x, float y, float z)
    {
        mCurrentMatrix = glm::scale(mCurrentMatrix, glm::vec3(x, y, z));
    }

    void BatchRenderer::MultMatrix(const glm::mat4 &m)
    {
        mCurrentMatrix = mCurrentMatrix * m;
    }

    void BatchRenderer::SetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        mCurrentColor = PackColor(r, g, b, a);
    }

    void BatchRenderer::SetColor(float r, float g, float b, float a)
    {
        mCurrentColor = PackColor(
            (unsigned char)(r * 255.0f + 0.5f),
            (unsigned char)(g * 255.0f + 0.5f),
            (unsigned char)(b * 255.0f + 0.5f),
            (unsigned char)(a * 255.0f + 0.5f));
    }

    void BatchRenderer::SetTextureId(unsigned int textureId)
    {
        if (mCurrentTextureId != textureId)
        {
            mCurrentTextureId = textureId;
            if (mConfig.enableProfiling)
                mStats.textureSwitches++;
        }
    }

    void BatchRenderer::SetBlendMode(BlendMode mode)
    {
        if (mCurrentBlendMode != mode)
        {
            mCurrentBlendMode = mode;
            switch (mode)
            {
            case BLEND_ALPHA:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BLEND_ADDITIVE:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;
            case BLEND_MULTIPLIED:
                glBlendFunc(GL_DST_COLOR, GL_ZERO);
                break;
            case BLEND_ADD_COLORS:
                glBlendFunc(GL_SRC_COLOR, GL_DST_COLOR);
                break;
            case BLEND_SUBTRACT_COLORS:
                glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_DST_COLOR);
                break;
            default:
                break;
            }
        }
    }

    void BatchRenderer::SetTexcoord(float u, float v)
    {
        mCurrentTexcoord[0] = u;
        mCurrentTexcoord[1] = v;
    }

    void BatchRenderer::SetDefault3DState()
    {
        mDepthTestEnabled = true;
        mDepthWriteEnabled = true;
        mBlendEnabled = false;
        mCullFaceEnabled = false;
    }

    void BatchRenderer::SetClipRect(float x, float y, float width, float height)
    {
        mClipRect.Set(x, y, width, height);
        mClipEnabled = true;
    }

    void BatchRenderer::SetClipRect(const FloatRect &rect)
    {
        mClipRect = rect;
        mClipEnabled = true;
    }

    void BatchRenderer::ClearClipRect()
    {
        mClipEnabled = false;
    }

    int BatchRenderer::ClipPolygonToRect(const ClipVertex *in, int inCount, ClipVertex *out) const
    {
        const float minX = mClipRect.x;
        const float minY = mClipRect.y;
        const float maxX = mClipRect.x + mClipRect.width;
        const float maxY = mClipRect.y + mClipRect.height;

        ClipVertex tmp[16];
        const ClipVertex *src = in;
        int srcCount = inCount;

        for (int plane = 0; plane < 4; ++plane)
        {
            ClipVertex *dst = (plane % 2 == 0) ? tmp : out;
            int dstCount = 0;

            for (int i = 0; i < srcCount; ++i)
            {
                const ClipVertex &cur = src[i];
                const ClipVertex &prev = src[(i + srcCount - 1) % srcCount];

                bool curIn = false, prevIn = false;
                switch (plane)
                {
                case 0:
                    curIn = cur.x >= minX;
                    prevIn = prev.x >= minX;
                    break;
                case 1:
                    curIn = cur.x <= maxX;
                    prevIn = prev.x <= maxX;
                    break;
                case 2:
                    curIn = cur.y >= minY;
                    prevIn = prev.y >= minY;
                    break;
                case 3:
                    curIn = cur.y <= maxY;
                    prevIn = prev.y <= maxY;
                    break;
                }

                if (curIn != prevIn)
                {
                    float t = 0.f;
                    switch (plane)
                    {
                    case 0:
                        t = (minX - prev.x) / (cur.x - prev.x);
                        break;
                    case 1:
                        t = (maxX - prev.x) / (cur.x - prev.x);
                        break;
                    case 2:
                        t = (minY - prev.y) / (cur.y - prev.y);
                        break;
                    case 3:
                        t = (maxY - prev.y) / (cur.y - prev.y);
                        break;
                    }
                    ClipVertex isect;
                    isect.x = prev.x + t * (cur.x - prev.x);
                    isect.y = prev.y + t * (cur.y - prev.y);
                    isect.u = prev.u + t * (cur.u - prev.u);
                    isect.v = prev.v + t * (cur.v - prev.v);
                    if (dstCount < 16)
                        dst[dstCount++] = isect;
                }
                if (curIn)
                {
                    if (dstCount < 16)
                        dst[dstCount++] = cur;
                }
            }

            src = dst;
            srcCount = dstCount;
            if (srcCount == 0)
                return 0;
        }

        if (src == tmp)
        {
            for (int i = 0; i < srcCount; ++i)
                out[i] = tmp[i];
        }
        return srcCount;
    }

    bool BatchRenderer::ClipSegmentToRect(float &x0, float &y0, float &x1, float &y1) const
    {
        const float minX = mClipRect.x;
        const float minY = mClipRect.y;
        const float maxX = mClipRect.x + mClipRect.width;
        const float maxY = mClipRect.y + mClipRect.height;

        float dx = x1 - x0;
        float dy = y1 - y0;
        float tMin = 0.f, tMax = 1.f;

        float p[4] = {-dx, dx, -dy, dy};
        float q[4] = {x0 - minX, maxX - x0, y0 - minY, maxY - y0};

        for (int i = 0; i < 4; ++i)
        {
            if (p[i] == 0.f)
            {
                if (q[i] < 0.f)
                    return false;
            }
            else
            {
                float t = q[i] / p[i];
                if (p[i] < 0.f)
                    tMin = MaxF(tMin, t);
                else
                    tMax = MinF(tMax, t);
            }
        }

        if (tMin > tMax)
            return false;

        float nx0 = x0 + tMin * dx, ny0 = y0 + tMin * dy;
        float nx1 = x0 + tMax * dx, ny1 = y0 + tMax * dy;
        x0 = nx0;
        y0 = ny0;
        x1 = nx1;
        y1 = ny1;
        return true;
    }

    void BatchRenderer::SubmitClippedQuad(float x0, float y0, float x1, float y1,
                                          float x2, float y2, float x3, float y3)
    {
        ClipVertex in[4] = {
            {x0, y0, 0.f, 0.f},
            {x1, y1, 1.f, 0.f},
            {x2, y2, 1.f, 1.f},
            {x3, y3, 0.f, 1.f},
        };
        ClipVertex out[16];
        int n = ClipPolygonToRect(in, 4, out);
        if (n < 3)
            return;

        for (int i = 1; i + 1 < n; ++i)
        {
            SubmitVertex(out[0].x, out[0].y, 0.f, out[0].u, out[0].v);
            SubmitVertex(out[i].x, out[i].y, 0.f, out[i].u, out[i].v);
            SubmitVertex(out[i + 1].x, out[i + 1].y, 0.f, out[i + 1].u, out[i + 1].v);
        }
    }

    void BatchRenderer::SubmitClippedLine(float x0, float y0, float x1, float y1)
    {
        if (!ClipSegmentToRect(x0, y0, x1, y1))
            return;
        SubmitVertex(x0, y0, 0.f);
        SubmitVertex(x1, y1, 0.f);
    }

    void BatchRenderer::EmitLine(float x0, float y0, float x1, float y1)
    {
        if (mClipEnabled)
        {
            float z0 = 0.f, z1 = 0.f;
            ApplyTransform(x0, y0, z0);
            ApplyTransform(x1, y1, z1);
            SubmitClippedLine(x0, y0, x1, y1);
        }
        else
        {
            Vertex2(x0, y0);
            Vertex2(x1, y1);
        }
    }

    void BatchRenderer::EmitTriangle(float x0, float y0, float x1, float y1, float x2, float y2)
    {
        EmitTexturedTriangle(x0, y0, 0.f, 0.f, x1, y1, 0.f, 0.f, x2, y2, 0.f, 0.f);
    }

    void BatchRenderer::EmitTexturedTriangle(float x0, float y0, float u0, float v0,
                                             float x1, float y1, float u1, float v1,
                                             float x2, float y2, float u2, float v2)
    {
        if (mClipEnabled)
        {
            float z0 = 0.f, z1 = 0.f, z2 = 0.f;
            ApplyTransform(x0, y0, z0);
            ApplyTransform(x1, y1, z1);
            ApplyTransform(x2, y2, z2);

            ClipVertex in[3] = {{x0, y0, u0, v0}, {x1, y1, u1, v1}, {x2, y2, u2, v2}};
            ClipVertex out[16];
            int n = ClipPolygonToRect(in, 3, out);
            for (int i = 1; i + 1 < n; ++i)
            {
                SubmitVertex(out[0].x, out[0].y, 0.f, out[0].u, out[0].v);
                SubmitVertex(out[i].x, out[i].y, 0.f, out[i].u, out[i].v);
                SubmitVertex(out[i + 1].x, out[i + 1].y, 0.f, out[i + 1].u, out[i + 1].v);
            }
        }
        else
        {
            SetTexcoord(u0, v0);
            Vertex2(x0, y0);
            SetTexcoord(u1, v1);
            Vertex2(x1, y1);
            SetTexcoord(u2, v2);
            Vertex2(x2, y2);
        }
    }

    void BatchRenderer::Vertex2(float x, float y)
    {
        Vertex3(x, y, 0.0f);
    }

    void BatchRenderer::Vertex3(float x, float y, float z)
    {
        if (!mInBeginEnd)
            return;

        ApplyTransform(x, y, z);

        Vertex v;
        v.x = x;
        v.y = y;
        v.z = z;
        v.u = mCurrentTexcoord[0];
        v.v = mCurrentTexcoord[1];
        UnpackColor(mCurrentColor, v.r, v.g, v.b, v.a);

        mVertices.push_back(v);

        if (mVertices.size() >= mConfig.maxVertices)
            FlushBatch();
    }

    void BatchRenderer::Begin(int mode)
    {
        if (mInBeginEnd)
        {
            std::printf("warning: Begin() without End()\n");
            End();
        }

        mInBeginEnd = true;
        mCurrentMode = mode;

        if (mDrawCalls.empty() ||
            mDrawCalls.back().mode != mode ||
            mDrawCalls.back().textureId != mCurrentTextureId ||
            mDrawCalls.back().depthTest != mDepthTestEnabled ||
            mDrawCalls.back().depthWrite != mDepthWriteEnabled ||
            mDrawCalls.back().blend != mBlendEnabled ||
            mDrawCalls.back().cullFace != mCullFaceEnabled)
        {
            DrawCall call;
            call.mode = mode;
            call.vertexCount = 0;
            call.vertexAlignment = mVertices.size();
            call.textureId = mCurrentTextureId;
            call.depthTest = mDepthTestEnabled;
            call.depthWrite = mDepthWriteEnabled;
            call.blend = mBlendEnabled;
            call.cullFace = mCullFaceEnabled;
            mDrawCalls.push_back(call);
        }
    }

    void BatchRenderer::End()
    {
        if (!mInBeginEnd)
            return;

        mInBeginEnd = false;

        if (!mDrawCalls.empty())
            mDrawCalls.back().vertexCount = (int)(mVertices.size() - mDrawCalls.back().vertexAlignment);
    }

    void BatchRenderer::DrawLine(float x0, float y0, float x1, float y1)
    {
        SetTextureId(mWhiteTexture);
        Begin(MODE_LINES);
        if (mClipEnabled)
        {
            float z = 0.f;
            ApplyTransform(x0, y0, z);
            z = 0.f;
            ApplyTransform(x1, y1, z);
            SubmitClippedLine(x0, y0, x1, y1);
        }
        else
        {
            Vertex2(x0, y0);
            Vertex2(x1, y1);
        }
        End();
    }

    void BatchRenderer::DrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
    {
        SetTextureId(mWhiteTexture);
        Begin(MODE_TRIANGLES);
        EmitTriangle(x1, y1, x2, y2, x3, y3);
        End();
    }

    void BatchRenderer::DrawRect(float x, float y, float width, float height, bool fill)
    {
        SetTextureId(mWhiteTexture);
        if (fill)
        {
            Begin(MODE_TRIANGLES);
            if (mClipEnabled)
            {
                float x0 = x, y0 = y, z0 = 0.f;
                float x1 = x + width, y1 = y, z1 = 0.f;
                float x2 = x + width, y2 = y + height, z2 = 0.f;
                float x3 = x, y3 = y + height, z3 = 0.f;
                ApplyTransform(x0, y0, z0);
                ApplyTransform(x1, y1, z1);
                ApplyTransform(x2, y2, z2);
                ApplyTransform(x3, y3, z3);
                SubmitClippedQuad(x0, y0, x1, y1, x2, y2, x3, y3);
            }
            else
            {
                Vertex2(x, y);
                Vertex2(x + width, y);
                Vertex2(x, y + height);
                Vertex2(x + width, y);
                Vertex2(x + width, y + height);
                Vertex2(x, y + height);
            }
            End();
        }
        else
        {
            Begin(MODE_LINES);
            if (mClipEnabled)
            {
                float corners[4][2] = {
                    {x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}};
                for (int i = 0; i < 4; ++i)
                {
                    float x0 = corners[i][0], y0 = corners[i][1], z0 = 0.f;
                    float x1 = corners[(i + 1) % 4][0], y1 = corners[(i + 1) % 4][1], z1 = 0.f;
                    ApplyTransform(x0, y0, z0);
                    ApplyTransform(x1, y1, z1);
                    SubmitClippedLine(x0, y0, x1, y1);
                }
            }
            else
            {
                Vertex2(x, y);
                Vertex2(x + width, y);
                Vertex2(x + width, y);
                Vertex2(x + width, y + height);
                Vertex2(x + width, y + height);
                Vertex2(x, y + height);
                Vertex2(x, y + height);
                Vertex2(x, y);
            }
            End();
        }
    }

    void BatchRenderer::DrawQuad(float x, float y, float width, float height)
    {
        SetTextureId(mWhiteTexture);
        Begin(MODE_QUADS);
        Vertex2(x, y);
        Vertex2(x + width, y);
        Vertex2(x + width, y + height);
        Vertex2(x, y + height);
        End();
    }

    void BatchRenderer::DrawCircle(float cx, float cy, float radius, int segments)
    {
        SetTextureId(mWhiteTexture);
        Begin(MODE_LINES);
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = (float)i * 2.0f * PI / (float)segments;
            float angle2 = (float)(i + 1) * 2.0f * PI / (float)segments;

            float x1 = cx + cosf(angle1) * radius;
            float y1 = cy + sinf(angle1) * radius;
            float x2 = cx + cosf(angle2) * radius;
            float y2 = cy + sinf(angle2) * radius;

            EmitLine(x1, y1, x2, y2);
        }
        End();
    }

    void BatchRenderer::DrawPolyline(const float *xyPairs, int pointCount)
    {
        if (!xyPairs || pointCount < 2)
            return;

        SetTextureId(mWhiteTexture);
        Begin(MODE_LINES);
        for (int i = 0; i < pointCount - 1; ++i)
            EmitLine(xyPairs[i * 2], xyPairs[i * 2 + 1], xyPairs[(i + 1) * 2], xyPairs[(i + 1) * 2 + 1]);
        End();
    }

    void BatchRenderer::DrawThickLine(float x0, float y0, float x1, float y1, float thickness)
    {
        float dx = x1 - x0, dy = y1 - y0;
        float len = sqrtf(dx * dx + dy * dy);
        if (len <= 0.0f)
            return;
        float nx = -dy / len * thickness * 0.5f;
        float ny = dx / len * thickness * 0.5f;
        SetTextureId(mWhiteTexture);
        Begin(MODE_TRIANGLES);
        EmitTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 - nx, y1 - ny);
        EmitTriangle(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny);
        End();
    }

    void BatchRenderer::DrawEllipse(float cx, float cy, float rx, float ry, bool fill, int segments)
    {
        if (segments < 3)
            segments = 3;
        float step = 2.0f * PI / (float)segments;
        SetTextureId(mWhiteTexture);
        if (fill)
        {
            Begin(MODE_TRIANGLES);
            for (int i = 0; i < segments; ++i)
            {
                float a0 = i * step, a1 = (i + 1) * step;
                EmitTriangle(cx, cy,
                             cx + rx * cosf(a0), cy + ry * sinf(a0),
                             cx + rx * cosf(a1), cy + ry * sinf(a1));
            }
            End();
        }
        else
        {
            Begin(MODE_LINES);
            for (int i = 0; i < segments; ++i)
            {
                float a0 = i * step, a1 = (i + 1) * step;
                EmitLine(cx + rx * cosf(a0), cy + ry * sinf(a0),
                         cx + rx * cosf(a1), cy + ry * sinf(a1));
            }
            End();
        }
    }

    void BatchRenderer::DrawArc(float cx, float cy, float radius, float startDeg, float endDeg, int segments)
    {
        if (segments < 1)
            segments = 1;
        float a0 = startDeg * DEG2RAD;
        float step = (endDeg - startDeg) * DEG2RAD / (float)segments;
        SetTextureId(mWhiteTexture);
        Begin(MODE_LINES);
        for (int i = 0; i < segments; ++i)
        {
            float t0 = a0 + i * step, t1 = a0 + (i + 1) * step;
            EmitLine(cx + radius * cosf(t0), cy + radius * sinf(t0),
                     cx + radius * cosf(t1), cy + radius * sinf(t1));
        }
        End();
    }

    void BatchRenderer::DrawRing(float cx, float cy, float rInner, float rOuter, bool fill, int segments)
    {
        if (segments < 3)
            segments = 3;
        float step = 2.0f * PI / (float)segments;
        SetTextureId(mWhiteTexture);
        if (fill)
        {
            Begin(MODE_TRIANGLES);
            for (int i = 0; i < segments; ++i)
            {
                float a0 = i * step, a1 = (i + 1) * step;
                float ix0 = cx + rInner * cosf(a0), iy0 = cy + rInner * sinf(a0);
                float ox0 = cx + rOuter * cosf(a0), oy0 = cy + rOuter * sinf(a0);
                float ix1 = cx + rInner * cosf(a1), iy1 = cy + rInner * sinf(a1);
                float ox1 = cx + rOuter * cosf(a1), oy1 = cy + rOuter * sinf(a1);
                EmitTriangle(ix0, iy0, ox0, oy0, ox1, oy1);
                EmitTriangle(ix0, iy0, ox1, oy1, ix1, iy1);
            }
            End();
        }
        else
        {
            DrawCircle(cx, cy, rInner, segments);
            DrawCircle(cx, cy, rOuter, segments);
        }
    }

    void BatchRenderer::DrawPolygon(float cx, float cy, int sides, float radius, float rotDeg, bool fill)
    {
        if (sides < 3)
            sides = 3;
        float step = 2.0f * PI / (float)sides;
        float rot = rotDeg * DEG2RAD;
        SetTextureId(mWhiteTexture);
        if (fill)
        {
            Begin(MODE_TRIANGLES);
            for (int i = 0; i < sides; ++i)
            {
                float a0 = rot + i * step, a1 = rot + (i + 1) * step;
                EmitTriangle(cx, cy,
                             cx + radius * cosf(a0), cy + radius * sinf(a0),
                             cx + radius * cosf(a1), cy + radius * sinf(a1));
            }
            End();
        }
        else
        {
            Begin(MODE_LINES);
            for (int i = 0; i < sides; ++i)
            {
                float a0 = rot + i * step, a1 = rot + (i + 1) * step;
                EmitLine(cx + radius * cosf(a0), cy + radius * sinf(a0),
                         cx + radius * cosf(a1), cy + radius * sinf(a1));
            }
            End();
        }
    }

    void BatchRenderer::DrawLine3D(float x0, float y0, float z0, float x1, float y1, float z1)
    {
        SetTextureId(mWhiteTexture);
        Begin(MODE_LINES);
        Vertex3(x0, y0, z0);
        Vertex3(x1, y1, z1);
        End();
    }

    void BatchRenderer::DrawWireBox(float minX, float minY, float minZ,
                                    float maxX, float maxY, float maxZ)
    {
        SetTextureId(mWhiteTexture);
        Begin(MODE_LINES);
        Vertex3(minX, minY, minZ);
        Vertex3(maxX, minY, minZ);
        Vertex3(maxX, minY, minZ);
        Vertex3(maxX, minY, maxZ);
        Vertex3(maxX, minY, maxZ);
        Vertex3(minX, minY, maxZ);
        Vertex3(minX, minY, maxZ);
        Vertex3(minX, minY, minZ);
        Vertex3(minX, maxY, minZ);
        Vertex3(maxX, maxY, minZ);
        Vertex3(maxX, maxY, minZ);
        Vertex3(maxX, maxY, maxZ);
        Vertex3(maxX, maxY, maxZ);
        Vertex3(minX, maxY, maxZ);
        Vertex3(minX, maxY, maxZ);
        Vertex3(minX, maxY, minZ);
        Vertex3(minX, minY, minZ);
        Vertex3(minX, maxY, minZ);
        Vertex3(maxX, minY, minZ);
        Vertex3(maxX, maxY, minZ);
        Vertex3(maxX, minY, maxZ);
        Vertex3(maxX, maxY, maxZ);
        Vertex3(minX, minY, maxZ);
        Vertex3(minX, maxY, maxZ);
        End();
    }

    void BatchRenderer::DrawWireSphere(float cx, float cy, float cz, float radius, int segments)
    {
        float step = 2.0f * PI / (float)segments;
        SetTextureId(mWhiteTexture);
        Begin(MODE_LINES);
        for (int i = 0; i < segments; ++i)
        {
            float a0 = i * step, a1 = (i + 1) * step;
            Vertex3(cx + radius * cosf(a0), cy + radius * sinf(a0), cz);
            Vertex3(cx + radius * cosf(a1), cy + radius * sinf(a1), cz);
            Vertex3(cx + radius * cosf(a0), cy, cz + radius * sinf(a0));
            Vertex3(cx + radius * cosf(a1), cy, cz + radius * sinf(a1));
            Vertex3(cx, cy + radius * sinf(a0), cz + radius * cosf(a0));
            Vertex3(cx, cy + radius * sinf(a1), cz + radius * cosf(a1));
        }
        End();
    }

    void BatchRenderer::DrawWireCylinder(float cx, float cy, float cz, float radius, float height, int segments)
    {
        float halfH = height * 0.5f;
        float step = 2.0f * PI / (float)segments;
        SetTextureId(mWhiteTexture);
        Begin(MODE_LINES);
        for (int i = 0; i < segments; ++i)
        {
            float a0 = i * step, a1 = (i + 1) * step;
            float x0 = radius * cosf(a0), z0 = radius * sinf(a0);
            float x1 = radius * cosf(a1), z1 = radius * sinf(a1);
            Vertex3(cx + x0, cy - halfH, cz + z0);
            Vertex3(cx + x1, cy - halfH, cz + z1);
            Vertex3(cx + x0, cy + halfH, cz + z0);
            Vertex3(cx + x1, cy + halfH, cz + z1);
        }
        for (int i = 0; i < 4; ++i)
        {
            float a = i * (PI * 0.5f);
            Vertex3(cx + radius * cosf(a), cy - halfH, cz + radius * sinf(a));
            Vertex3(cx + radius * cosf(a), cy + halfH, cz + radius * sinf(a));
        }
        End();
    }

    void BatchRenderer::DrawWireCapsule(float cx, float cy, float cz, float radius, float height, int segments)
    {
        float halfH = height * 0.5f;
        float step = 2.0f * PI / (float)segments;
        int hsegs = segments / 2;
        float hstep = PI / (float)hsegs;

        SetTextureId(mWhiteTexture);
        Begin(MODE_LINES);
        for (int i = 0; i < segments; ++i)
        {
            float a0 = i * step, a1 = (i + 1) * step;
            Vertex3(cx + radius * cosf(a0), cy - halfH, cz + radius * sinf(a0));
            Vertex3(cx + radius * cosf(a1), cy - halfH, cz + radius * sinf(a1));
            Vertex3(cx + radius * cosf(a0), cy + halfH, cz + radius * sinf(a0));
            Vertex3(cx + radius * cosf(a1), cy + halfH, cz + radius * sinf(a1));
        }
        for (int i = 0; i < 4; ++i)
        {
            float a = i * (PI * 0.5f);
            Vertex3(cx + radius * cosf(a), cy - halfH, cz + radius * sinf(a));
            Vertex3(cx + radius * cosf(a), cy + halfH, cz + radius * sinf(a));
        }
        for (int i = 0; i < hsegs; ++i)
        {
            float t0 = i * hstep, t1 = (i + 1) * hstep;
            Vertex3(cx + radius * cosf(t0), cy + halfH + radius * sinf(t0), cz);
            Vertex3(cx + radius * cosf(t1), cy + halfH + radius * sinf(t1), cz);
            Vertex3(cx, cy + halfH + radius * sinf(t0), cz + radius * cosf(t0));
            Vertex3(cx, cy + halfH + radius * sinf(t1), cz + radius * cosf(t1));
        }
        for (int i = 0; i < hsegs; ++i)
        {
            float t0 = i * hstep, t1 = (i + 1) * hstep;
            Vertex3(cx + radius * cosf(t0), cy - halfH - radius * sinf(t0), cz);
            Vertex3(cx + radius * cosf(t1), cy - halfH - radius * sinf(t1), cz);
            Vertex3(cx, cy - halfH - radius * sinf(t0), cz + radius * cosf(t0));
            Vertex3(cx, cy - halfH - radius * sinf(t1), cz + radius * cosf(t1));
        }
        End();
    }

    void BatchRenderer::DrawSolidBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
    {
        SetTextureId(mWhiteTexture);
        Begin(MODE_TRIANGLES);
        Vertex3(minX, minY, minZ);
        Vertex3(maxX, minY, maxZ);
        Vertex3(maxX, minY, minZ);
        Vertex3(minX, minY, minZ);
        Vertex3(minX, minY, maxZ);
        Vertex3(maxX, minY, maxZ);
        Vertex3(minX, maxY, minZ);
        Vertex3(maxX, maxY, minZ);
        Vertex3(maxX, maxY, maxZ);
        Vertex3(minX, maxY, minZ);
        Vertex3(maxX, maxY, maxZ);
        Vertex3(minX, maxY, maxZ);
        Vertex3(minX, minY, minZ);
        Vertex3(maxX, maxY, minZ);
        Vertex3(minX, maxY, minZ);
        Vertex3(minX, minY, minZ);
        Vertex3(maxX, minY, minZ);
        Vertex3(maxX, maxY, minZ);
        Vertex3(minX, minY, maxZ);
        Vertex3(minX, maxY, maxZ);
        Vertex3(maxX, maxY, maxZ);
        Vertex3(minX, minY, maxZ);
        Vertex3(maxX, maxY, maxZ);
        Vertex3(maxX, minY, maxZ);
        Vertex3(minX, minY, minZ);
        Vertex3(minX, maxY, maxZ);
        Vertex3(minX, minY, maxZ);
        Vertex3(minX, minY, minZ);
        Vertex3(minX, maxY, minZ);
        Vertex3(minX, maxY, maxZ);
        Vertex3(maxX, minY, minZ);
        Vertex3(maxX, minY, maxZ);
        Vertex3(maxX, maxY, maxZ);
        Vertex3(maxX, minY, minZ);
        Vertex3(maxX, maxY, maxZ);
        Vertex3(maxX, maxY, minZ);
        End();
    }

    void BatchRenderer::DrawSolidSphere(float cx, float cy, float cz, float radius, int rings, int segments)
    {
        float rstep = PI / (float)rings;
        float sstep = 2.0f * PI / (float)segments;
        SetTextureId(mWhiteTexture);
        Begin(MODE_TRIANGLES);
        for (int r = 0; r < rings; ++r)
        {
            float phi0 = r * rstep, phi1 = (r + 1) * rstep;
            for (int s = 0; s < segments; ++s)
            {
                float th0 = s * sstep, th1 = (s + 1) * sstep;
                float x00 = radius * sinf(phi0) * cosf(th0), y00 = radius * cosf(phi0), z00 = radius * sinf(phi0) * sinf(th0);
                float x10 = radius * sinf(phi1) * cosf(th0), y10 = radius * cosf(phi1), z10 = radius * sinf(phi1) * sinf(th0);
                float x01 = radius * sinf(phi0) * cosf(th1), y01 = radius * cosf(phi0), z01 = radius * sinf(phi0) * sinf(th1);
                float x11 = radius * sinf(phi1) * cosf(th1), y11 = radius * cosf(phi1), z11 = radius * sinf(phi1) * sinf(th1);
                Vertex3(cx + x00, cy + y00, cz + z00);
                Vertex3(cx + x10, cy + y10, cz + z10);
                Vertex3(cx + x11, cy + y11, cz + z11);
                Vertex3(cx + x00, cy + y00, cz + z00);
                Vertex3(cx + x11, cy + y11, cz + z11);
                Vertex3(cx + x01, cy + y01, cz + z01);
            }
        }
        End();
    }

    void BatchRenderer::DrawSolidCylinder(float cx, float cy, float cz, float radius, float height, int segments)
    {
        float halfH = height * 0.5f;
        float step = 2.0f * PI / (float)segments;
        SetTextureId(mWhiteTexture);
        Begin(MODE_TRIANGLES);
        for (int i = 0; i < segments; ++i)
        {
            float a0 = i * step, a1 = (i + 1) * step;
            float x0 = radius * cosf(a0), z0 = radius * sinf(a0);
            float x1 = radius * cosf(a1), z1 = radius * sinf(a1);
            Vertex3(cx + x0, cy - halfH, cz + z0);
            Vertex3(cx + x1, cy - halfH, cz + z1);
            Vertex3(cx + x1, cy + halfH, cz + z1);
            Vertex3(cx + x0, cy - halfH, cz + z0);
            Vertex3(cx + x1, cy + halfH, cz + z1);
            Vertex3(cx + x0, cy + halfH, cz + z0);
            Vertex3(cx, cy - halfH, cz);
            Vertex3(cx + x1, cy - halfH, cz + z1);
            Vertex3(cx + x0, cy - halfH, cz + z0);
            Vertex3(cx, cy + halfH, cz);
            Vertex3(cx + x0, cy + halfH, cz + z0);
            Vertex3(cx + x1, cy + halfH, cz + z1);
        }
        End();
    }

    void BatchRenderer::DrawSolidCapsule(float cx, float cy, float cz, float radius, float height, int rings, int segments)
    {
        float halfH = height * 0.5f;
        float sstep = 2.0f * PI / (float)segments;
        float hstep = (PI * 0.5f) / (float)rings;

        SetTextureId(mWhiteTexture);
        Begin(MODE_TRIANGLES);
        for (int i = 0; i < segments; ++i)
        {
            float a0 = i * sstep, a1 = (i + 1) * sstep;
            float x0 = radius * cosf(a0), z0 = radius * sinf(a0);
            float x1 = radius * cosf(a1), z1 = radius * sinf(a1);
            Vertex3(cx + x0, cy - halfH, cz + z0);
            Vertex3(cx + x1, cy - halfH, cz + z1);
            Vertex3(cx + x1, cy + halfH, cz + z1);
            Vertex3(cx + x0, cy - halfH, cz + z0);
            Vertex3(cx + x1, cy + halfH, cz + z1);
            Vertex3(cx + x0, cy + halfH, cz + z0);
        }
        for (int r = 0; r < rings; ++r)
        {
            float t0 = r * hstep, t1 = (r + 1) * hstep;
            for (int s = 0; s < segments; ++s)
            {
                float a0 = s * sstep, a1 = (s + 1) * sstep;
                float x00 = radius * sinf(t0) * cosf(a0), y00 = radius * cosf(t0), z00 = radius * sinf(t0) * sinf(a0);
                float x10 = radius * sinf(t1) * cosf(a0), y10 = radius * cosf(t1), z10 = radius * sinf(t1) * sinf(a0);
                float x01 = radius * sinf(t0) * cosf(a1), y01 = radius * cosf(t0), z01 = radius * sinf(t0) * sinf(a1);
                float x11 = radius * sinf(t1) * cosf(a1), y11 = radius * cosf(t1), z11 = radius * sinf(t1) * sinf(a1);
                Vertex3(cx + x00, cy + halfH + y00, cz + z00);
                Vertex3(cx + x10, cy + halfH + y10, cz + z10);
                Vertex3(cx + x11, cy + halfH + y11, cz + z11);
                Vertex3(cx + x00, cy + halfH + y00, cz + z00);
                Vertex3(cx + x11, cy + halfH + y11, cz + z11);
                Vertex3(cx + x01, cy + halfH + y01, cz + z01);
            }
        }
        for (int r = 0; r < rings; ++r)
        {
            float t0 = PI * 0.5f + r * hstep, t1 = PI * 0.5f + (r + 1) * hstep;
            for (int s = 0; s < segments; ++s)
            {
                float a0 = s * sstep, a1 = (s + 1) * sstep;
                float x00 = radius * sinf(t0) * cosf(a0), y00 = radius * cosf(t0), z00 = radius * sinf(t0) * sinf(a0);
                float x10 = radius * sinf(t1) * cosf(a0), y10 = radius * cosf(t1), z10 = radius * sinf(t1) * sinf(a0);
                float x01 = radius * sinf(t0) * cosf(a1), y01 = radius * cosf(t0), z01 = radius * sinf(t0) * sinf(a1);
                float x11 = radius * sinf(t1) * cosf(a1), y11 = radius * cosf(t1), z11 = radius * sinf(t1) * sinf(a1);
                Vertex3(cx + x00, cy - halfH + y00, cz + z00);
                Vertex3(cx + x10, cy - halfH + y10, cz + z10);
                Vertex3(cx + x11, cy - halfH + y11, cz + z11);
                Vertex3(cx + x00, cy - halfH + y00, cz + z00);
                Vertex3(cx + x11, cy - halfH + y11, cz + z11);
                Vertex3(cx + x01, cy - halfH + y01, cz + z01);
            }
        }
        End();
    }

    void BatchRenderer::DrawAxis(float x, float y, float z, float size)
    {
        unsigned int savedColor = mCurrentColor;
        SetTextureId(mWhiteTexture);

        SetColor((unsigned char)255, (unsigned char)0, (unsigned char)0, (unsigned char)255);
        Begin(MODE_LINES);
        Vertex3(x, y, z);
        Vertex3(x + size, y, z);
        End();
        SetColor((unsigned char)0, (unsigned char)255, (unsigned char)0, (unsigned char)255);
        Begin(MODE_LINES);
        Vertex3(x, y, z);
        Vertex3(x, y + size, z);
        End();
        SetColor((unsigned char)0, (unsigned char)0, (unsigned char)255, (unsigned char)255);
        Begin(MODE_LINES);
        Vertex3(x, y, z);
        Vertex3(x, y, z + size);
        End();

        mCurrentColor = savedColor;
    }

    void BatchRenderer::DrawWireGrid(float y, int slices, float spacing, bool axes)
    {
        slices = slices < 1 ? 1 : slices;
        const float half = (float)slices * spacing * 0.5f;

        SetTextureId(mWhiteTexture);
        Begin(MODE_LINES);
        for (int i = 0; i <= slices; ++i)
        {
            const float offset = -half + (float)i * spacing;
            const bool center = axes && fabsf(offset) < 0.0001f;

            if (center)
                SetColor(0.8f, 0.2f, 0.2f, 1.0f);
            else
                SetColor(0.45f, 0.45f, 0.45f, 1.0f);
            Vertex3(offset, y, -half);
            Vertex3(offset, y, half);

            if (center)
                SetColor(0.2f, 0.8f, 0.2f, 1.0f);
            else
                SetColor(0.45f, 0.45f, 0.45f, 1.0f);
            Vertex3(-half, y, offset);
            Vertex3(half, y, offset);
        }
        End();
        if (axes)
            SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void BatchRenderer::DrawTexture(unsigned int textureId,
                                    float dstX, float dstY, float dstW, float dstH,
                                    float srcX, float srcY, float srcW, float srcH,
                                    float pivotX, float pivotY, float rotationDeg)
    {
        SetTextureId(textureId);

        if (rotationDeg != 0.0f)
        {
            PushMatrix();
            Translate(dstX + pivotX, dstY + pivotY);
            Rotate(rotationDeg, 0.0f, 0.0f, 1.0f);
            Translate(-pivotX, -pivotY);
            dstX = 0.0f;
            dstY = 0.0f;
        }

        Begin(MODE_TRIANGLES);

        float u0 = srcX, v0 = srcY;
        float u1 = srcX + srcW, v1 = srcY + srcH;

        if (srcW == 0.0f || srcH == 0.0f)
        {
            u0 = 0.0f;
            v0 = 0.0f;
            u1 = 1.0f;
            v1 = 1.0f;
        }

        EmitTexturedTriangle(dstX, dstY, u0, v0,
                             dstX + dstW, dstY, u1, v0,
                             dstX + dstW, dstY + dstH, u1, v1);
        EmitTexturedTriangle(dstX, dstY, u0, v0,
                             dstX + dstW, dstY + dstH, u1, v1,
                             dstX, dstY + dstH, u0, v1);

        End();

        if (rotationDeg != 0.0f)
            PopMatrix();
    }

    void BatchRenderer::DrawSprite(unsigned int textureId, float x, float y, float width, float height)
    {
        if (mVertices.size() + 4 > mConfig.maxVertices)
            FlushBatch();

        SetTextureId(textureId);
        Begin(MODE_QUADS);

        std::size_t base = mVertices.size();
        mVertices.resize(base + 4);
        Vertex *p = mVertices.data() + base;

        unsigned char r, g, b, a;
        UnpackColor(mCurrentColor, r, g, b, a);

        float x1 = x + width;
        float y1 = y + height;

        p[0].x = x;
        p[0].y = y;
        p[0].z = 0.0f;
        p[0].u = 0.0f;
        p[0].v = 0.0f;
        p[1].x = x1;
        p[1].y = y;
        p[1].z = 0.0f;
        p[1].u = 1.0f;
        p[1].v = 0.0f;
        p[2].x = x1;
        p[2].y = y1;
        p[2].z = 0.0f;
        p[2].u = 1.0f;
        p[2].v = 1.0f;
        p[3].x = x;
        p[3].y = y1;
        p[3].z = 0.0f;
        p[3].u = 0.0f;
        p[3].v = 1.0f;
        for (int i = 0; i < 4; ++i)
        {
            p[i].r = r;
            p[i].g = g;
            p[i].b = b;
            p[i].a = a;
        }

        End();
    }

    void BatchRenderer::BeginSpriteRun(unsigned int textureId)
    {
        SetTextureId(textureId);
        Begin(MODE_QUADS);
        UnpackColor(mCurrentColor, mRunR, mRunG, mRunB, mRunA);
    }

    void BatchRenderer::SubmitSprite(float x, float y, float width, float height)
    {
        if (CT_UNLIKELY(mVertices.size() + 4 > mConfig.maxVertices))
        {
            End();
            FlushBatch();
            Begin(MODE_QUADS);
        }

        std::size_t base = mVertices.size();
        mVertices.resize(base + 4);
        Vertex *p = mVertices.data() + base;

        float x1 = x + width;
        float y1 = y + height;

        p[0].x = x;
        p[0].y = y;
        p[0].z = 0.0f;
        p[0].u = 0.0f;
        p[0].v = 0.0f;
        p[1].x = x1;
        p[1].y = y;
        p[1].z = 0.0f;
        p[1].u = 1.0f;
        p[1].v = 0.0f;
        p[2].x = x1;
        p[2].y = y1;
        p[2].z = 0.0f;
        p[2].u = 1.0f;
        p[2].v = 1.0f;
        p[3].x = x;
        p[3].y = y1;
        p[3].z = 0.0f;
        p[3].u = 0.0f;
        p[3].v = 1.0f;
        for (int i = 0; i < 4; ++i)
        {
            p[i].r = mRunR;
            p[i].g = mRunG;
            p[i].b = mRunB;
            p[i].a = mRunA;
        }
    }

    void BatchRenderer::EndSpriteRun()
    {
        End();
    }

    void BatchRenderer::DrawTexture(unsigned int textureId, const Matrix2D &matrix,
                                    float width, float height, int texWidth, int texHeight,
                                    float pivotX, float pivotY,
                                    float srcX, float srcY, float srcW, float srcH,
                                    bool flipX, bool flipY)
    {
        if (mVertices.size() + 4 >= mConfig.maxVertices)
            FlushBatch();

        float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
        if (srcW != 0.0f && srcH != 0.0f && texWidth > 0 && texHeight > 0)
        {
            u0 = srcX / (float)texWidth;
            v0 = srcY / (float)texHeight;
            u1 = (srcX + srcW) / (float)texWidth;
            v1 = (srcY + srcH) / (float)texHeight;
        }
        if (flipX)
        {
            float t = u0;
            u0 = u1;
            u1 = t;
        }
        if (flipY)
        {
            float t = v0;
            v0 = v1;
            v1 = t;
        }

        float x0 = -pivotX * width;
        float y0 = -pivotY * height;
        float x1 = x0 + width;
        float y1 = y0 + height;

        glm::vec2 p0 = matrix.Transform(x0, y0);
        glm::vec2 p1 = matrix.Transform(x1, y0);
        glm::vec2 p2 = matrix.Transform(x1, y1);
        glm::vec2 p3 = matrix.Transform(x0, y1);

        SetTextureId(textureId);
        Begin(MODE_QUADS);
        SubmitVertex(p0.x, p0.y, 0.0f, u0, v0);
        SubmitVertex(p1.x, p1.y, 0.0f, u1, v0);
        SubmitVertex(p2.x, p2.y, 0.0f, u1, v1);
        SubmitVertex(p3.x, p3.y, 0.0f, u0, v1);
        End();
    }

    void BatchRenderer::DrawRenderBatch()
    {
        FlushBatch();
    }

    void BatchRenderer::BeginFrame()
    {
        if (mConfig.enableProfiling)
            mFrameStartTime = (long)clock();
    }

    void BatchRenderer::EndFrame()
    {
        long start = (long)clock();

        if (mInBeginEnd)
            End();

        if (!mVertices.empty())
            FlushBatch();

        long end = (long)clock();

        if (mConfig.enableProfiling)
        {
            mStats.renderTime = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
            mStats.totalTime = (double)(end - mFrameStartTime) / CLOCKS_PER_SEC * 1000.0;
            mStats.frameCount++;
            mStats.batchTime = mStats.totalTime - mStats.renderTime;
        }
    }

    void BatchRenderer::ResetStats()
    {
        std::memset(&mStats, 0, sizeof(mStats));
    }

    void BatchRenderer::PrintStats() const
    {
        const Stats &s = mStats;
        std::printf("=== BatchRenderer Stats ===\n");
        std::printf("  Draw Calls:        %zu\n", s.drawCalls);
        std::printf("  Vertices Drawn:    %zu\n", s.verticesDrawn);
        std::printf("  Indices Drawn:     %zu\n", s.indicesDrawn);
        std::printf("  Texture Switches:  %zu\n", s.textureSwitches);
        std::printf("  Batches Flushed:   %zu\n", s.batchesFlushed);
        std::printf("  Frame Count:       %zu\n", s.frameCount);
        std::printf("===========================\n");
    }

    void BatchRenderer::SetProjection(const glm::mat4 &matrix)
    {
        mProjection = matrix;
    }

    unsigned int BatchRenderer::PackColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        return (unsigned int)r |
               ((unsigned int)g << 8) |
               ((unsigned int)b << 16) |
               ((unsigned int)a << 24);
    }

    void BatchRenderer::UnpackColor(unsigned int packed, unsigned char &r, unsigned char &g,
                                    unsigned char &b, unsigned char &a)
    {
        r = packed & 0xFF;
        g = (packed >> 8) & 0xFF;
        b = (packed >> 16) & 0xFF;
        a = (packed >> 24) & 0xFF;
    }

    void BatchRenderer::UpdateProjection()
    {
        mProjection = glm::ortho(0.0f, (float)mWindowWidth,
                                 (float)mWindowHeight, 0.0f, -1.0f, 1.0f);
    }

    void BatchRenderer::FlushBatch()
    {
        if (mVertices.empty() || mDrawCalls.empty())
            return;

        glBindVertexArray(mVAO);

        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        (GLsizeiptr)(mVertices.size() * sizeof(Vertex)), mVertices.data());

        mIndices.clear();
        for (size_t i = 0; i < mDrawCalls.size(); ++i)
        {
            const DrawCall &call = mDrawCalls[i];
            if (call.mode == MODE_QUADS)
            {
                size_t quadCount = (size_t)call.vertexCount / 4;
                for (size_t q = 0; q < quadCount; ++q)
                {
                    size_t baseVertex = call.vertexAlignment + q * 4;
                    mIndices.push_back((unsigned short)(baseVertex + 0));
                    mIndices.push_back((unsigned short)(baseVertex + 1));
                    mIndices.push_back((unsigned short)(baseVertex + 2));
                    mIndices.push_back((unsigned short)(baseVertex + 0));
                    mIndices.push_back((unsigned short)(baseVertex + 2));
                    mIndices.push_back((unsigned short)(baseVertex + 3));
                }
            }
        }

        if (!mIndices.empty())
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                            (GLsizeiptr)(mIndices.size() * sizeof(unsigned short)), mIndices.data());
        }

        ApplyDrawCalls();

        if (mConfig.enableProfiling)
        {
            mStats.drawCalls += mDrawCalls.size();
            mStats.verticesDrawn += mVertices.size();
            mStats.indicesDrawn += mIndices.size();
            mStats.batchesFlushed++;
        }

        mVertices.clear();
        mIndices.clear();
        mDrawCalls.clear();
    }

    void BatchRenderer::ApplyDrawCalls()
    {
        glUseProgram(mProgram);
        glUniformMatrix4fv(mMvpLoc, 1, GL_FALSE, glm::value_ptr(mProjection));
        glBindVertexArray(mVAO);
        glActiveTexture(GL_TEXTURE0);

        size_t vertexOffset = 0;

        for (size_t i = 0; i < mDrawCalls.size(); ++i)
        {
            const DrawCall &call = mDrawCalls[i];

            if (call.depthTest)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
            glDepthMask(call.depthWrite ? GL_TRUE : GL_FALSE);
            if (call.cullFace)
                glEnable(GL_CULL_FACE);
            else
                glDisable(GL_CULL_FACE);
            if (call.blend)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
            {
                glDisable(GL_BLEND);
            }

            GLuint tex = (call.textureId != 0) ? call.textureId : mWhiteTexture;
            glBindTexture(GL_TEXTURE_2D, tex);

            if (call.mode == MODE_LINES)
            {
                glDrawArrays(GL_LINES, (GLint)vertexOffset, (GLsizei)call.vertexCount);
            }
            else if (call.mode == MODE_TRIANGLES)
            {
                glDrawArrays(GL_TRIANGLES, (GLint)vertexOffset, (GLsizei)call.vertexCount);
            }
            else if (call.mode == MODE_QUADS)
            {
                int indexOffset = 0;
                for (size_t j = 0; j < i; ++j)
                {
                    if (mDrawCalls[j].mode == MODE_QUADS)
                        indexOffset += (mDrawCalls[j].vertexCount / 4) * 6;
                }
                size_t indexCount = ((size_t)call.vertexCount / 4) * 6;
                glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_SHORT,
                               (void *)(size_t)(indexOffset * sizeof(unsigned short)));
            }

            vertexOffset += (size_t)call.vertexCount;
        }
    }

    bool BatchRenderer::SetupShaders()
    {
        GLuint vs = CompileShader(GL_VERTEX_SHADER, VERTEX_SHADER_SOURCE);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SOURCE);
        if (!vs || !fs)
            return false;

        mProgram = glCreateProgram();
        glAttachShader(mProgram, vs);
        glAttachShader(mProgram, fs);
        glLinkProgram(mProgram);
        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint ok = 0;
        glGetProgramiv(mProgram, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            char log[1024];
            glGetProgramInfoLog(mProgram, sizeof(log), nullptr, log);
            std::printf("shader link error: %s\n", log);
            glDeleteProgram(mProgram);
            mProgram = 0;
            return false;
        }

        mMvpLoc = glGetUniformLocation(mProgram, "u_mvp");
        mTexLoc = glGetUniformLocation(mProgram, "u_texture");
        glUseProgram(mProgram);
        glUniform1i(mTexLoc, 0);
        glUseProgram(0);
        return true;
    }

    void BatchRenderer::SetupBuffers()
    {
        glGenBuffers(1, &mVBO);
        glGenBuffers(1, &mIBO);
        glGenVertexArrays(1, &mVAO);

        glBindVertexArray(mVAO);

        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(mConfig.maxVertices * sizeof(Vertex)),
                     nullptr, GL_STREAM_DRAW);

        GLsizei stride = sizeof(Vertex);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)offsetof(Vertex, u));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void *)offsetof(Vertex, r));
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(mConfig.maxVertices * 3 * sizeof(unsigned short)),
                     nullptr, GL_STREAM_DRAW);

        glBindVertexArray(0);
    }

    void BatchRenderer::SetupTexture()
    {
        unsigned char whitePixel[4] = {255, 255, 255, 255};
        glGenTextures(1, &mWhiteTexture);
        glBindTexture(GL_TEXTURE_2D, mWhiteTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    void BatchRenderer::SetupFontTexture()
    {
        unsigned char *atlas = (unsigned char *)std::malloc(FONT_ATLAS_W * FONT_ATLAS_H * 4);
        std::memset(atlas, 0, FONT_ATLAS_W * FONT_ATLAS_H * 4);

        for (int g = 0; g < 96; ++g)
        {
            int cellX = (g % FONT_COLS) * 8;
            int cellY = (g / FONT_COLS) * 8;
            for (int row = 0; row < 8; ++row)
            {
                unsigned char bits = kFont8x8[g][row];
                for (int col = 0; col < 8; ++col)
                {
                    if (!((bits >> col) & 1))
                        continue;
                    unsigned char *p = &atlas[((cellY + row) * FONT_ATLAS_W + cellX + col) * 4];
                    p[0] = p[1] = p[2] = p[3] = 255;
                }
            }
        }

        glGenTextures(1, &mFontTexture);
        glBindTexture(GL_TEXTURE_2D, mFontTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FONT_ATLAS_W, FONT_ATLAS_H, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, atlas);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        std::free(atlas);
    }

    void BatchRenderer::SubmitVertex(float x, float y, float z)
    {
        SubmitVertex(x, y, z, mCurrentTexcoord[0], mCurrentTexcoord[1]);
    }

    void BatchRenderer::SubmitVertex(float x, float y, float z, float u, float v)
    {
        Vertex vert;
        vert.x = x;
        vert.y = y;
        vert.z = z;
        vert.u = u;
        vert.v = v;
        UnpackColor(mCurrentColor, vert.r, vert.g, vert.b, vert.a);
        mVertices.push_back(vert);
    }

    void BatchRenderer::ApplyTransform(float &x, float &y, float &z)
    {
        glm::vec4 t = mCurrentMatrix * glm::vec4(x, y, z, 1.0f);
        x = t.x;
        y = t.y;
        z = t.z;
    }

    void BatchRenderer::DrawText(float x, float y, float size, const char *text)
    {
        if (!text || size <= 0.f)
            return;

        SetTextureId(mFontTexture);
        Begin(MODE_TRIANGLES);

        const float cw = 8.f / (float)FONT_ATLAS_W;
        const float ch = 8.f / (float)FONT_ATLAS_H;
        float penX = x, penY = y;

        for (const char *c = text; *c; ++c)
        {
            if (*c == '\n')
            {
                penX = x;
                penY += size;
                continue;
            }
            unsigned char code = (unsigned char)*c;
            if (code < 32 || code > 127)
                code = '?';
            if (code != ' ')
            {
                int g = code - 32;
                const float u0 = (float)(g % FONT_COLS) * cw;
                const float v0 = (float)(g / FONT_COLS) * ch;
                const float u1 = u0 + cw;
                const float v1 = v0 + ch;

                EmitTexturedTriangle(penX, penY, u0, v0,
                                     penX + size, penY, u1, v0,
                                     penX, penY + size, u0, v1);
                EmitTexturedTriangle(penX + size, penY, u1, v0,
                                     penX + size, penY + size, u1, v1,
                                     penX, penY + size, u0, v1);
            }
            penX += size;
        }

        End();
    }

    float BatchRenderer::TextWidth(float size, const char *text) const
    {
        if (!text)
            return 0.f;
        unsigned int longest = 0, line = 0;
        for (const char *c = text; *c; ++c)
        {
            if (*c == '\n')
            {
                if (line > longest)
                    longest = line;
                line = 0;
                continue;
            }
            ++line;
        }
        if (line > longest)
            longest = line;
        return (float)longest * size;
    }

} // namespace k2d
