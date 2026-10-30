#include "k2d/Animation2D.h"

#include "k2d/GameObject.h"
#include "k2d/Scene.h"
#include "k2d/SpriteComponent.h"
#include "k2d/Texture.h"

#include <cstring>

namespace k2d
{
    AnimationClip::AnimationClip()
        : name(), texture(nullptr), frameWidth(0), frameHeight(0), frameCount(0),
          atlasPadding(0.0f), atlasGap(0.0f), frame(0),
          direction(1), framesPerSecond(0.0f), accumulator(0.0f), playing(false), mode(AnimationMode::Loop) {}

    AnimationClip::AnimationClip(const char *clipName, Texture *clipTexture, int width, int height,
                                 int count, float fps, AnimationMode clipMode)
        : name(clipName), texture(clipTexture), frameWidth(width > 0 ? width : 0),
          frameHeight(height > 0 ? height : 0), frameCount(count > 0 ? count : 0),
          atlasPadding(0.0f), atlasGap(0.0f), frame(0),
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
            clip->frames.clear();
            clip->atlasPadding = Math::Vec2(0.0f);
            clip->atlasGap = Math::Vec2(0.0f);
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

    bool Animation2D::removeClip(const char *name)
    {
        for (size_t i = 0; i < mClips.size(); ++i)
        {
            if (mClips[i].name != name)
                continue;
            mClips.erase(mClips.begin() + i);
            if (mActiveClip == (int)i)
            {
                mActiveClip = mClips.empty() ? -1 : 0;
                if (mActiveClip >= 0)
                    applyFrame();
            }
            else if (mActiveClip > (int)i)
            {
                --mActiveClip;
            }
            return true;
        }
        return false;
    }

    int Animation2D::actualFrameCount(const AnimationClip &clip)
    {
        return clip.frames.empty() ? clip.frameCount : static_cast<int>(clip.frames.size());
    }

    bool Animation2D::addFrame(const char *clipName, Texture *texture, const Math::Vec4 &rect,
                               const char *texturePath)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip)
            return false;
        if (clip->frames.empty())
        {
            clip->frameWidth = 0;
            clip->frameHeight = 0;
            clip->frameCount = 0;
            // Explicit-frame clips own their texture reference per frame.
            clip->texture = nullptr;
        }
        AnimationFrame frame;
        frame.texture = texture;
        frame.texturePath = texturePath ? texturePath : "";
        frame.rect = rect;
        clip->frames.push_back(frame);
        clip->frameCount = static_cast<int>(clip->frames.size());
        if (clip->frame >= clip->frameCount)
            clip->frame = clip->frameCount - 1;
        applyFrame();
        return true;
    }

    bool Animation2D::setFrame(const char *clipName, size_t index, Texture *texture, const Math::Vec4 &rect,
                               const char *texturePath)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip || index >= clip->frames.size())
            return false;
        clip->frames[index].texture = texture;
        if (texturePath)
            clip->frames[index].texturePath = texturePath;
        clip->frames[index].rect = rect;
        if (clip == activeClip() && static_cast<size_t>(clip->frame) == index)
            applyFrame();
        return true;
    }

    bool Animation2D::setFrameOffset(const char *clipName, size_t index, const Math::Vec2 &offset)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip || index >= clip->frames.size())
            return false;
        clip->frames[index].offset = offset;
        if (clip == activeClip() && static_cast<size_t>(clip->frame) == index)
            applyFrame();
        return true;
    }

    bool Animation2D::addFramePoint(const char *clipName, size_t frameIndex, const Math::Vec2 &point)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip || frameIndex >= clip->frames.size())
            return false;
        clip->frames[frameIndex].points.push_back(point);
        return true;
    }

    bool Animation2D::setFramePoint(const char *clipName, size_t frameIndex, size_t pointIndex,
                                    const Math::Vec2 &point)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip || frameIndex >= clip->frames.size() || pointIndex >= clip->frames[frameIndex].points.size())
            return false;
        clip->frames[frameIndex].points[pointIndex] = point;
        return true;
    }

    bool Animation2D::removeFramePoint(const char *clipName, size_t frameIndex, size_t pointIndex)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip || frameIndex >= clip->frames.size() || pointIndex >= clip->frames[frameIndex].points.size())
            return false;
        ct::Vector<Math::Vec2> &points = clip->frames[frameIndex].points;
        points.erase(points.begin() + pointIndex);
        return true;
    }

    size_t Animation2D::framePointCount(const char *clipName, size_t frameIndex) const
    {
        const AnimationClip *clip = findClip(clipName);
        return clip && frameIndex < clip->frames.size() ? clip->frames[frameIndex].points.size() : 0u;
    }

    const Math::Vec2 *Animation2D::framePointAt(const char *clipName, size_t frameIndex, size_t pointIndex) const
    {
        const AnimationClip *clip = findClip(clipName);
        if (!clip || frameIndex >= clip->frames.size() || pointIndex >= clip->frames[frameIndex].points.size())
            return nullptr;
        return &clip->frames[frameIndex].points[pointIndex];
    }

    size_t Animation2D::currentFramePointCount() const
    {
        const AnimationClip *clip = activeClip();
        return clip && !clip->frames.empty() && clip->frame >= 0 && static_cast<size_t>(clip->frame) < clip->frames.size()
                   ? clip->frames[static_cast<size_t>(clip->frame)].points.size()
                   : 0u;
    }

    bool Animation2D::currentFramePoint(size_t pointIndex, Math::Vec2 &outPoint) const
    {
        const AnimationClip *clip = activeClip();
        if (!clip || clip->frames.empty() || clip->frame < 0 || static_cast<size_t>(clip->frame) >= clip->frames.size() ||
            pointIndex >= clip->frames[static_cast<size_t>(clip->frame)].points.size())
            return false;
        outPoint = clip->frames[static_cast<size_t>(clip->frame)].points[pointIndex];
        return true;
    }

    bool Animation2D::currentFrameRealPoint(size_t pointIndex, Math::Vec2 &outWorld) const
    {
        Math::Vec2 pixel;
        if (!currentFramePoint(pointIndex, pixel))
            return false;
        const GameObject *object = owner();
        const SpriteComponent *sprite = object ? object->getComponent<SpriteComponent>() : nullptr;
        if (!object || !sprite)
            return false;

        const Math::Vec2 size = sprite->size();
        if (sprite->flipX())
            pixel.x = size.x - pixel.x;
        if (sprite->flipY())
            pixel.y = size.y - pixel.y;
        const Math::Vec2 local = sprite->renderOffset() +
                                 Math::Vec2(pixel.x - sprite->pivot().x * size.x,
                                            pixel.y - sprite->pivot().y * size.y);
        outWorld = object->globalTransform().Transform(local);
        return true;
    }

    bool Animation2D::removeFrame(const char *clipName, size_t index)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip || index >= clip->frames.size())
            return false;
        clip->frames.erase(clip->frames.begin() + index);
        clip->frameCount = static_cast<int>(clip->frames.size());
        if (clip->frame >= clip->frameCount)
            clip->frame = clip->frameCount > 0 ? clip->frameCount - 1 : 0;
        if (clip->frames.empty())
            clip->playing = false;
        applyFrame();
        return true;
    }

    bool Animation2D::setClipAtlasLayout(const char *clipName, const Math::Vec2 &padding,
                                          const Math::Vec2 &gap)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip)
            return false;
        clip->atlasPadding.x = padding.x > 0.0f ? padding.x : 0.0f;
        clip->atlasPadding.y = padding.y > 0.0f ? padding.y : 0.0f;
        clip->atlasGap.x = gap.x > 0.0f ? gap.x : 0.0f;
        clip->atlasGap.y = gap.y > 0.0f ? gap.y : 0.0f;
        applyFrame();
        return true;
    }

    size_t Animation2D::frameCount(const char *clipName) const
    {
        const AnimationClip *clip = findClip(clipName);
        return clip ? (clip->frames.empty() ? 0u : clip->frames.size()) : 0u;
    }

    const AnimationFrame *Animation2D::frameAt(const char *clipName, size_t index) const
    {
        const AnimationClip *clip = findClip(clipName);
        return clip && index < clip->frames.size() ? &clip->frames[index] : nullptr;
    }

    bool Animation2D::addEvent(const char *clipName, int frame, const char *name)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip || !name)
            return false;
        AnimationEvent event;
        event.frame = frame;
        event.name = name;
        clip->events.push_back(event);
        return true;
    }

    bool Animation2D::removeEvent(const char *clipName, size_t index)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip || index >= clip->events.size())
            return false;
        clip->events.erase(clip->events.begin() + index);
        return true;
    }

    bool Animation2D::setEvent(const char *clipName, size_t index, int frame, const char *name)
    {
        AnimationClip *clip = findClip(clipName);
        if (!clip || index >= clip->events.size() || !name)
            return false;
        clip->events[index].frame = frame;
        clip->events[index].name = name;
        return true;
    }

    size_t Animation2D::eventCount(const char *clipName) const
    {
        const AnimationClip *clip = findClip(clipName);
        return clip ? clip->events.size() : 0u;
    }

    const AnimationEvent *Animation2D::eventAt(const char *clipName, size_t index) const
    {
        const AnimationClip *clip = findClip(clipName);
        return clip && index < clip->events.size() ? &clip->events[index] : nullptr;
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
        clip->playing = actualFrameCount(*clip) > 0 && clip->framesPerSecond > 0.0f;
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
            clip->playing = actualFrameCount(*clip) > 0 && clip->framesPerSecond > 0.0f;
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
        const int count = actualFrameCount(*clip);
        clip->frame = count <= 0 ? 0 : frame < 0 ? 0 : frame >= count ? count - 1 : frame;
        applyFrame();
    }

    Texture *Animation2D::texture() const { const AnimationClip *c = activeClip(); return c ? c->texture : nullptr; }
    int Animation2D::frame() const { const AnimationClip *c = activeClip(); return c ? c->frame : 0; }
    int Animation2D::frameCount() const { const AnimationClip *c = activeClip(); return c ? actualFrameCount(*c) : 0; }
    float Animation2D::framesPerSecond() const { const AnimationClip *c = activeClip(); return c ? c->framesPerSecond : 0.0f; }
    bool Animation2D::playing() const { const AnimationClip *c = activeClip(); return c && c->playing; }

    void Animation2D::onUpdate(float deltaTime)
    {
        Advance(deltaTime);
    }

    void Animation2D::Advance(float deltaTime)
    {
        AnimationClip *clip = activeClip();
        if (!clip || !clip->playing || actualFrameCount(*clip) <= 0 || clip->framesPerSecond <= 0.0f || deltaTime <= 0.0f)
            return;
        const int frameCount = actualFrameCount(*clip);
        clip->accumulator += deltaTime * clip->framesPerSecond;
        while (clip->accumulator >= 1.0f)
        {
            clip->accumulator -= 1.0f;
            if (clip->mode == AnimationMode::PingPong)
            {
                if (frameCount > 1)
                {
                    clip->frame += clip->direction;
                    if (clip->frame >= frameCount - 1) { clip->frame = frameCount - 1; clip->direction = -1; }
                    else if (clip->frame <= 0) { clip->frame = 0; clip->direction = 1; }
                    fireFrameEvents(*clip);
                }
            }
            else
            {
                ++clip->frame;
                if (clip->frame >= frameCount)
                {
                    if (clip->mode == AnimationMode::Loop)
                    {
                        clip->frame = 0;
                        fireFrameEvents(*clip);
                    }
                    else
                    {
                        clip->frame = frameCount - 1;
                        clip->playing = false;
                        fireClipFinished(*clip);
                    }
                }
                else
                {
                    fireFrameEvents(*clip);
                }
            }
        }
        applyFrame();
    }

    void Animation2D::fireFrameEvents(const AnimationClip &clip)
    {
        GameObject *object = owner();
        Scene *scene = object ? object->scene() : nullptr;
        if (!scene)
            return;
        for (size_t i = 0; i < clip.events.size(); ++i)
        {
            const AnimationEvent &event = clip.events[i];
            if (event.frame == clip.frame)
                scene->dispatchAnimationEvent(object, clip.name.c_str(), event.name.c_str(), false);
        }
    }

    void Animation2D::fireClipFinished(const AnimationClip &clip)
    {
        GameObject *object = owner();
        Scene *scene = object ? object->scene() : nullptr;
        if (!scene)
            return;
        scene->dispatchAnimationEvent(object, clip.name.c_str(), "", true);
    }

    void Animation2D::applyFrame()
    {
        AnimationClip *clip = activeClip();
        if (!clip || actualFrameCount(*clip) <= 0 || !owner())
            return;
        SpriteComponent *sprite = owner()->getComponent<SpriteComponent>();
        if (!sprite)
            return;
        if (!clip->frames.empty())
        {
            const AnimationFrame &frame = clip->frames[clip->frame];
            Texture *texture = frame.texture;
            const Math::Vec4 &rect = frame.rect;
            if (!texture)
                return;
            sprite->setTexture(texture);
            sprite->setRenderOffset(frame.offset);
            if (rect.z > 0.0f && rect.w > 0.0f)
            {
                sprite->setSize(Math::Vec2(rect.z, rect.w));
                sprite->setSourceRect(rect.x, rect.y, rect.z, rect.w);
            }
            else
            {
                sprite->setSize(Math::Vec2((float)texture->Width(), (float)texture->Height()));
                sprite->clearSourceRect();
            }
            return;
        }
        if (!clip->texture || clip->frameWidth <= 0 || clip->frameHeight <= 0)
            return;
        const int columns = (int)((clip->texture->Width() - clip->atlasPadding.x * 2.0f + clip->atlasGap.x) /
                                  (clip->frameWidth + clip->atlasGap.x));
        if (columns <= 0)
            return;
        int x = (int)(clip->atlasPadding.x + (clip->frame % columns) * (clip->frameWidth + clip->atlasGap.x));
        int y = (int)(clip->atlasPadding.y + (clip->frame / columns) * (clip->frameHeight + clip->atlasGap.y));
        sprite->setTexture(clip->texture);
        sprite->setRenderOffset(Math::Vec2(0.0f));
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
