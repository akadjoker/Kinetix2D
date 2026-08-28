#pragma once

#include "k2d/Component.h"
#include <ct/hashmap.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>
#include <mathc.h>

#include <cstdint>
#include <functional>

namespace k2d
{

struct BoneKeyframe
{
    enum Easing : uint8_t { Linear, Step, EaseIn, EaseOut, EaseInOut };
    float time = 0.0f;
    float value = 0.0f;
    Easing easing = Linear;
};

struct BoneTrack
{
    enum Property : uint8_t { PositionX, PositionY, Rotation, ScaleX, ScaleY };
    ct::String boneName;
    Property property = Rotation;
    ct::Vector<BoneKeyframe> keyframes;

    void addKeyframe(float time, float value, BoneKeyframe::Easing easing = BoneKeyframe::Linear);
    float sample(float time) const;
};

struct BoneAnimationClip
{
    ct::String name;
    float duration = 1.0f;
    bool loop = true;
    ct::Vector<BoneTrack> tracks;
};

class Bone2D;

class Skeleton2D : public Component
{
public:
    static const ComponentType Type = ComponentType::Skeleton;

    Skeleton2D();

    void addClip(const BoneAnimationClip& clip);
    bool removeClip(const char* name);
    BoneAnimationClip* getClip(const char* name);
    const BoneAnimationClip* getClip(const char* name) const;
    size_t clipCount() const { return mClips.size(); }
    BoneAnimationClip* clipAt(size_t index)
    {
        return index < mClips.size() ? &mClips[index] : nullptr;
    }
    const BoneAnimationClip* clipAt(size_t index) const
    {
        return index < mClips.size() ? &mClips[index] : nullptr;
    }

    bool play(const char* name, bool loop = true, float speed = 1.0f);
    void stop();
    void pause();
    void resume();
    void seek(float time);

    float currentTime() const { return mTime; }
    bool playing() const { return mPlaying; }
    const char* currentAnimation() const { return mCurrent.c_str(); }
    float speed() const { return mSpeed; }
    void setSpeed(float speed) { mSpeed = speed; }

    Bone2D* findBone(const char* name) const;
    void resetToRest();

    // Solve a bone chain towards a world-space target using CCD.
    bool solveIK(const char* effectorName, const Math::Vec2& target,
                 int chainLength = 0, int iterations = 8, float tolerance = 0.5f);

    std::function<void(const char*)> onAnimationFinished;
    std::function<void(const char*)> onAnimationLooped;

protected:
    void onUpdate(float deltaTime) override;

private:
    ct::Vector<BoneAnimationClip> mClips;
    ct::String mCurrent;
    // Track lookups hit this instead of walking the subtree per track per
    // frame; the scene's topology version keeps it safe across deletions.
    ct::HashMap<ct::String, Bone2D *> mBoneCache;
    uint32_t mBoneCacheVersion = 0;
    bool mBoneCacheValid = false;
    float mTime = 0.0f;
    float mSpeed = 1.0f;
    bool mLoop = true;
    bool mPlaying = false;

    void applyClip(const BoneAnimationClip& clip);
    Bone2D* cachedBone(const ct::String& name);
};

} // namespace k2d