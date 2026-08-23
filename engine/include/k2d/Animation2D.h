#pragma once

#include "k2d/Component.h"

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{

    class Texture;

    enum class AnimationMode
    {
        OneShot,
        Loop,
        PingPong
    };

    struct AnimationClip
    {
        ct::String name;
        Texture *texture;
        int frameWidth;
        int frameHeight;
        int frameCount;
        int frame;
        int direction;
        float framesPerSecond;
        float accumulator;
        bool playing;
        AnimationMode mode;

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
        bool play(const char *name);
        const char *currentClip() const;
        size_t clipCount() const { return mClips.size(); }

        const AnimationClip *clipAt(size_t index) const
        {
            return index < mClips.size() ? &mClips[index] : nullptr;
        }

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
        void applyFrame();

        AnimationClip *activeClip();
        const AnimationClip *activeClip() const;
        AnimationClip *findClip(const char *name);
        const AnimationClip *findClip(const char *name) const;

        ct::Vector<AnimationClip> mClips;
        int mActiveClip;
    };

}