#include "k2d/Skeleton2D.h"
#include "k2d/Utils.h"
#include "k2d/Scene.h"

#include "k2d/Bone2D.h"
#include "k2d/GameObject.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace k2d
{

void BoneTrack::addKeyframe(float time, float value, BoneKeyframe::Easing easing)
{
    BoneKeyframe keyframe{time, value, easing};
    size_t index = 0;
    while (index < keyframes.size() && keyframes[index].time < time)
        ++index;
    keyframes.insert(keyframes.begin() + index, keyframe);
}

static float applyEasing(float value, BoneKeyframe::Easing easing)
{
    switch (easing)
    {
    case BoneKeyframe::EaseIn: return value * value;
    case BoneKeyframe::EaseOut: return value * (2.0f - value);
    case BoneKeyframe::EaseInOut:
        return value < 0.5f ? 2.0f * value * value : -1.0f + (4.0f - 2.0f * value) * value;
    default: return value;
    }
}

float BoneTrack::sample(float time) const
{
    if (keyframes.empty())
        return 0.0f;
    if (keyframes.size() == 1 || time <= keyframes.front().time)
        return keyframes.front().value;
    if (time >= keyframes.back().time)
        return keyframes.back().value;

    for (size_t index = 0; index + 1 < keyframes.size(); ++index)
    {
        const BoneKeyframe& first = keyframes[index];
        const BoneKeyframe& second = keyframes[index + 1];
        if (time > second.time)
            continue;
        if (first.easing == BoneKeyframe::Step)
            return first.value;
        const float span = second.time - first.time;
        if (span <= 0.000001f)
            return first.value;
        const float t = applyEasing((time - first.time) / span, first.easing);
        if (property == Rotation)
        {
            float difference = second.value - first.value;
            while (difference > 180.0f) difference -= 360.0f;
            while (difference < -180.0f) difference += 360.0f;
            return first.value + t * difference;
        }
        return first.value + t * (second.value - first.value);
    }
    return keyframes.back().value;
}

Skeleton2D::Skeleton2D() : Component(Type, ComponentEventUpdate) {}

void Skeleton2D::addClip(const BoneAnimationClip& clip)
{
    BoneAnimationClip* existing = getClip(clip.name.c_str());
    if (existing)
        *existing = clip;
    else
        mClips.push_back(clip);
}

bool Skeleton2D::removeClip(const char* name)
{
    if (!name)
        return false;
    const size_t oldSize = mClips.size();
    mClips.erase(std::remove_if(mClips.begin(), mClips.end(),
        [name](const BoneAnimationClip& clip) { return clip.name == name; }), mClips.end());
    return oldSize != mClips.size();
}

BoneAnimationClip* Skeleton2D::getClip(const char* name)
{
    if (!name)
        return nullptr;
    for (BoneAnimationClip& clip : mClips)
        if (clip.name == name) return &clip;
    return nullptr;
}

const BoneAnimationClip* Skeleton2D::getClip(const char* name) const
{
    if (!name)
        return nullptr;
    for (const BoneAnimationClip& clip : mClips)
        if (clip.name == name) return &clip;
    return nullptr;
}

bool Skeleton2D::play(const char* name, bool loop, float speed)
{
    const BoneAnimationClip* clip = getClip(name);
    if (!clip)
        return false;
    resetToRest();
    mCurrent = name;
    mTime = 0.0f;
    mLoop = loop;
    mSpeed = speed;
    mPlaying = true;
    applyClip(*clip);
    return true;
}

void Skeleton2D::stop()
{
    mPlaying = false;
    mTime = 0.0f;
    resetToRest();
}

void Skeleton2D::pause() { mPlaying = false; }
void Skeleton2D::resume() { mPlaying = !mCurrent.empty(); }

void Skeleton2D::seek(float time)
{
    const BoneAnimationClip* clip = getClip(mCurrent.c_str());
    if (!clip)
        return;
    const float duration = Max(0.0f, clip->duration);
    mTime = Max(0.0f, Min(time, duration));
    applyClip(*clip);
}

static Bone2D* findBoneRecursive(GameObject* object, const char* name)
{
    if (!object)
        return nullptr;
    Bone2D* bone = object->getComponent<Bone2D>();
    if (bone && name && std::strcmp(object->name().c_str(), name) == 0)
        return bone;
    for (size_t index = 0; index < object->childCount(); ++index)
    {
        bone = findBoneRecursive(object->child(index), name);
        if (bone)
            return bone;
    }
    return nullptr;
}

Bone2D* Skeleton2D::findBone(const char* name) const
{
    return owner() ? findBoneRecursive(owner(), name) : nullptr;
}

static void collectBonesRecursive(GameObject* object, ct::HashMap<ct::String, Bone2D*>& out)
{
    if (Bone2D* bone = object->getComponent<Bone2D>())
        if (!out.find(object->name()))
            out.put(object->name(), bone);
    for (size_t index = 0; index < object->childCount(); ++index)
        collectBonesRecursive(object->child(index), out);
}

Bone2D* Skeleton2D::cachedBone(const ct::String& name)
{
    Scene* scene = owner() ? owner()->scene() : nullptr;
    if (!scene)
        return findBone(name.c_str());
    if (!mBoneCacheValid || mBoneCacheVersion != scene->topologyVersion())
    {
        mBoneCache.clear();
        collectBonesRecursive(owner(), mBoneCache);
        mBoneCacheVersion = scene->topologyVersion();
        mBoneCacheValid = true;
    }
    Bone2D** bone = mBoneCache.find(name);
    return bone ? *bone : nullptr;
}

static Math::Vec2 boneTip(const Bone2D* bone)
{
    return bone->owner()->globalTransform().Transform(bone->length(), 0.0f);
}

bool Skeleton2D::solveIK(const char* effectorName, const Math::Vec2& target,
                         int chainLength, int iterations, float tolerance)
{
    Bone2D* effector = findBone(effectorName);
    if (!effector || !effector->owner() || iterations <= 0)
        return false;

    ct::Vector<Bone2D*> chain;
    for (GameObject* object = effector->owner(); object; object = object->parent())
    {
        if (Bone2D* bone = object->getComponent<Bone2D>())
            chain.push_back(bone);
        if (object == owner())
            break;
    }
    if (chain.empty())
        return false;
    if (chainLength > 0 && chainLength < static_cast<int>(chain.size()))
        chain.resize(static_cast<size_t>(chainLength));

    const float maxError = tolerance > 0.0f ? tolerance : 0.0f;
    for (int iteration = 0; iteration < iterations; ++iteration)
    {
        if (std::hypot(boneTip(effector).x - target.x, boneTip(effector).y - target.y) <= maxError)
            break;

        for (Bone2D* bone : chain)
        {
            GameObject* joint = bone->owner();
            const Math::Vec2 jointPosition = joint->globalPosition();
            const Math::Vec2 endPosition = boneTip(effector);
            const Math::Vec2 toEnd(endPosition.x - jointPosition.x, endPosition.y - jointPosition.y);
            const Math::Vec2 toTarget(target.x - jointPosition.x, target.y - jointPosition.y);
            const float endLength = std::hypot(toEnd.x, toEnd.y);
            const float targetLength = std::hypot(toTarget.x, toTarget.y);
            if (endLength <= 0.0001f || targetLength <= 0.0001f)
                continue;

            const float cross = toEnd.x * toTarget.y - toEnd.y * toTarget.x;
            const float dot = toEnd.x * toTarget.x + toEnd.y * toTarget.y;
            const float deltaDegrees = std::atan2(cross, dot) * 57.295779513f;
            joint->setRotationDegrees(joint->rotationDegrees() + deltaDegrees);

            if (std::hypot(boneTip(effector).x - target.x, boneTip(effector).y - target.y) <= maxError)
                break;
        }
    }
    return true;
}

static void resetBonesRecursive(GameObject* object)
{
    if (!object)
        return;
    if (Bone2D* bone = object->getComponent<Bone2D>())
        bone->resetToRest();
    for (size_t index = 0; index < object->childCount(); ++index)
        resetBonesRecursive(object->child(index));
}

void Skeleton2D::resetToRest()
{
    if (owner())
        resetBonesRecursive(owner());
}

void Skeleton2D::applyClip(const BoneAnimationClip& clip)
{
    for (const BoneTrack& track : clip.tracks)
    {
        Bone2D* bone = cachedBone(track.boneName);
        if (!bone || !bone->owner())
            continue;
        const float value = track.sample(mTime);
        GameObject* object = bone->owner();
        switch (track.property)
        {
        case BoneTrack::PositionX:
            object->setPosition(Math::Vec2(bone->restPosition().x + value, object->position().y));
            break;
        case BoneTrack::PositionY:
            object->setPosition(Math::Vec2(object->position().x, bone->restPosition().y + value));
            break;
        case BoneTrack::Rotation:
            object->setRotationDegrees(bone->restRotationDegrees() + value);
            break;
        case BoneTrack::ScaleX:
            object->setScale(Math::Vec2(bone->restScale().x * value, object->scale().y));
            break;
        case BoneTrack::ScaleY:
            object->setScale(Math::Vec2(object->scale().x, bone->restScale().y * value));
            break;
        }
    }
}

void Skeleton2D::onUpdate(float deltaTime)
{
    if (!mPlaying || mCurrent.empty())
        return;
    const BoneAnimationClip* clip = getClip(mCurrent.c_str());
    if (!clip || clip->duration <= 0.0f)
        return;

    mTime += deltaTime * mSpeed;
    bool looped = false;
    if (mSpeed >= 0.0f && mTime >= clip->duration)
    {
        if (mLoop)
        {
            mTime = std::fmod(mTime, clip->duration);
            looped = true;
        }
        else
        {
            mTime = clip->duration;
            mPlaying = false;
            applyClip(*clip);
            if (onAnimationFinished) onAnimationFinished(mCurrent.c_str());
            return;
        }
    }
    else if (mSpeed < 0.0f && mTime < 0.0f)
    {
        if (mLoop)
        {
            mTime = clip->duration + std::fmod(mTime, clip->duration);
            looped = true;
        }
        else
        {
            mTime = 0.0f;
            mPlaying = false;
            applyClip(*clip);
            if (onAnimationFinished) onAnimationFinished(mCurrent.c_str());
            return;
        }
    }
    applyClip(*clip);
    if (looped && onAnimationLooped)
        onAnimationLooped(mCurrent.c_str());
}

} // namespace k2d
