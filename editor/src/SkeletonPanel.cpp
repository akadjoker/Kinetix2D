#include "SkeletonPanel.h"

#include "Icons.h"
#include "PropertyFields.h"

#include "BasicWidgets.hpp"
#include "ComboBox.hpp"
#include "SpinSlider.hpp"
#include "TextInputWidgets.hpp"
#include "Timeline.hpp"
#include "ToolBar.hpp"

#include <k2d/Bone2D.h>
#include <k2d/GameObject.h>
#include <k2d/Scene.h>
#include <k2d/Skeleton2D.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{

const char* const kPropertyNames[] = {"x", "y", "rotation", "scale x", "scale y"};
constexpr float kSameTime = 0.0001f;
constexpr float kFramesPerSecond = 30.0f;

class ClipName : public rt::TextInput
{
public:
    IG_WIDGET_CLASS(ClipName, rt::TextInput);

    using rt::TextInput::TextInput;

    Vec2f sizeHint() const override { return {140.0f, rt::TextInput::sizeHint().y}; }
};

Skeleton2D* skeletonOf(GameObject* object)
{
    for (; object; object = object->parent())
        if (Skeleton2D* skeleton = object->getComponent<Skeleton2D>())
            return skeleton;
    return nullptr;
}

template <class F> void forEachSkeleton(GameObject& object, const F& f)
{
    if (Skeleton2D* skeleton = object.getComponent<Skeleton2D>())
        f(*skeleton);
    for (std::size_t i = 0; i < object.childCount(); ++i)
        forEachSkeleton(*object.child(i), f);
}

void saveRest(GameObject& object)
{
    if (Bone2D* bone = object.getComponent<Bone2D>())
        bone->saveRestPose();
    for (std::size_t i = 0; i < object.childCount(); ++i)
        saveRest(*object.child(i));
}

// A skeleton has a clip current only while this panel shows it posed.
bool posed(const Skeleton2D& skeleton)
{
    return skeleton.currentAnimation()[0] != '\0';
}

} // namespace

SkeletonPanel::SkeletonPanel(Scene& scene) : rt::BoxLayout(rt::LayoutDir::Vertical), mScene(scene)
{
    setPadding(0.0f);
    setSpacing(0.0f);

    mEmpty = createChild<rt::Label>("Select a skeleton or one of its bones");
    mEmpty->setMargins(12.0f);
    mAddSkeleton = iconButton(this, "Skeleton", "Add a Skeleton2D to this object",
                              [this]() { addComponentRequested.emit("Skeleton2D"); });

    mContent = createChild<rt::BoxLayout>(rt::LayoutDir::Vertical);
    mContent->setStretch(1.0f);
    mContent->setPadding(4.0f);
    mContent->setSpacing(4.0f);

    auto* bar = mContent->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    bar->setPadding(0.0f);
    bar->setSpacing(6.0f);
    auto* clipTools = bar->createChild<rt::ToolBar>();
    clipTools->setDraggable(false);
    const int addClip = clipTools->addButton(Icons::get("Add"), "New clip");
    const int duplicateClip = clipTools->addButton(Icons::get("Duplicate"), "Duplicate the clip");
    const int removeClip = clipTools->addButton(Icons::get("Remove"), "Delete the clip");
    clipTools->buttonClicked.connect([this, addClip, duplicateClip, removeClip](int item)
    {
        Skeleton2D* s = skeleton();
        if (!s)
            return;
        BoneAnimationClip* current = clip();
        if (item == addClip || item == duplicateClip)
        {
            if (item == duplicateClip && !current)
                return;
            BoneAnimationClip made;
            if (item == duplicateClip)
                made = *current;
            char name[64];
            for (int n = static_cast<int>(s->clipCount()) + 1;; ++n)
            {
                std::snprintf(name, sizeof name, "%s_%d", item == addClip ? "clip" : current->name.c_str(), n);
                if (!s->getClip(name))
                    break;
            }
            made.name = name;
            s->addClip(made);
            mClip = static_cast<int>(s->clipCount()) - 1;
            mTime = 0.0f;
            changed(item == addClip ? "New clip" : "Duplicate clip");
        }
        else if (item == removeClip && current)
        {
            const ct::String name = current->name;
            s->clearAnimation();
            s->removeClip(name.c_str());
            mClip = mClip > 0 ? mClip - 1 : (s->clipCount() > 0 ? 0 : -1);
            changed("Delete clip");
        }
    });
    mClips = bar->createChild<rt::ComboBox>();
    mClips->selectionChanged.connect([this](int index)
    {
        if (mSyncing)
            return;
        if (Skeleton2D* s = skeleton())
            s->clearAnimation();
        mClip = index;
        mTime = 0.0f;
        syncClip();
        applyPose();
    });
    mName = bar->createChild<ClipName>("");
    mName->setPlaceholder("Clip name");
    mName->submitted.connect([this](const rt::String& name)
    {
        Skeleton2D* s = skeleton();
        BoneAnimationClip* current = clip();
        if (!current || name.empty() || name == current->name)
            return;
        if (s->getClip(name.c_str()))
        {
            mName->setText(current->name);
            return;
        }
        if (s->autoplay() == current->name)
            s->setAutoplay(name.c_str());
        s->clearAnimation();
        current->name = name;
        changed("Rename clip");
    });
    NumberSpec durationSpec;
    durationSpec.step = 0.05;
    durationSpec.minimum = 0.05;
    durationSpec.maximum = 600.0;
    durationSpec.width = 130.0f;
    mDuration = addNumberField(bar, "length", 1.0, durationSpec, [this](double value, bool final)
    {
        BoneAnimationClip* current = clip();
        if (!current || mSyncing)
            return;
        current->duration = static_cast<float>(value);
        mTimeline->setTimeRange(0.0f, current->duration);
        if (final)
            changed("Clip length");
    });
    mLoop = bar->createChild<rt::CheckBox>("Loop");
    mLoop->toggled.connect([this](bool on)
    {
        if (BoneAnimationClip* current = clip(); current && !mSyncing)
        {
            current->loop = on;
            changed("Clip loop");
        }
    });

    mTools = bar->createChild<rt::ToolBar>();
    mTools->setDraggable(false);
    mPlayItem = mTools->addToggle(Icons::get("Play"), "Play the clip");
    mAnimateItem = mTools->addToggle(rt::IconId::Record,
                                     "Animate: the bones show the clip at the playhead, and posing them in the scene "
                                     "view keys them (drag a bone to turn it, its tip to pull its chain)");
    const int keyItem = mTools->addButton(Icons::get("Bone"), "Key the selected bone at the playhead");
    const int removeKeyItem = mTools->addButton(Icons::get("Remove"), "Delete the selected keys");
    mTools->toggleChanged.connect([this](int item, bool on)
    {
        if (item == mPlayItem)
        {
            if (on && !mAnimate)
                setAnimate(true);
            mPlaying = on && mAnimate;
            mTools->setItemChecked(mPlayItem, mPlaying);
        }
        else if (item == mAnimateItem)
            setAnimate(on);
    });
    mTools->buttonClicked.connect([this, keyItem, removeKeyItem](int item)
    {
        if (item == keyItem && mObject && mObject->getComponent<Bone2D>())
        {
            keyBone(*mObject);
            changed("Key bone");
        }
        else if (item == removeKeyItem)
        {
            BoneAnimationClip* current = clip();
            ct::Vector<rt::TimelineKeyRef> keys;
            mTimeline->selectedKeys(keys);
            if (!current || keys.empty())
                return;
            // Highest index first, so the ones still to go keep theirs.
            for (std::size_t i = 0; i < keys.size(); ++i)
                for (std::size_t j = i + 1; j < keys.size(); ++j)
                    if (keys[j].track > keys[i].track || (keys[j].track == keys[i].track && keys[j].key > keys[i].key))
                    {
                        const rt::TimelineKeyRef t = keys[i];
                        keys[i] = keys[j];
                        keys[j] = t;
                    }
            for (const rt::TimelineKeyRef& ref : keys)
            {
                BoneTrack& track = current->tracks[static_cast<std::size_t>(ref.track)];
                track.keyframes.erase(track.keyframes.begin() + ref.key);
            }
            for (std::size_t t = current->tracks.size(); t > 0; --t)
                if (current->tracks[t - 1].keyframes.empty())
                    current->tracks.erase(current->tracks.begin() + static_cast<std::ptrdiff_t>(t - 1));
            changed("Delete keys");
        }
    });
    NumberSpec chainSpec;
    chainSpec.integer = true;
    chainSpec.step = 1.0;
    chainSpec.minimum = 1.0;
    chainSpec.maximum = 16.0;
    chainSpec.width = 110.0f;
    addNumberField(bar, "IK", mIkChain, chainSpec, [this](double value, bool)
    {
        mIkChain = static_cast<int>(value);
    });
    mEasing = bar->createChild<rt::ComboBox>();
    mEasing->addItem("Linear");
    mEasing->addItem("Step");
    mEasing->addItem("Ease in");
    mEasing->addItem("Ease out");
    mEasing->addItem("Ease in out");
    mEasing->selectionChanged.connect([this](int index)
    {
        BoneAnimationClip* current = clip();
        ct::Vector<rt::TimelineKeyRef> keys;
        mTimeline->selectedKeys(keys);
        if (mSyncing || !current || keys.empty() || index < 0)
            return;
        for (const rt::TimelineKeyRef& ref : keys)
            current->tracks[static_cast<std::size_t>(ref.track)].keyframes[static_cast<std::size_t>(ref.key)].easing =
                static_cast<BoneKeyframe::Easing>(index);
        changed("Key easing");
    });
    mInfo = bar->createChild<rt::Label>("");
    mInfo->setStretch(1.0f);
    mInfo->setLayoutAlign(rt::Align::Center);

    mTimeline = mContent->createChild<rt::Timeline>();
    mTimeline->setStretch(1.0f);
    mTimeline->setHeaderWidth(170.0f);
    mTimeline->setFrameRate(kFramesPerSecond);
    mTimeline->setSnapping(true);
    mTimeline->onPlayheadChanged.connect([this](float time)
    {
        // On whole frames, so keys made there line up across tracks.
        mTime = std::floor(time * kFramesPerSecond + 0.5f) / kFramesPerSecond;
        applyPose();
    });
    mTimeline->onKeyframeSelected.connect([this](int track, int key)
    {
        BoneAnimationClip* current = clip();
        if (!current || track < 0 || track >= static_cast<int>(current->tracks.size()))
            return;
        const BoneTrack& t = current->tracks[static_cast<std::size_t>(track)];
        if (key < 0 || key >= static_cast<int>(t.keyframes.size()))
            return;
        mSyncing = true;
        mEasing->setSelectedIndex(static_cast<int>(t.keyframes[static_cast<std::size_t>(key)].easing));
        mSyncing = false;
    });
    mTimeline->onKeyframeMoved.connect([this](int track, int key, float time)
    {
        BoneAnimationClip* current = clip();
        if (!current || track < 0 || track >= static_cast<int>(current->tracks.size()))
            return;
        BoneTrack& t = current->tracks[static_cast<std::size_t>(track)];
        if (key < 0 || key >= static_cast<int>(t.keyframes.size()))
            return;
        t.keyframes[static_cast<std::size_t>(key)].time = time < 0.0f ? 0.0f : (time > current->duration ? current->duration : time);
        applyPose();
    });
    mTimeline->onKeyframeDropped.connect([this](int, int, float, float)
    {
        BoneAnimationClip* current = clip();
        if (!current)
            return;
        // Keys back in time order, as sampling expects.
        for (BoneTrack& track : current->tracks)
            for (std::size_t i = 1; i < track.keyframes.size(); ++i)
                for (std::size_t j = i; j > 0 && track.keyframes[j].time < track.keyframes[j - 1].time; --j)
                {
                    const BoneKeyframe k = track.keyframes[j];
                    track.keyframes[j] = track.keyframes[j - 1];
                    track.keyframes[j - 1] = k;
                }
        changed("Move keys");
    });
    show(nullptr);
}

Skeleton2D* SkeletonPanel::skeleton() const
{
    return skeletonOf(mObject);
}

BoneAnimationClip* SkeletonPanel::clip() const
{
    Skeleton2D* s = skeleton();
    return s && mClip >= 0 ? s->clipAt(static_cast<std::size_t>(mClip)) : nullptr;
}

void SkeletonPanel::show(GameObject* object)
{
    // The object shown before may be gone (undo, a scene opened): only the
    // address of its skeleton is compared, never followed.
    Skeleton2D* s = skeletonOf(object);
    if (s != mShownSkeleton)
        mClip = 0;
    mShownSkeleton = s;
    mObject = object;
    mEmpty->setVisible(!s);
    mEmpty->setText(object ? "Neither this object nor one above it has a Skeleton2D"
                           : "Select a skeleton or one of its bones");
    mAddSkeleton->setVisible(object && !s);
    mContent->setVisible(s != nullptr);
    if (s && (mClip < 0 || mClip >= static_cast<int>(s->clipCount())))
        mClip = s->clipCount() > 0 ? 0 : -1;
    syncClips();
    syncClip();
    applyPose();
}

void SkeletonPanel::syncClips()
{
    mSyncing = true;
    mClips->clear();
    if (Skeleton2D* s = skeleton())
        for (std::size_t i = 0; i < s->clipCount(); ++i)
            mClips->addItem(s->clipAt(i)->name);
    mClips->setSelectedIndex(mClip);
    mSyncing = false;
}

void SkeletonPanel::syncClip()
{
    mSyncing = true;
    BoneAnimationClip* current = clip();
    mName->setText(current ? current->name : ct::String());
    mDuration->setValueNoSignal(current ? current->duration : 1.0);
    mLoop->setChecked(current ? current->loop : true);
    mName->setEnabled(current != nullptr);
    mDuration->setEnabled(current != nullptr);
    mLoop->setEnabled(current != nullptr);
    mTools->setItemChecked(mAnimateItem, mAnimate);
    mTools->setItemChecked(mPlayItem, mPlaying);
    if (current && mTime > current->duration)
        mTime = current->duration;
    char info[160];
    if (!current)
        std::snprintf(info, sizeof info, "%s", skeleton() ? "No clip: make one with +" : "");
    else if (mObject && mObject->getComponent<Bone2D>())
        std::snprintf(info, sizeof info, "%s  %.2f s", mObject->name().c_str(), mTime);
    else
        std::snprintf(info, sizeof info, "%.2f s: select a bone to key it", mTime);
    mInfo->setText(info);
    mSyncing = false;
    syncTimeline();
}

void SkeletonPanel::syncTimeline()
{
    BoneAnimationClip* current = clip();
    mTimeline->clearTracks();
    if (!current)
        return;
    for (const BoneTrack& track : current->tracks)
    {
        char name[96];
        std::snprintf(name, sizeof name, "%s  %s", track.boneName.c_str(), kPropertyNames[track.property]);
        const int id = mTimeline->addTrack(name);
        for (const BoneKeyframe& key : track.keyframes)
            mTimeline->addKeyframe(id, key.time);
    }
    mTimeline->setTimeRange(0.0f, current->duration > 0.0f ? current->duration : 1.0f);
    mTimeline->setPlayhead(mTime);
}

void SkeletonPanel::applyPose()
{
    Skeleton2D* s = skeleton();
    BoneAnimationClip* current = clip();
    if (!mAnimate || !s || !current)
        return;
    if (current->name != s->currentAnimation())
    {
        s->play(current->name.c_str(), current->loop, s->speed());
        s->pause();
    }
    s->seek(mTime);
}

void SkeletonPanel::setAnimate(bool on)
{
    Skeleton2D* s = skeleton();
    if (on && (!s || !clip()))
        on = false;
    if (on && !mAnimate)
    {
        // What the bones show now is the pose the keys are measured from.
        forEachSkeleton(mScene.root(), [](Skeleton2D& other)
        {
            if (posed(other))
                other.clearAnimation();
        });
        if (s->owner())
            saveRest(*s->owner());
    }
    mAnimate = on;
    if (!on)
    {
        mPlaying = false;
        forEachSkeleton(mScene.root(), [](Skeleton2D& other)
        {
            if (posed(other))
                other.clearAnimation();
        });
    }
    mTools->setItemChecked(mAnimateItem, mAnimate);
    mTools->setItemChecked(mPlayItem, mPlaying);
    applyPose();
}

void SkeletonPanel::stopAnimating()
{
    if (mAnimate)
        setAnimate(false);
}

void SkeletonPanel::restPose()
{
    forEachSkeleton(mScene.root(), [](Skeleton2D& s)
    {
        if (posed(s))
            s.resetToRest();
    });
}

void SkeletonPanel::showPose()
{
    forEachSkeleton(mScene.root(), [](Skeleton2D& s)
    {
        if (posed(s))
            s.seek(s.currentTime());
    });
}

void SkeletonPanel::tick(float deltaTime)
{
    BoneAnimationClip* current = clip();
    if (!mPlaying || !mAnimate || !current || current->duration <= 0.0f || !mContent->isVisible())
        return;
    mTime += deltaTime;
    if (mTime > current->duration)
        mTime = current->loop ? std::fmod(mTime, current->duration) : current->duration;
    mTimeline->setPlayhead(mTime);
    applyPose();
}

void SkeletonPanel::keyProperty(BoneAnimationClip& c, const char* bone, int property, float value, bool always)
{
    BoneTrack* track = nullptr;
    for (BoneTrack& t : c.tracks)
        if (t.property == property && t.boneName == bone)
            track = &t;
    // A turn keeps the angle nearest the one the clip has there, so the bone
    // never swings the long way round between keys.
    if (property == BoneTrack::Rotation)
    {
        const float reference = track ? track->sample(mTime) : 0.0f;
        while (value - reference > 180.0f)
            value -= 360.0f;
        while (value - reference < -180.0f)
            value += 360.0f;
    }
    if (!track)
    {
        const float neutral = property >= BoneTrack::ScaleX ? 1.0f : 0.0f;
        if (!always && std::fabs(value - neutral) < 0.001f)
            return;
        BoneTrack made;
        made.boneName = bone;
        made.property = static_cast<BoneTrack::Property>(property);
        c.tracks.push_back(made);
        track = &c.tracks[c.tracks.size() - 1];
    }
    for (BoneKeyframe& key : track->keyframes)
        if (std::fabs(key.time - mTime) < kSameTime)
        {
            key.value = value;
            return;
        }
    track->addKeyframe(mTime, value);
}

void SkeletonPanel::keyBone(GameObject& boneObject)
{
    Skeleton2D* s = skeletonOf(&boneObject);
    BoneAnimationClip* current = clip();
    if (!mAnimate || !s || s != skeleton() || !current)
        return;
    GameObject* object = &boneObject;
    for (int n = 0; object && n < mIkChain; ++n)
    {
        const Bone2D* bone = object->getComponent<Bone2D>();
        if (!bone)
            break;
        const char* name = object->name().c_str();
        const Math::Vec2 rest = bone->restPosition();
        const Math::Vec2 restScale = bone->restScale();
        keyProperty(*current, name, BoneTrack::Rotation, object->rotationDegrees() - bone->restRotationDegrees(),
                    n == 0);
        keyProperty(*current, name, BoneTrack::PositionX, object->position().x - rest.x, false);
        keyProperty(*current, name, BoneTrack::PositionY, object->position().y - rest.y, false);
        keyProperty(*current, name, BoneTrack::ScaleX,
                    std::fabs(restScale.x) > 0.0001f ? object->scale().x / restScale.x : 1.0f, false);
        keyProperty(*current, name, BoneTrack::ScaleY,
                    std::fabs(restScale.y) > 0.0001f ? object->scale().y / restScale.y : 1.0f, false);
        if (object == s->owner())
            break;
        object = object->parent();
    }
    applyPose();
}

void SkeletonPanel::changed(const char* label)
{
    syncClips();
    syncClip();
    applyPose();
    edited.emit(label);
}

} // namespace k2d::editor
