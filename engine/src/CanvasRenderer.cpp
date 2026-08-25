#include "k2d/CanvasRenderer.h"
#include "k2d/Profiler.h"
#include "font_data.h"

#include <glad/glad.h>



#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace k2d
{

    namespace
    {
        static const int FONT_COLS = 16;
        static const int FONT_ATLAS_W = 128;
        static const int FONT_ATLAS_H = 48;

        static const char *VERTEX_SHADER_SOURCE = R"(#version 300 es
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoord;
layout(location = 2) in vec4 a_color;
layout(location = 3) in uint a_lightMask;

uniform mat4 u_mvp;

out vec2 v_texcoord;
out vec4 v_color;
out vec2 v_world;
flat out uint v_lightMask;

void main()
{
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
    v_world = a_position.xy;
    v_lightMask = a_lightMask;
}
)";

        static const char *FRAGMENT_SHADER_SOURCE = R"(#version 300 es
// highp, not mediump: light math runs on world-space distances. mediump is
// fp16 on real mobile/AMD GLES drivers, and length() squares its input --
// fp16 overflows to Inf at sqrt(65504) = 255.9, silently killing every light
// beyond 256 world units (a perfect radius-256 cutoff circle). GLES3
// guarantees highp support in fragment shaders; Godot's canvas.glsl uses
// highp for the same reason.
precision highp float;

in vec2 v_texcoord;
in vec4 v_color;
in vec2 v_world;
flat in uint v_lightMask;

uniform sampler2D u_texture;
uniform sampler2D u_shadowAtlas;
uniform sampler2D u_lightTexture;
uniform int u_hasLightTexture;
uniform sampler2D u_normalMap;
uniform int u_hasNormalMap;
uniform vec2 u_canvasSize;
uniform vec4 u_canvasModulate;

uniform int u_lightCount;
uniform vec2 u_lightPos[8];
uniform vec4 u_lightColor[8];
uniform float u_lightRadius[8];
uniform uint u_lightCullMask[8];
uniform float u_lightHeight[8];
uniform int u_directionalLightCount;
uniform vec2 u_directionalLightDirection[8];
uniform vec4 u_directionalLightColor[8];
uniform int u_directionalLightShadowFlags[8];
uniform vec4 u_directionalLightShadowColor[8];
uniform uint u_directionalCullMask[8];
uniform float u_directionalHeight[8];
uniform int u_lightShadowFlags[8];
uniform int u_lightShadowFilter[8];
uniform vec4 u_lightShadowColor[8];
uniform int u_directionalShadowFilter[8];

out vec4 FragColor;

float shadowDepth(sampler2D atlas, vec2 uv, int filterMode)
{
    if (filterMode == 0)
        return texture(atlas, uv).r;
    float pixel = 1.0 / 2048.0;
    float total = 0.0;
    float count = 0.0;
    int radius = filterMode == 1 ? 2 : 6;
    for (int x = -6; x <= 6; ++x)
    {
        if (abs(x) > radius)
            continue;
        total += texture(atlas, uv + vec2(float(x) * pixel, 0.0)).r;
        count += 1.0;
    }
    return total / count;
}

void main()
{
    vec4 base = texture(u_texture, v_texcoord) * v_color;
    // Godot's CanvasModulate (canvas.glsl:716, color *= canvas_modulation):
    // darkens the unlit pass only. The light loops below multiply by the
    // UN-modulated base color (canvas.glsl:711 base_color / :814
    // light_color.rgb *= base_color.rgb), so lights "reveal" the surface's
    // true colors inside their radius instead of pasting a colored disc.
    FragColor = vec4(base.rgb * u_canvasModulate.rgb, base.a);
    // Godot's normal map decode (canvas.glsl:646): RG channels are the xy
    // tilt in 0..1, remapped to -1..1 (Y flipped, Godot's own normal map
    // authoring convention); Z reconstructed so the result is a unit vector.
    // A flat/neutral texel (128,128,255) decodes to (0,0,1) -- straight out
    // of the screen, matching an un-normal-mapped sprite's implicit normal.
    vec3 normal = vec3(0.0, 0.0, 1.0);
    if (u_hasNormalMap != 0)
    {
        vec2 normalXY = texture(u_normalMap, v_texcoord).xy * vec2(2.0, -2.0) - vec2(1.0, -1.0);
        normal = vec3(normalXY, sqrt(max(0.0, 1.0 - dot(normalXY, normalXY))));
    }
    if (u_lightCount > 0)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (i >= u_lightCount)
                break;
            // Godot's CanvasItem light_mask / Light2D range_item_cull_mask:
            // this light only affects items sharing at least one bit.
            if ((v_lightMask & u_lightCullMask[i]) == 0u)
                continue;
            vec2 toLight = u_lightPos[i] - v_world;
            float dist = length(toLight);
            float radialCut = 1.0 - dist / u_lightRadius[i];
            if (radialCut <= 0.0)
                continue;
            // Godot's Light2D "texture" (canvas.glsl:793 light_color =
            // textureLod(atlas_texture, ...)): the cookie is a FULL RGBA
            // sample -- the final ADD multiplies rgb AND alpha (:509
            // color.rgb += light_color.rgb * light_color.a), so a gradient
            // authored in RGB (neutral_point_light.png) or in ALPHA
            // (fire.png) both fade the light out smoothly. Reading just one
            // channel (an earlier version read .r) breaks alpha-gradient
            // cookies into a hard-edged uniform disc. light_color is then
            // cookie x light tint (rgb) / cookie alpha x energy (a), so the
            // shadow mix below and the final ADD work unchanged.
            vec4 light_color = u_lightColor[i];
            if (u_hasLightTexture != 0)
            {
                vec4 cookie = texture(u_lightTexture, -toLight / u_lightRadius[i] * 0.5 + 0.5);
                light_color = vec4(cookie.rgb * u_lightColor[i].rgb,
                                   cookie.a * u_lightColor[i].a);
            }
            else
            {
                light_color.a *= radialCut * radialCut;
            }
            if ((u_lightShadowFlags[i] & 1) != 0)
            {
                vec2 lightRay = v_world - u_lightPos[i];
                float angle = atan(lightRay.y, lightRay.x);
                float sector = floor((angle + 0.78539816339) / 1.57079632679);
                sector = mod(sector + 4.0, 4.0);
                vec2 direction = vec2(1.0, 0.0);
                if (sector == 1.0) direction = vec2(0.0, 1.0);
                if (sector == 2.0) direction = vec2(-1.0, 0.0);
                if (sector == 3.0) direction = vec2(0.0, -1.0);
                vec2 perpendicular = vec2(-direction.y, direction.x);
                float along = dot(lightRay, direction);
                float lateral = dot(lightRay, perpendicular);
                // tan(angle) within this 90 deg sector, matching the occluder
                // encode pass (RenderShadowAtlas), which projects edges with
                // true GPU perspective. Within a correctly picked sector
                // |lateral| <= along (45 deg half-angle), so this stays in [-1,1].
                float tangent = along > 0.0001 ? (lateral / along) : (lateral < 0.0 ? -1.0 : 1.0);
                float atlasU = (sector + (tangent + 1.0) * 0.5) / 4.0;
                float atlasV = (float(i * 2) + 0.5) / 16.0;
                float storedDepth = shadowDepth(u_shadowAtlas, vec2(atlasU, atlasV), u_lightShadowFilter[i]);
                float shadow = (along > 0.0 && along / u_lightRadius[i] > storedDepth + 0.002) ? 1.0 : 0.0;
                vec4 shadow_color = u_lightShadowColor[i];
                shadow_color.a *= light_color.a;
                light_color = mix(light_color, shadow_color, shadow);
            }
            // Godot canvas.glsl:814 light_color.rgb *= base_color.rgb, then
            // blend ADD (:509). The light reflects the surface's own color;
            // the falloff lives inside light_color (cookie rgb and/or alpha).
            float cNdotL = 1.0;
            if (u_hasNormalMap != 0)
            {
                // canvas.glsl:799/809/420: light_vec points from the surface
                // to the light, height is the light's Z offset above the
                // plane -- toLight is already (light - surface) in-plane.
                vec3 lightVec = normalize(vec3(toLight, u_lightHeight[i]));
                cNdotL = max(0.0, dot(normal, lightVec));
            }
            FragColor.rgb += light_color.rgb * light_color.a * base.rgb * cNdotL;
        }
    }
    for (int i = 0; i < 8; ++i)
    {
        if (i >= u_directionalLightCount)
            break;
        if ((v_lightMask & u_directionalCullMask[i]) == 0u)
            continue;
        vec4 light_color = u_directionalLightColor[i];
        if ((u_directionalLightShadowFlags[i] & 1) != 0)
        {
            vec2 center = u_canvasSize * 0.5;
            float halfSize = length(u_canvasSize) * 0.5;
            float shadowDistance = length(u_canvasSize) * 2.0;
            vec2 rayDir = u_directionalLightDirection[i];
            vec2 perpendicular = vec2(-rayDir.y, rayDir.x);
            vec2 relative = v_world - center;
            float lateral = dot(relative, perpendicular);
            float along = dot(relative, rayDir);
            float atlasU = (lateral / halfSize + 1.0) * 0.5;
            float atlasV = (8.0 + float(i * 2) + 0.5) / 16.0;
            float storedDepth = shadowDepth(u_shadowAtlas, vec2(atlasU, atlasV), u_directionalShadowFilter[i]);
            float fragmentDepth = (along + shadowDistance * 0.5) / shadowDistance;
            float shadow = (fragmentDepth > storedDepth + 0.002) ? 1.0 : 0.0;
            vec4 shadow_color = u_directionalLightShadowColor[i];
            shadow_color.a *= light_color.a;
            light_color = mix(light_color, shadow_color, shadow);
        }
        float cNdotL = 1.0;
        if (u_hasNormalMap != 0)
        {
            // canvas.glsl:736: directional height is a 0..1 MIX factor
            // between the in-plane direction and straight down (0,0,1), not
            // a Z offset like point lights. rayDir/direction here already
            // points FROM the light TOWARD the scene (see the shadow raycast
            // above), so the vector TO the light is its negation.
            vec2 rayDir = u_directionalLightDirection[i];
            vec3 lightVec = normalize(mix(vec3(-rayDir, 0.0), vec3(0.0, 0.0, 1.0), u_directionalHeight[i]));
            cNdotL = max(0.0, dot(normal, lightVec));
        }
        FragColor.rgb += light_color.rgb * light_color.a * base.rgb * cNdotL;
    }
}
)";

        static const char *SHADOW_VERTEX_SHADER_SOURCE = R"(#version 300 es
layout(location = 0) in vec4 a_v;
out float v_dist;
void main()
{
    // Godot's shadow frustum in radius-normalized units:
    // near n = 0.001, far f = 1.1 -> z_clip = ((f+n)*d - 2*f*n) / (f - n).
    float d = a_v.z;
    gl_Position = vec4(a_v.x, a_v.y, (1.101 * d - 0.0022) / 1.099, a_v.w);
    v_dist = d;
}
)";

        static const char *SHADOW_FRAGMENT_SHADER_SOURCE = R"(#version 300 es
precision highp float;
in float v_dist;
layout(location = 0) out float out_depth;
void main()
{
    out_depth = v_dist;
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
                std::printf("canvas shader compile error: %s\n", log);
                glDeleteShader(s);
                return 0;
            }
            return s;
        }

        static GLuint CreateProgram(const char *vertexSource, const char *fragmentSource)
        {
            GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexSource);
            GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
            if (!vertex || !fragment)
            {
                if (vertex) glDeleteShader(vertex);
                if (fragment) glDeleteShader(fragment);
                return 0;
            }
            GLuint program = glCreateProgram();
            glAttachShader(program, vertex);
            glAttachShader(program, fragment);
            glLinkProgram(program);
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            GLint linked = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if (!linked)
            {
                char log[1024];
                glGetProgramInfoLog(program, sizeof(log), nullptr, log);
                std::printf("shadow shader link error: %s\n", log);
                glDeleteProgram(program);
                return 0;
            }
            return program;
        }

        static void UnpackColor(unsigned int packed, unsigned char &r, unsigned char &g,
                                unsigned char &b, unsigned char &a)
        {
            r = packed & 0xFF;
            g = (packed >> 8) & 0xFF;
            b = (packed >> 16) & 0xFF;
            a = (packed >> 24) & 0xFF;
        }

        static void ApplyBlend(RenderState &state, BlendMode mode)
        {
            switch (mode)
            {
            case BLEND_MIX:
                state.SetBlendEquation(GL_FUNC_ADD);
                state.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BLEND_ADD:
                state.SetBlendEquation(GL_FUNC_ADD);
                state.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;
            case BLEND_SUB:
                state.SetBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                state.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;
            case BLEND_MUL:
                state.SetBlendEquation(GL_FUNC_ADD);
                state.SetBlendFunc(GL_DST_COLOR, GL_ZERO);
                break;
            default:
                state.SetBlendEquation(GL_FUNC_ADD);
                state.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            }
        }
    } 

    CanvasRenderer::CanvasRenderer()
        : mConfig(), mStats(), mVertices(), mIndices(), mDrawCalls(),
          mCurrentTextureId(0), mProjection(1.0f), mOccluderEdges(),
          mVAO(0), mVBO(0), mIBO(0), mProgram(0), mShadowProgram(0), mWhiteTexture(0), mFontTexture(0),
          mDefaultLightTexture(0), mLightTextureLoc(-1), mHasLightTextureLoc(-1),
          mCanvasModulate(1.0f, 1.0f, 1.0f, 1.0f), mCanvasModulateLoc(-1),
          mNormalMapLoc(-1), mHasNormalMapLoc(-1), mLightHeightLoc(-1), mDirectionalHeightLoc(-1),
          mShadowAtlas(0), mShadowDepth(0), mShadowFramebuffer(0), mShadowVAO(0), mShadowVBO(0),
          mMvpLoc(-1), mTexLoc(-1), mLightCountLoc(-1), mLightPosLoc(-1),
          mLightColorLoc(-1), mLightRadiusLoc(-1), mLightCullMaskLoc(-1),
          mDirectionalLightCountLoc(-1), mDirectionalLightDirectionLoc(-1),
          mDirectionalLightColorLoc(-1), mDirectionalCullMaskLoc(-1), mDirectionalLightShadowFlagsLoc(-1),
          mDirectionalLightShadowColorLoc(-1), mDirectionalShadowFilterLoc(-1),
          mLights(nullptr), mLightCount(0),
          mDirectionalLights(nullptr), mDirectionalLightCount(0),
          mShadowFlagsLoc(-1), mShadowColorLoc(-1), mShadowFilterLoc(-1),
          mShadowAtlasLoc(-1), mCanvasSizeLoc(-1),
          mOccluders(nullptr), mOccluderCount(0), mOrthoWidth(1280.0f), mOrthoHeight(720.0f)
    {
        std::memset(&mStats, 0, sizeof(mStats));
    }

    CanvasRenderer::~CanvasRenderer()
    {
        Shutdown();
    }

    bool CanvasRenderer::Init(const Config &config)
    {
        mConfig = config;
        mVertices.reserve(mConfig.maxVertices);
        mIndices.reserve(mConfig.maxVertices * 3);
        mDrawCalls.reserve(mConfig.maxDrawCalls);

        if (!SetupShaders())
            return false;
        SetupBuffers();
        SetupTexture();
        SetupFontTexture();
        SetupShadowAtlas();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        std::printf("CanvasRenderer initialized: maxVertices=%zu maxDrawCalls=%zu\n",
                    mConfig.maxVertices, mConfig.maxDrawCalls);
        return true;
    }

    void CanvasRenderer::Shutdown()
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
        if (mVAO)
        {
            glDeleteVertexArrays(1, &mVAO);
            mVAO = 0;
        }
        if (mProgram)
        {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }
        if (mShadowProgram)
        {
            glDeleteProgram(mShadowProgram);
            mShadowProgram = 0;
        }
        ShutdownShadowAtlas();
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
    }

    void CanvasRenderer::SetProjection(const Math::Mat4 &matrix)
    {
        mProjection = matrix;
    }

    void CanvasRenderer::SetOrtho(float width, float height)
    {
        mProjection = Math::Mat4::Ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
        mOrthoWidth = width;
        mOrthoHeight = height;
    }

    void CanvasRenderer::SetCanvasModulate(float r, float g, float b)
    {
        mCanvasModulate = Color(r, g, b, 1.0f);
    }

    unsigned int CanvasRenderer::CreateShader(const char *fragmentSource)
    {

        GLuint program = CreateProgram(VERTEX_SHADER_SOURCE, fragmentSource);
        if (!program)
            return 0;

        glUseProgram(program);
        GLint texLoc = glGetUniformLocation(program, "u_texture");
        if (texLoc >= 0)
            glUniform1i(texLoc, 0);
        glUseProgram(0);
        return program;
    }

    void CanvasRenderer::DestroyShader(unsigned int program)
    {
        if (program != 0)
            glDeleteProgram(program);
    }

    void CanvasRenderer::SetDefaultLightTexture(unsigned int textureId)
    {
        mDefaultLightTexture = textureId;
        if (mDefaultLightTexture != 0)
        {

            glBindTexture(GL_TEXTURE_2D, mDefaultLightTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void CanvasRenderer::DrawItems(const RenderItem *items, size_t count,
                                   const PointLight *lights, size_t lightCount,
                                   const DirectionalLight *directionalLights, size_t directionalLightCount,
                                   const Occluder *occluders, size_t occluderCount)
    {
        ProfileScope profileScope("render.expand");
        mLights = lights;
        mLightCount = (int)lightCount;
        mDirectionalLights = directionalLights;
        mDirectionalLightCount = (int)directionalLightCount;
        mOccluders = occluders;
        mOccluderCount = occluderCount;

        FlattenOccluderEdges();
        RenderShadowAtlas();
        // Shadow rendering and external UI passes may alter GL state directly.
        // Start the canvas pass with an unknown state, then cache it for every
        // draw call and across capacity-triggered Flush() calls.
        mState.Invalidate();

        for (size_t i = 0; i < count; ++i)
        {
            const RenderItem &item = items[i];

            Matrix2D drawTransform;
            for (size_t j = 0; j < item.commands.size(); ++j)
            {
                const RenderCommand &c = item.commands[j];
                switch (c.type)
                {
                case RenderCommand::kTransform:
                    drawTransform = c.matrix;
                    break;
                case RenderCommand::kRect:
                {
                    Matrix2D m = item.xform * drawTransform * Matrix2D::Translation(c.x, c.y);
                    EmitQuad(item.blendMode, c.textureId, c.normalTextureId, c.customProgram, m,
                             c.width, c.height,
                             c.texWidth, c.texHeight, c.pivotX, c.pivotY,
                             c.srcX, c.srcY, c.srcW, c.srcH,
                             (c.flags & 1) != 0, (c.flags & 2) != 0, c.color, c.lightMask);
                    break;
                }
                case RenderCommand::kPolygon:
                {
                    const ct::Vector<Math::Vec2> *points = c.polygonPoints;
                    if (!points && !c.ownedPolygonPoints.empty())
                        points = &c.ownedPolygonPoints;
                    if (points && points->size() >= 3)
                    {
                        Matrix2D m = item.xform * drawTransform;
                        EmitPolygon(item.blendMode, c.textureId, c.normalTextureId, c.customProgram, m,
                                   *points, c.color, c.lightMask, c.texWidth, c.texHeight);
                    }
                    break;
                }
                case RenderCommand::kText:
                {
                    Matrix2D m = item.xform * drawTransform;
                    EmitText(item.blendMode, m, c.x, c.y, c.width, c.text.c_str(), c.color, c.lightMask);
                    break;
                }
                default:
                    break;
                }
            }
        }

        Flush();
    }

    void CanvasRenderer::EmitQuad(BlendMode blendMode, unsigned int textureId, unsigned int normalTextureId,
                                  unsigned int customProgram, const Matrix2D &matrix,
                                  float width, float height, int texWidth, int texHeight,
                                  float pivotX, float pivotY,
                                  float srcX, float srcY, float srcW, float srcH,
                                  bool flipX, bool flipY, const Color &color, unsigned int lightMask)
    {
        if (mVertices.size() + 4 > mConfig.maxVertices)
            Flush();

        if (mDrawCalls.empty() ||
            mDrawCalls.back().textureId != textureId ||
            mDrawCalls.back().normalTextureId != normalTextureId ||
            mDrawCalls.back().program != customProgram ||
            mDrawCalls.back().blendMode != blendMode)
        {
            if (mCurrentTextureId != textureId)
                mStats.textureSwitches++;
            DrawCall call;
            call.vertexCount = 0;
            call.indexCount = 0;
            call.vertexAlignment = mVertices.size();
            call.textureId = textureId;
            call.normalTextureId = normalTextureId;
            call.program = customProgram;
            call.blendMode = blendMode;
            mDrawCalls.push_back(call);
            mCurrentTextureId = textureId;
        }

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

        Math::Vec2 p0 = matrix.Transform(x0, y0);
        Math::Vec2 p1 = matrix.Transform(x1, y0);
        Math::Vec2 p2 = matrix.Transform(x1, y1);
        Math::Vec2 p3 = matrix.Transform(x0, y1);

        unsigned char r, g, b, a;
        UnpackColor(color.Packed(), r, g, b, a);

        size_t base = mVertices.size();
        mVertices.push_back(Vertex{p0.x, p0.y, 0.0f, u0, v0, r, g, b, a, lightMask});
        mVertices.push_back(Vertex{p1.x, p1.y, 0.0f, u1, v0, r, g, b, a, lightMask});
        mVertices.push_back(Vertex{p2.x, p2.y, 0.0f, u1, v1, r, g, b, a, lightMask});
        mVertices.push_back(Vertex{p3.x, p3.y, 0.0f, u0, v1, r, g, b, a, lightMask});

        mIndices.push_back((unsigned short)(base + 0));
        mIndices.push_back((unsigned short)(base + 1));
        mIndices.push_back((unsigned short)(base + 2));
        mIndices.push_back((unsigned short)(base + 0));
        mIndices.push_back((unsigned short)(base + 2));
        mIndices.push_back((unsigned short)(base + 3));

        mDrawCalls.back().vertexCount += 4;
        mDrawCalls.back().indexCount += 6;
    }

    void CanvasRenderer::EmitPolygon(BlendMode blendMode, unsigned int textureId, unsigned int normalTextureId,
                                     unsigned int customProgram, const Matrix2D &matrix,
                                     const ct::Vector<Math::Vec2> &points,
                                     const Color &color, unsigned int lightMask,
                                     int texWidth, int texHeight)
    {
        if (points.size() < 3)
            return;
        if (mVertices.size() + points.size() > mConfig.maxVertices)
            Flush();

        if (mDrawCalls.empty() ||
            mDrawCalls.back().textureId != textureId ||
            mDrawCalls.back().normalTextureId != normalTextureId ||
            mDrawCalls.back().program != customProgram ||
            mDrawCalls.back().blendMode != blendMode)
        {
            if (mCurrentTextureId != textureId)
                mStats.textureSwitches++;
            DrawCall call;
            call.vertexCount = 0;
            call.indexCount = 0;
            call.vertexAlignment = mVertices.size();
            call.textureId = textureId;
            call.normalTextureId = normalTextureId;
            call.program = customProgram;
            call.blendMode = blendMode;
            mDrawCalls.push_back(call);
            mCurrentTextureId = textureId;
        }

        unsigned char r, g, b, a;
        UnpackColor(color.Packed(), r, g, b, a);
        size_t base = mVertices.size();
        for (size_t i = 0; i < points.size(); ++i)
        {
            const Math::Vec2 &local = points[i];
            Math::Vec2 p = matrix.Transform(local);

            float u = texWidth > 0 ? local.x / (float)texWidth : 0.0f;
            float v = texHeight > 0 ? local.y / (float)texHeight : 0.0f;
            mVertices.push_back(Vertex{p.x, p.y, 0.0f, u, v, r, g, b, a, lightMask});
        }
        for (size_t i = 0; i + 2 < points.size(); i += 3)
        {
            mIndices.push_back((unsigned short)(base + i));
            mIndices.push_back((unsigned short)(base + i + 1));
            mIndices.push_back((unsigned short)(base + i + 2));
        }
        mDrawCalls.back().vertexCount += points.size();
        mDrawCalls.back().indexCount += (points.size() / 3) * 3;
    }

    void CanvasRenderer::EmitText(BlendMode blendMode, const Matrix2D &matrix, float x, float y, float size,
                                  const char *text, const Color &color, unsigned int lightMask)
    {
        if (!text || !text[0] || size <= 0.0f || mFontTexture == 0)
            return;

        const float cellWidth = 8.0f / (float)FONT_ATLAS_W;
        const float cellHeight = 8.0f / (float)FONT_ATLAS_H;
        float penX = x;
        float penY = y;
        for (const char *character = text; *character; ++character)
        {
            if (*character == '\n')
            {
                penX = x;
                penY += size;
                continue;
            }

            unsigned char code = (unsigned char)*character;
            if (code < 32 || code > 127)
                code = '?';
            if (code != ' ')
            {
                const int glyph = code - 32;
                const float srcX = (float)(glyph % FONT_COLS) * 8.0f;
                const float srcY = (float)(glyph / FONT_COLS) * 8.0f;
                EmitQuad(blendMode, mFontTexture, 0, 0,
                         matrix * Matrix2D::Translation(penX, penY), size, size,
                         FONT_ATLAS_W, FONT_ATLAS_H, 0.0f, 0.0f,
                         srcX, srcY, 8.0f, 8.0f, false, false, color, lightMask);
            }
            penX += size;
        }
    }

    void CanvasRenderer::Flush()
    {
        ProfileScope profileScope("render.flush");
        if (mVertices.empty() || mDrawCalls.empty())
        {
            mVertices.clear();
            mIndices.clear();
            mDrawCalls.clear();
            mCurrentTextureId = 0;
            return;
        }

        mState.BindVertexArray(mVAO);
        mState.BindArrayBuffer(mVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        (GLsizeiptr)(mVertices.size() * sizeof(Vertex)), mVertices.data());
        mState.BindElementArrayBuffer(mIBO);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                        (GLsizeiptr)(mIndices.size() * sizeof(unsigned short)), mIndices.data());

        ApplyDrawCalls();

        mStats.drawCalls += mDrawCalls.size();
        mStats.verticesDrawn += mVertices.size();
        mStats.indicesDrawn += mIndices.size();
        mStats.batchesFlushed++;

        mVertices.clear();
        mIndices.clear();
        mDrawCalls.clear();
        mCurrentTextureId = 0;
    }

    void CanvasRenderer::ApplyDrawCalls()
    {
        mState.UseProgram(mProgram);
        glUniformMatrix4fv(mMvpLoc, 1, GL_FALSE, mProjection.Data());
        glUniform1i(mShadowAtlasLoc, 1);
        glUniform1i(mHasLightTextureLoc, mDefaultLightTexture != 0 ? 1 : 0);
        glUniform4fv(mCanvasModulateLoc, 1, &mCanvasModulate.r);
        glUniform2f(mCanvasSizeLoc, mOrthoWidth, mOrthoHeight);
        glUniform1i(mLightCountLoc, mLightCount);
        if (mLightCount > 0)
        {
            float pos[16];
            float col[32];
            float rad[8];
            unsigned int cullMask[8];
            float height[8];
            for (int i = 0; i < mLightCount; ++i)
            {
                pos[i * 2 + 0] = mLights[i].position.x;
                pos[i * 2 + 1] = mLights[i].position.y;
                col[i * 4 + 0] = mLights[i].color.r;
                col[i * 4 + 1] = mLights[i].color.g;
                col[i * 4 + 2] = mLights[i].color.b;
                col[i * 4 + 3] = mLights[i].color.a;
                rad[i] = mLights[i].radius;
                cullMask[i] = mLights[i].cullMask;
                height[i] = mLights[i].height;
            }
            glUniform2fv(mLightPosLoc, mLightCount, pos);
            glUniform4fv(mLightColorLoc, mLightCount, col);
            glUniform1fv(mLightRadiusLoc, mLightCount, rad);
            glUniform1uiv(mLightCullMaskLoc, mLightCount, cullMask);
            glUniform1fv(mLightHeightLoc, mLightCount, height);
        }

        glUniform1i(mDirectionalLightCountLoc, mDirectionalLightCount);
        if (mDirectionalLightCount > 0)
        {
            float direction[16];
            float color[32];
            unsigned int dirCullMask[8];
            float dirHeight[8];
            for (int i = 0; i < mDirectionalLightCount; ++i)
            {
                direction[i * 2 + 0] = mDirectionalLights[i].direction.x;
                direction[i * 2 + 1] = mDirectionalLights[i].direction.y;
                color[i * 4 + 0] = mDirectionalLights[i].color.r;
                color[i * 4 + 1] = mDirectionalLights[i].color.g;
                color[i * 4 + 2] = mDirectionalLights[i].color.b;
                color[i * 4 + 3] = mDirectionalLights[i].color.a;
                dirCullMask[i] = mDirectionalLights[i].cullMask;
                dirHeight[i] = mDirectionalLights[i].height;
            }
            glUniform2fv(mDirectionalLightDirectionLoc, mDirectionalLightCount, direction);
            glUniform4fv(mDirectionalLightColorLoc, mDirectionalLightCount, color);
            glUniform1uiv(mDirectionalCullMaskLoc, mDirectionalLightCount, dirCullMask);
            glUniform1fv(mDirectionalHeightLoc, mDirectionalLightCount, dirHeight);
        }
        int directionalFlags[8];
        int directionalFilters[8];
        float directionalShadowColor[32];
        for (int i = 0; i < 8; ++i)
        {
            directionalFlags[i] = 0;
            directionalFilters[i] = 0;
            for (int j = 0; j < 4; ++j)
                directionalShadowColor[i * 4 + j] = 0.0f;
        }
        for (int i = 0; i < mDirectionalLightCount; ++i)
        {
            directionalFlags[i] = mDirectionalLights[i].useShadow ? 1 : 0;
            directionalFilters[i] = (int)mDirectionalLights[i].shadowFilter;
            directionalShadowColor[i * 4 + 0] = mDirectionalLights[i].shadowColor.r;
            directionalShadowColor[i * 4 + 1] = mDirectionalLights[i].shadowColor.g;
            directionalShadowColor[i * 4 + 2] = mDirectionalLights[i].shadowColor.b;
            directionalShadowColor[i * 4 + 3] = mDirectionalLights[i].shadowColor.a;
        }
        glUniform1iv(mDirectionalLightShadowFlagsLoc, 8, directionalFlags);
        glUniform1iv(mDirectionalShadowFilterLoc, 8, directionalFilters);
        glUniform4fv(mDirectionalLightShadowColorLoc, 8, directionalShadowColor);

        int flags[8];
        int filters[8];
        float scolor[32];
        for (int i = 0; i < 8; ++i)
        {
            flags[i] = 0;
            filters[i] = 0;
            for (int j = 0; j < 4; ++j)
                scolor[i * 4 + j] = 0.0f;
        }
        for (int i = 0; i < mLightCount; ++i)
        {
            if (mLights[i].useShadow)
                flags[i] = 1;
            filters[i] = (int)mLights[i].shadowFilter;
            scolor[i * 4 + 0] = mLights[i].shadowColor.r;
            scolor[i * 4 + 1] = mLights[i].shadowColor.g;
            scolor[i * 4 + 2] = mLights[i].shadowColor.b;
            scolor[i * 4 + 3] = mLights[i].shadowColor.a;
        }
        glUniform1iv(mShadowFlagsLoc, 8, flags);
        glUniform1iv(mShadowFilterLoc, 8, filters);
        glUniform4fv(mShadowColorLoc, 8, scolor);
        mState.BindVertexArray(mVAO);
        mState.BindTexture2D(1, mShadowAtlas);
        if (mDefaultLightTexture != 0)
            mState.BindTexture2D(2, mDefaultLightTexture);

        size_t indexOffset = 0;

        for (size_t i = 0; i < mDrawCalls.size(); ++i)
        {
            const DrawCall &call = mDrawCalls[i];

            mState.SetBlendEnabled(true);
            ApplyBlend(mState, call.blendMode);

            GLuint wantProgram = (call.program != 0) ? call.program : mProgram;
            if (wantProgram != mProgram)
            {
                mState.UseProgram(wantProgram);
                GLint mvpLoc = glGetUniformLocation(wantProgram, "u_mvp");
                if (mvpLoc >= 0)
                    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mProjection.Data());
            }
            else
            {
                mState.UseProgram(mProgram);
            }

            GLuint tex = (call.textureId != 0) ? call.textureId : mWhiteTexture;
            mState.BindTexture2D(0, tex);

            if (wantProgram == mProgram)
            {
                glUniform1i(mHasNormalMapLoc, call.normalTextureId != 0 ? 1 : 0);
                if (call.normalTextureId != 0)
                    mState.BindTexture2D(3, call.normalTextureId);
            }

            size_t indexCount = call.indexCount;
            glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_SHORT,
                           (void *)(size_t)(indexOffset * sizeof(unsigned short)));

            indexOffset += indexCount;
        }
    }

    bool CanvasRenderer::SetupShaders()
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
            std::printf("canvas shader link error: %s\n", log);
            glDeleteProgram(mProgram);
            mProgram = 0;
            return false;
        }

        mMvpLoc = glGetUniformLocation(mProgram, "u_mvp");
        mTexLoc = glGetUniformLocation(mProgram, "u_texture");
        mLightCountLoc = glGetUniformLocation(mProgram, "u_lightCount");
        mLightPosLoc = glGetUniformLocation(mProgram, "u_lightPos");
        mLightColorLoc = glGetUniformLocation(mProgram, "u_lightColor");
        mLightRadiusLoc = glGetUniformLocation(mProgram, "u_lightRadius");
        mLightCullMaskLoc = glGetUniformLocation(mProgram, "u_lightCullMask");
        mLightHeightLoc = glGetUniformLocation(mProgram, "u_lightHeight");
        mDirectionalCullMaskLoc = glGetUniformLocation(mProgram, "u_directionalCullMask");
        mDirectionalHeightLoc = glGetUniformLocation(mProgram, "u_directionalHeight");
        mDirectionalLightCountLoc = glGetUniformLocation(mProgram, "u_directionalLightCount");
        mDirectionalLightDirectionLoc = glGetUniformLocation(mProgram, "u_directionalLightDirection");
        mDirectionalLightColorLoc = glGetUniformLocation(mProgram, "u_directionalLightColor");
        mDirectionalLightShadowFlagsLoc = glGetUniformLocation(mProgram, "u_directionalLightShadowFlags");
        mDirectionalLightShadowColorLoc = glGetUniformLocation(mProgram, "u_directionalLightShadowColor");
        mDirectionalShadowFilterLoc = glGetUniformLocation(mProgram, "u_directionalShadowFilter");
        mShadowFlagsLoc = glGetUniformLocation(mProgram, "u_lightShadowFlags");
        mShadowColorLoc = glGetUniformLocation(mProgram, "u_lightShadowColor");
        mShadowFilterLoc = glGetUniformLocation(mProgram, "u_lightShadowFilter");
        mShadowAtlasLoc = glGetUniformLocation(mProgram, "u_shadowAtlas");
        mCanvasSizeLoc = glGetUniformLocation(mProgram, "u_canvasSize");
        mLightTextureLoc = glGetUniformLocation(mProgram, "u_lightTexture");
        mHasLightTextureLoc = glGetUniformLocation(mProgram, "u_hasLightTexture");
        mCanvasModulateLoc = glGetUniformLocation(mProgram, "u_canvasModulate");
        mNormalMapLoc = glGetUniformLocation(mProgram, "u_normalMap");
        mHasNormalMapLoc = glGetUniformLocation(mProgram, "u_hasNormalMap");
        glUseProgram(mProgram);
        glUniform1i(mTexLoc, 0);
        glUniform1i(mShadowAtlasLoc, 1);
        glUniform1i(mLightTextureLoc, 2);
        glUniform1i(mNormalMapLoc, 3);
        glUseProgram(0);
        return true;
    }

    void CanvasRenderer::SetupBuffers()
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

        glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, stride, (void *)offsetof(Vertex, lightMask));
        glEnableVertexAttribArray(3);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(mConfig.maxVertices * 3 * sizeof(unsigned short)),
                     nullptr, GL_STREAM_DRAW);

        glBindVertexArray(0);
    }

    void CanvasRenderer::SetupTexture()
    {
        unsigned char whitePixel[4] = {255, 255, 255, 255};
        glGenTextures(1, &mWhiteTexture);
        glBindTexture(GL_TEXTURE_2D, mWhiteTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    void CanvasRenderer::SetupFontTexture()
    {
        unsigned char *atlas = (unsigned char *)std::malloc(FONT_ATLAS_W * FONT_ATLAS_H * 4);
        if (!atlas)
            return;
        std::memset(atlas, 0, FONT_ATLAS_W * FONT_ATLAS_H * 4);

        for (int glyph = 0; glyph < 96; ++glyph)
        {
            const int cellX = (glyph % FONT_COLS) * 8;
            const int cellY = (glyph / FONT_COLS) * 8;
            for (int row = 0; row < 8; ++row)
            {
                const unsigned char bits = kFont8x8[glyph][row];
                for (int column = 0; column < 8; ++column)
                {
                    if (((bits >> column) & 1) == 0)
                        continue;
                    unsigned char *pixel = &atlas[((cellY + row) * FONT_ATLAS_W + cellX + column) * 4];
                    pixel[0] = pixel[1] = pixel[2] = pixel[3] = 255;
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

    void CanvasRenderer::SetupShadowAtlas()
    {
        static const int textureSize = 2048;
        static const int rows = 16;

        mShadowProgram = CreateProgram(SHADOW_VERTEX_SHADER_SOURCE, SHADOW_FRAGMENT_SHADER_SOURCE);
        if (!mShadowProgram)
            return;

        glGenTextures(1, &mShadowAtlas);
        glBindTexture(GL_TEXTURE_2D, mShadowAtlas);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, textureSize, rows, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers(1, &mShadowDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, mShadowDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, textureSize, rows);

        glGenFramebuffers(1, &mShadowFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mShadowFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mShadowAtlas, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mShadowDepth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::printf("shadow atlas framebuffer is incomplete\n");
            ShutdownShadowAtlas();
            return;
        }
        glViewport(0, 0, textureSize, rows);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glGenVertexArrays(1, &mShadowVAO);
        glGenBuffers(1, &mShadowVBO);
        glBindVertexArray(mShadowVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mShadowVBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(ShadowVertex), nullptr);
        glBindVertexArray(0);
    }

    void CanvasRenderer::ShutdownShadowAtlas()
    {
        if (mShadowVBO)
        {
            glDeleteBuffers(1, &mShadowVBO);
            mShadowVBO = 0;
        }
        if (mShadowVAO)
        {
            glDeleteVertexArrays(1, &mShadowVAO);
            mShadowVAO = 0;
        }
        if (mShadowFramebuffer)
        {
            glDeleteFramebuffers(1, &mShadowFramebuffer);
            mShadowFramebuffer = 0;
        }
        if (mShadowDepth)
        {
            glDeleteRenderbuffers(1, &mShadowDepth);
            mShadowDepth = 0;
        }
        if (mShadowAtlas)
        {
            glDeleteTextures(1, &mShadowAtlas);
            mShadowAtlas = 0;
        }
        mShadowVertices.clear();
    }

    void CanvasRenderer::RenderShadowAtlas()
    {
        if (!mShadowFramebuffer || !mShadowProgram || mOccluderEdges.empty())
            return;

        GLint previousViewport[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, previousViewport);

        static const int textureSize = 2048;
        static const Math::Vec2 directions[4] = {
            Math::Vec2(1.0f, 0.0f), Math::Vec2(0.0f, 1.0f),
            Math::Vec2(-1.0f, 0.0f), Math::Vec2(0.0f, -1.0f)};

        glBindFramebuffer(GL_FRAMEBUFFER, mShadowFramebuffer);
        glUseProgram(mShadowProgram);
        glBindVertexArray(mShadowVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mShadowVBO);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_SCISSOR_TEST);

        for (int lightIndex = 0; lightIndex < mLightCount; ++lightIndex)
        {
            if (!mLights[lightIndex].useShadow || mLights[lightIndex].radius <= 0.0f)
                continue;

            glViewport(0, lightIndex * 2, textureSize, 2);
            glScissor(0, lightIndex * 2, textureSize, 2);
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            for (int sector = 0; sector < 4; ++sector)
            {
                const Math::Vec2 direction = directions[sector];
                const Math::Vec2 perpendicular(-direction.y, direction.x);
                int viewportX = (textureSize / 4) * sector;
                glViewport(viewportX, lightIndex * 2, textureSize / 4, 2);
                glScissor(viewportX, lightIndex * 2, textureSize / 4, 2);

                mShadowVertices.clear();
                const float radius = mLights[lightIndex].radius;
                for (size_t edgeIndex = 0; edgeIndex < mOccluderEdges.size(); ++edgeIndex)
                {
                    Math::Vec2 a = Math::Vec2(mOccluderEdges[edgeIndex].x,
                                            mOccluderEdges[edgeIndex].y) - mLights[lightIndex].position;
                    Math::Vec2 b = Math::Vec2(mOccluderEdges[edgeIndex].z,
                                            mOccluderEdges[edgeIndex].w) - mLights[lightIndex].position;

                    float aAlong = a.Dot(direction);
                    float bAlong = b.Dot(direction);
                    float aLateral = a.Dot(perpendicular);
                    float bLateral = b.Dot(perpendicular);

                    const float kPolyHeight = 100.0f;
                    float aDist = aAlong / radius;
                    float bDist = bAlong / radius;
                    mShadowVertices.push_back(ShadowVertex{aLateral / radius, -kPolyHeight, aDist, aDist});
                    mShadowVertices.push_back(ShadowVertex{bLateral / radius, -kPolyHeight, bDist, bDist});
                    mShadowVertices.push_back(ShadowVertex{bLateral / radius, kPolyHeight, bDist, bDist});
                    mShadowVertices.push_back(ShadowVertex{aLateral / radius, kPolyHeight, aDist, aDist});
                }

                glBufferData(GL_ARRAY_BUFFER,
                             (GLsizeiptr)(mShadowVertices.size() * sizeof(ShadowVertex)),
                             mShadowVertices.data(), GL_STREAM_DRAW);
                for (size_t i = 0; i + 3 < mShadowVertices.size(); i += 4)
                {
                    glDrawArrays(GL_TRIANGLE_FAN, (GLint)i, 4);
                }
            }
        }

        float directionalHalfSize = std::sqrt(mOrthoWidth * mOrthoWidth + mOrthoHeight * mOrthoHeight) * 0.5f;
        float directionalDistance = directionalHalfSize * 4.0f;
        Math::Vec2 canvasCenter(mOrthoWidth * 0.5f, mOrthoHeight * 0.5f);
        for (int lightIndex = 0; lightIndex < mDirectionalLightCount; ++lightIndex)
        {
            if (!mDirectionalLights[lightIndex].useShadow)
                continue;

            int row = 8 + lightIndex * 2;
            glViewport(0, row, textureSize, 2);
            glScissor(0, row, textureSize, 2);
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Math::Vec2 direction = mDirectionalLights[lightIndex].direction;
            Math::Vec2 perpendicular(-direction.y, direction.x);
            mShadowVertices.clear();
            for (size_t edgeIndex = 0; edgeIndex < mOccluderEdges.size(); ++edgeIndex)
            {
                Math::Vec2 a = Math::Vec2(mOccluderEdges[edgeIndex].x,
                                        mOccluderEdges[edgeIndex].y) - canvasCenter;
                Math::Vec2 b = Math::Vec2(mOccluderEdges[edgeIndex].z,
                                        mOccluderEdges[edgeIndex].w) - canvasCenter;
                float aLateral = a.Dot(perpendicular) / directionalHalfSize;
                float bLateral = b.Dot(perpendicular) / directionalHalfSize;
                float aDepth = (a.Dot(direction) + directionalDistance * 0.5f) / directionalDistance;
                float bDepth = (b.Dot(direction) + directionalDistance * 0.5f) / directionalDistance;

                mShadowVertices.push_back(ShadowVertex{aLateral, -1.0f, aDepth, 1.0f});
                mShadowVertices.push_back(ShadowVertex{bLateral, -1.0f, bDepth, 1.0f});
                mShadowVertices.push_back(ShadowVertex{bLateral, 1.0f, bDepth, 1.0f});
                mShadowVertices.push_back(ShadowVertex{aLateral, 1.0f, aDepth, 1.0f});
            }
            glBufferData(GL_ARRAY_BUFFER,
                         (GLsizeiptr)(mShadowVertices.size() * sizeof(ShadowVertex)),
                         mShadowVertices.data(), GL_STREAM_DRAW);
            for (size_t i = 0; i + 3 < mShadowVertices.size(); i += 4)
                glDrawArrays(GL_TRIANGLE_FAN, (GLint)i, 4);
        }

        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(previousViewport[0], previousViewport[1],
                   previousViewport[2], previousViewport[3]);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
    }

    void CanvasRenderer::FlattenOccluderEdges()
    {

        mOccluderEdges.clear();
        for (size_t o = 0; o < mOccluderCount; ++o)
        {
            const ct::Vector<Math::Vec2> &pts = *mOccluders[o].points;
            int n = (int)pts.size();
            for (int e = 0; e < n; ++e)
            {
                Math::Vec2 a = mOccluders[o].xform.Transform(pts[e]);
                Math::Vec2 b = mOccluders[o].xform.Transform(pts[(e + 1) % n]);
                mOccluderEdges.push_back(Math::Vec4(a.x, a.y, b.x, b.y));
            }
        }
    }

    void CanvasRenderer::ResetStats()
    {
        std::memset(&mStats, 0, sizeof(mStats));
    }

    void CanvasRenderer::PrintStats() const
    {
        const Stats &s = mStats;
        std::printf("=== CanvasRenderer Stats ===\n");
        std::printf("  Draw Calls:        %zu\n", s.drawCalls);
        std::printf("  Vertices Drawn:    %zu\n", s.verticesDrawn);
        std::printf("  Indices Drawn:     %zu\n", s.indicesDrawn);
        std::printf("  Texture Switches:  %zu\n", s.textureSwitches);
        std::printf("  Batches Flushed:   %zu\n", s.batchesFlushed);
        std::printf("============================\n");
    }

}
