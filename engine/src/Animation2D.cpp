#include "k2d/Animation2D.h"

#include "k2d/GameObject.h"
#include "k2d/SpriteComponent.h"
#include "k2d/Texture.h"

#include <cstring>

namespace k2d
{
    AnimationClip::AnimationClip()
        : name(), texture(nullptr), frameWidth(0), frameHeight(0), frameCount(0), frame(0),
          direction(1), framesPerSecond(0.0f), accumulator(0.0f), playing(false), mode(AnimationMode::Loop) {}

    AnimationClip::AnimationClip(const char *clipName, Texture *clipTexture, int width, int height,
                                 int count, float fps, AnimationMode clipMode)
        : name(clipName), texture(clipTexture), frameWidth(width > 0 ? width : 0),
          frameHeight(height > 0 ? height : 0), frameCount(count > 0 ? count : 0), frame(0),
          direction(1), framesPerSecond(fps > 0.0f ? fps : 0.0f), accumulator(0.0f),
          playing(false), mode(clipMode) {}

    Animation2D::Animation2D() : Component(Type, ComponentEventUpdate), mClips(), mActiveClip(-1) {}

    void Animation2D::addClip(const char *name, Texture *texture, int frameWidth, int frameHeight,
                              int frameCount, float fps, AnimationMode mode)
    {
        AnimationClip *clip = findClip(name);
        if (clip)
        {
            clip->texture = texture;
            clip->frameWidth = frameWidth > 0 ? frameWidth : 0;
            clip->frameHeight = frameHeight > 0 ? frameHeight : 0;
            clip->frameCount = frameCount > 0 ? frameCount : 0;
            clip->framesPerSecond = fps > 0.0f ? fps : 0.0f;
            clip->mode = mode;
            clip->frame = 0;
            clip->direction = 1;
            clip->accumulator = 0.0f;
            clip->playing = false;
            return;
        }
        mClips.push_back(AnimationClip(name, texture, frameWidth, frameHeight,
                                       frameCount, fps, mode));
        if (mActiveClip < 0)
            mActiveClip = 0;
    }

    bool Animation2D::play(const char *name)
    {
        AnimationClip *clip = findClip(name);
        if (!clip)
            return false;
        for (size_t i = 0; i < mClips.size(); ++i)
            if (&mClips[i] == clip)
                mActiveClip = (int)i;
        clip->frame = 0;
        clip->direction = 1;
        clip->accumulator = 0.0f;
        clip->playing = clip->frameCount > 0 && clip->framesPerSecond > 0.0f;
        applyFrame();
        return true;
    }

    const char *Animation2D::currentClip() const
    {
        const AnimationClip *clip = activeClip();
        return clip ? clip->name.c_str() : "";
    }

    void Animation2D::setSpriteSheet(Texture *texture, int frameWidth, int frameHeight,
                                     int frameCount, float fps)
    {
        addClip("default", texture, frameWidth, frameHeight, frameCount, fps, AnimationMode::Loop);
        play("default");
    }

    void Animation2D::setMode(AnimationMode mode)
    {
        AnimationClip *clip = activeClip();
        if (clip)
            clip->mode = mode;
    }

    AnimationMode Animation2D::mode() const
    {
        const AnimationClip *clip = activeClip();
        return clip ? clip->mode : AnimationMode::Loop;
    }

    void Animation2D::play()
    {
        AnimationClip *clip = activeClip();
        if (clip)
            clip->playing = clip->frameCount > 0 && clip->framesPerSecond > 0.0f;
    }

    void Animation2D::stop()
    {
        AnimationClip *clip = activeClip();
        if (clip)
            clip->playing = false;
    }

    void Animation2D::reset()
    {
        AnimationClip *clip = activeClip();
        if (!clip)
            return;
        clip->frame = 0;
        clip->direction = 1;
        clip->accumulator = 0.0f;
        applyFrame();
    }

    void Animation2D::setFrame(int frame)
    {
        AnimationClip *clip = activeClip();
        if (!clip)
            return;
        clip->frame = clip->frameCount <= 0 ? 0 : frame < 0 ? 0 : frame >= clip->frameCount ? clip->frameCount - 1 : frame;
        applyFrame();
    }

    Texture *Animation2D::texture() const { const AnimationClip *c = activeClip(); return c ? c->texture : nullptr; }
    int Animation2D::frame() const { const AnimationClip *c = activeClip(); return c ? c->frame : 0; }
    int Animation2D::frameCount() const { const AnimationClip *c = activeClip(); return c ? c->frameCount : 0; }
    float Animation2D::framesPerSecond() const { const AnimationClip *c = activeClip(); return c ? c->framesPerSecond : 0.0f; }
    bool Animation2D::playing() const { const AnimationClip *c = activeClip(); return c && c->playing; }

    void Animation2D::onUpdate(float deltaTime)
    {
        AnimationClip *clip = activeClip();
        if (!clip || !clip->playing || clip->frameCount <= 0 || clip->framesPerSecond <= 0.0f || deltaTime <= 0.0f)
            return;
        clip->accumulator += deltaTime * clip->framesPerSecond;
        while (clip->accumulator >= 1.0f)
        {
            clip->accumulator -= 1.0f;
            if (clip->mode == AnimationMode::PingPong)
            {
                if (clip->frameCount > 1)
                {
                    clip->frame += clip->direction;
                    if (clip->frame >= clip->frameCount - 1) { clip->frame = clip->frameCount - 1; clip->direction = -1; }
                    else if (clip->frame <= 0) { clip->frame = 0; clip->direction = 1; }
                }
            }
            else
            {
                ++clip->frame;
                if (clip->frame >= clip->frameCount)
                {
                    if (clip->mode == AnimationMode::Loop)
                        clip->frame = 0;
                    else { clip->frame = clip->frameCount - 1; clip->playing = false; }
                }
            }
        }
        applyFrame();
    }

    void Animation2D::applyFrame()
    {
        AnimationClip *clip = activeClip();
        if (!clip || !clip->texture || clip->frameWidth <= 0 || clip->frameHeight <= 0 || clip->frameCount <= 0 || !owner())
            return;
        SpriteComponent *sprite = owner()->getComponent<SpriteComponent>();
        if (!sprite)
            return;
        const int columns = clip->texture->Width() / clip->frameWidth;
        if (columns <= 0)
            return;
        int x = (clip->frame % columns) * clip->frameWidth;
        int y = (clip->frame / columns) * clip->frameHeight;
        sprite->setTexture(clip->texture);
        sprite->setSize(Math::Vec2((float)clip->frameWidth, (float)clip->frameHeight));
        sprite->setSourceRect((float)x, (float)y, (float)clip->frameWidth, (float)clip->frameHeight);
    }

    AnimationClip *Animation2D::activeClip() { return mActiveClip >= 0 && mActiveClip < (int)mClips.size() ? &mClips[mActiveClip] : nullptr; }
    const AnimationClip *Animation2D::activeClip() const { return mActiveClip >= 0 && mActiveClip < (int)mClips.size() ? &mClips[mActiveClip] : nullptr; }
    AnimationClip *Animation2D::findClip(const char *name)
    {
        if (!name) return nullptr;
        for (size_t i = 0; i < mClips.size(); ++i) if (std::strcmp(mClips[i].name.c_str(), name) == 0) return &mClips[i];
        return nullptr;
    }
    const AnimationClip *Animation2D::findClip(const char *name) const
    {
        if (!name) return nullptr;
        for (size_t i = 0; i < mClips.size(); ++i) if (std::strcmp(mClips[i].name.c_str(), name) == 0) return &mClips[i];
        return nullptr;
    }
}