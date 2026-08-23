#pragma once

#include "k2d/CanvasTypes.h"

#include <glm/glm.hpp>

namespace k2d
{

    class Texture;

    class Material2D
    {
    public:
        Material2D();

        Texture *texture() const { return mTexture; }
        void setTexture(Texture *texture) { mTexture = texture; }
        const glm::vec4 &color() const { return mColor; }
        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        const glm::vec4 &sourceRect() const { return mSourceRect; }
        bool hasSourceRect() const { return mHasSourceRect; }
        void setSourceRect(float x, float y, float width, float height);
        void clearSourceRect();
        // Godot's TextureRect STRETCH_TILE: repeats the texture tileX/tileY
        // times across the sprite's size instead of stretching one copy to
        // fit (setSize already does the stretch). Needs the texture loaded
        // or created with repeat=true (GL_REPEAT wrap) or the edges clamp
        // instead of tiling. Ignored when an explicit source rect is set.
        const glm::vec2 &tiling() const { return mTiling; }
        void setTiling(float tileX, float tileY) { mTiling = glm::vec2(tileX, tileY); }
        // Godot's CanvasItem light_mask: only lights whose cullMask shares a
        // bit with this illuminate the item. Default 1 (bit 0).
        unsigned int lightMask() const { return mLightMask; }
        void setLightMask(unsigned int mask) { mLightMask = mask; }
        // Godot's CanvasTexture normal_texture: per-pixel surface normal
        // (RG = xy tilt, encoded 0..1; B/A unused here), sampled at the same
        // UV as the diffuse texture. nullptr (default) = flat lighting,
        // unchanged from before this existed.
        Texture *normalMap() const { return mNormalMap; }
        void setNormalMap(Texture *normalMap) { mNormalMap = normalMap; }
        // A GL program from CanvasRenderer::CreateShader, or 0 (default) for
        // the engine's built-in canvas shader.
        unsigned int customShader() const { return mCustomShader; }
        void setCustomShader(unsigned int program) { mCustomShader = program; }
        const glm::vec2 &pivot() const { return mPivot; }
        void setPivot(const glm::vec2 &pivot) { mPivot = pivot; }
        bool flipX() const { return mFlipX; }
        bool flipY() const { return mFlipY; }
        void setFlip(bool flipX, bool flipY) { mFlipX = flipX; mFlipY = flipY; }
        BlendMode blendMode() const { return mBlendMode; }
        void setBlendMode(BlendMode mode) { mBlendMode = mode; }

    private:
        Texture *mTexture;
        glm::vec4 mColor;
        glm::vec4 mSourceRect;
        glm::vec2 mTiling;
        glm::vec2 mPivot;
        BlendMode mBlendMode;
        bool mHasSourceRect;
        bool mFlipX;
        bool mFlipY;
        unsigned int mLightMask;
        Texture *mNormalMap;
        unsigned int mCustomShader;
    };

}
