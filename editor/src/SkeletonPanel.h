#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
class GameObject;
class Scene;
class Skeleton2D;
struct BoneAnimationClip;
}

namespace ig::retained
{
class CheckBox;
class ComboBox;
class Label;
class SpinSlider;
class TextInput;
class Timeline;
class ToolBar;
}

namespace k2d::editor
{

// The clips of the Skeleton2D on the selected object or above it: a track per
// bone and property with its keys on a timeline. Under Animate the bones show
// the clip at the playhead and a pose made in the scene view becomes keys;
// otherwise moving bones edits the rest pose the keys are measured from.
class SkeletonPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(SkeletonPanel, ig::retained::BoxLayout);

    explicit SkeletonPanel(Scene& scene);

    void show(GameObject* object);
    // Advances the preview while it plays.
    void tick(float deltaTime);

    bool animating() const { return mAnimate; }
    // Bones of every posed skeleton back at rest, and the clip shown again:
    // what the scene file and the undo steps hold is the rest pose.
    void restPose();
    void showPose();
    // Animate off, every skeleton at rest (before Play).
    void stopAnimating();

    // Keys the bone's pose at the playhead; its IK chain too.
    void keyBone(GameObject& bone);
    int ikChain() const { return mIkChain; }

    ig::retained::Signal<const char*> edited;
    ig::retained::Signal<const char*> addComponentRequested;

private:
    Skeleton2D* skeleton() const;
    BoneAnimationClip* clip() const;
    void syncClips();
    void syncClip();
    void syncTimeline();
    void applyPose();
    void keyProperty(BoneAnimationClip& clip, const char* bone, int property, float value, bool always);
    void changed(const char* label);
    void setAnimate(bool on);

    Scene& mScene;
    GameObject* mObject = nullptr;
    const Skeleton2D* mShownSkeleton = nullptr;
    int mClip = -1;
    float mTime = 0.0f;
    bool mPlaying = false;
    bool mAnimate = false;
    bool mSyncing = false;
    int mIkChain = 2;

    ig::retained::Label* mEmpty = nullptr;
    ig::retained::ToolBar* mAddSkeleton = nullptr;
    ig::retained::BoxLayout* mContent = nullptr;
    ig::retained::ComboBox* mClips = nullptr;
    ig::retained::TextInput* mName = nullptr;
    ig::retained::SpinSlider* mDuration = nullptr;
    ig::retained::CheckBox* mLoop = nullptr;
    ig::retained::ToolBar* mTools = nullptr;
    int mPlayItem = -1;
    int mAnimateItem = -1;
    ig::retained::ComboBox* mEasing = nullptr;
    ig::retained::Label* mInfo = nullptr;
    ig::retained::Timeline* mTimeline = nullptr;
};

} // namespace k2d::editor
