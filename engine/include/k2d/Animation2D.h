#pragma once

#include "k2d/Component.h"

#include <ct/string.hpp>
#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{

    class Texture;

    enum class AnimationMode
    {
        OneShot,
        Loop,
        PingPong
    };

    // A frame can be a rectangle from an atlas or a complete, independent image.
    // A zero-sized rect means the whole texture.
    struct AnimationFrame
    {
        Texture *texture = nullptr;
        // Kept with the frame so a scene can restore it even when the editor
        // has not kept the texture pointer in its transient asset cache.
        ct::String texturePath;
        Math::Vec4 rect = Math::Vec4(0.0f);
        Math::Vec2 offset = Math::Vec2(0.0f);
    };

    struct AnimationEvent
    {
        int frame = 0;
        ct::String name;
    };

    struct AnimationClip
    {
        ct::String name;
        Texture *texture;
        int frameWidth;
        int frameHeight;
        int frameCount;
        Math::Vec2 atlasPadding;
        Math::Vec2 atlasGap;
        int frame;
        int direction;
        float framesPerSecond;
        float accumulator;
        bool playing;
        AnimationMode mode;
        ct::Vector<AnimationFrame> frames;
        ct::Vector<AnimationEvent> events;

        AnimationClip();
        AnimationClip(const char *clipName, Texture *clipTexture, int width, int height,
                      int count, float fps, AnimationMode clipMode);
    };

    class Animation2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Animation;

        Animation2D();

        void addClip(const char *name, Texture *texture, int frameWidth, int frameHeight,
                     int frameCount, float framesPerSecond,
                     AnimationMode mode = AnimationMode::Loop);
        bool removeClip(const char *name);
        bool addFrame(const char *clipName, Texture *texture, const Math::Vec4 &rect = Math::Vec4(0.0f),
                      const char *texturePath = nullptr);
        bool setFrame(const char *clipName, size_t index, Texture *texture, const Math::Vec4 &rect,
                      const char *texturePath = nullptr);
        bool setFrameOffset(const char *clipName, size_t index, const Math::Vec2 &offset);
        bool removeFrame(const char *clipName, size_t index);
        bool setClipAtlasLayout(const char *clipName, const Math::Vec2 &padding, const Math::Vec2 &gap);
        bool play(const char *name);
        const char *currentClip() const;
        size_t clipCount() const { return mClips.size(); }

        AnimationClip *clipAt(size_t index)
        {
            return index < mClips.size() ? &mClips[index] : nullptr;
        }
        const AnimationClip *clipAt(size_t index) const
        {
            return index < mClips.size() ? &mClips[index] : nullptr;
        }
        size_t frameCount(const char *clipName) const;
        const AnimationFrame *frameAt(const char *clipName, size_t index) const;

        bool addEvent(const char *clipName, int frame, const char *name);
        bool removeEvent(const char *clipName, size_t index);
        bool setEvent(const char *clipName, size_t index, int frame, const char *name);
        size_t eventCount(const char *clipName) const;
        const AnimationEvent *eventAt(const char *clipName, size_t index) const;

        void setSpriteSheet(Texture *texture, int frameWidth, int frameHeight,
                            int frameCount, float framesPerSecond);
        void setMode(AnimationMode mode);
        AnimationMode mode() const;
        void setLoop(bool loop) { setMode(loop ? AnimationMode::Loop : AnimationMode::OneShot); }
        void play();
        void stop();
        void reset();
        void setFrame(int frame);

        Texture *texture() const;
        int frame() const;
        int frameCount() const;
        float framesPerSecond() const;
        bool playing() const;
        bool loop() const { return mode() == AnimationMode::Loop; }

        void Advance(float deltaTime);

    protected:
        void onUpdate(float deltaTime) override;

    private:
        static int actualFrameCount(const AnimationClip &clip);
        void applyFrame();
        void fireFrameEvents(const AnimationClip &clip);
        void fireClipFinished(const AnimationClip &clip);

        AnimationClip *activeClip();
        const AnimationClip *activeClip() const;
        AnimationClip *findClip(const char *name);
        const AnimationClip *findClip(const char *name) const;

        ct::Vector<AnimationClip> mClips;
        int mActiveClip;
    };

}
