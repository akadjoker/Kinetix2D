#include "AnimatorPanel.h"

#include "AssetsPanel.h"
#include "FrameStrip.h"
#include "Icons.h"
#include "PropertyFields.h"
#include "core/EditorFileSystem.h"

#include "BasicWidgets.hpp"
#include "ComboBox.hpp"
#include "ScrollWidgets.hpp"
#include "SpinSlider.hpp"
#include "TextInputWidgets.hpp"
#include "Theme.hpp"
#include "ToolBar.hpp"

#include <k2d/Animation2D.h>
#include <k2d/Assets.h>
#include <k2d/GameObject.h>
#include <k2d/Texture.h>

#include <cmath>
#include <cstdio>
#include <utility>

namespace rt = ig::retained;

namespace k2d::editor
{

// The clip at the preview frame, fitted and centred.
class ClipPreview : public rt::Widget
{
public:
    IG_WIDGET_CLASS(ClipPreview, rt::Widget);

    void show(const AnimationClip* clip, int frame)
    {
        mClip = clip;
        mFrame = frame;
        markDirty();
    }

    Vec2f sizeHint() const override { return {150.0f, 110.0f}; }

    void paint(rt::PaintContext& ctx) override
    {
        if (!visible_)
            return;
        const rt::Rect abs = absoluteRect();
        ctx.pushClip(abs);
        ctx.fill.SetColor(rt::Color(22, 24, 28, 255));
        ctx.fillRect(abs.x, abs.y, abs.w, abs.h);
        Texture* texture = nullptr;
        Math::Vec4 r;
        if (mClip && clipFrameImage(*mClip, mFrame, texture, r) && r.z > 0.0f && r.w > 0.0f)
        {
            const float fit = std::fmin((abs.w - 12.0f) / r.z, (abs.h - 12.0f) / r.w);
            const float scale = fit >= 1.0f ? std::floor(fit) : fit;
            const float w = r.z * scale, h = r.w * scale;
            rt::TextureHandle handle;
            handle.value = texture->Id();
            const float tw = static_cast<float>(texture->Width()), th = static_cast<float>(texture->Height());
            ctx.drawImage(handle, rt::Rect{abs.x + (abs.w - w) * 0.5f, abs.y + (abs.h - h) * 0.5f, w, h},
                          rt::Rect{r.x / tw, r.y / th, r.z / tw, r.w / th});
        }
        ctx.popClip();
    }

private:
    const AnimationClip* mClip = nullptr;
    int mFrame = 0;
};

namespace
{

class SideColumn : public rt::BoxLayout
{
public:
    IG_WIDGET_CLASS(SideColumn, rt::BoxLayout);

    SideColumn() : rt::BoxLayout(rt::LayoutDir::Vertical) {}

    Vec2f sizeHint() const override { return {200.0f, rt::BoxLayout::sizeHint().y}; }
};

// The clip's name, at a set width in the bar above the frames.
class ClipName : public rt::TextInput
{
public:
    IG_WIDGET_CLASS(ClipName, rt::TextInput);

    using rt::TextInput::TextInput;

    Vec2f sizeHint() const override { return {140.0f, rt::TextInput::sizeHint().y}; }
};

bool isImage(const ct::String& path)
{
    const ct::String ext = EditorFileSystem::extension(path);
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tga";
}

} // namespace

AnimatorPanel::AnimatorPanel(Assets& assets) : rt::BoxLayout(rt::LayoutDir::Vertical), mAssets(assets)
{
    setPadding(0.0f);
    setSpacing(0.0f);

    mEmpty = createChild<rt::Label>("Select an object to edit its animations");
    mEmpty->setMargins(12.0f);
    mAddAnimation = iconButton(this, "Animation", "Add an Animation2D to this object",
                               [this]() { addComponentRequested.emit("Animation2D"); });

    mContent = createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    mContent->setStretch(1.0f);
    mContent->setPadding(4.0f);
    mContent->setSpacing(6.0f);

    // Clips.
    auto* side = mContent->createChild<SideColumn>();
    side->setPadding(0.0f);
    side->setSpacing(4.0f);
    auto* clipTools = side->createChild<rt::ToolBar>();
    clipTools->setDraggable(false);
    const int addClip = clipTools->addButton(Icons::get("Add"), "New clip");
    const int duplicateClip = clipTools->addButton(Icons::get("Duplicate"), "Duplicate the clip");
    const int removeClip = clipTools->addButton(Icons::get("Remove"), "Delete the clip");
    const int playClip = clipTools->addButton(Icons::get("Play"), "Play this clip when the scene starts");
    clipTools->buttonClicked.connect([this, addClip, duplicateClip, removeClip, playClip](int item)
    {
        Animation2D* anim = animation();
        if (!anim)
            return;
        AnimationClip* current = clip();
        if (item == addClip || item == duplicateClip)
        {
            if (item == duplicateClip && !current)
                return;
            char name[64];
            for (int n = static_cast<int>(anim->clipCount()) + 1;; ++n)
            {
                std::snprintf(name, sizeof name, "%s_%d", item == addClip ? "clip" : current->name.c_str(), n);
                bool taken = false;
                for (std::size_t i = 0; i < anim->clipCount(); ++i)
                    taken = taken || anim->clipAt(i)->name == name;
                if (!taken)
                    break;
            }
            if (item == addClip)
                anim->addClip(name, nullptr, 0, 0, 0, 8.0f, AnimationMode::Loop);
            else
            {
                const AnimationClip copy = *current;
                anim->addClip(name, nullptr, 0, 0, 0, copy.framesPerSecond, copy.mode);
                AnimationClip* made = anim->clipAt(anim->clipCount() - 1);
                const ct::String kept = made->name;
                *made = copy;
                made->name = kept;
            }
            mClip = static_cast<int>(anim->clipCount()) - 1;
            changed(item == addClip ? "New clip" : "Duplicate clip");
        }
        else if (item == removeClip && current)
        {
            const ct::String name = current->name;
            anim->removeClip(name.c_str());
            mClip = mClip > 0 ? mClip - 1 : (anim->clipCount() > 0 ? 0 : -1);
            changed("Delete clip");
        }
        else if (item == playClip && current)
        {
            const ct::String name = current->name;
            anim->play(name.c_str());
            changed("Starting clip");
        }
    });
    mClips = side->createChild<rt::ListBox>();
    mClips->setStretch(1.0f);
    mClips->selectionChanged.connect([this](int index)
    {
        if (!mSyncing)
            selectClip(index);
    });
    // Frames.
    auto* middle = mContent->createChild<rt::BoxLayout>(rt::LayoutDir::Vertical);
    middle->setStretch(1.0f);
    middle->setPadding(0.0f);
    middle->setSpacing(4.0f);
    auto* frameBar = middle->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    frameBar->setPadding(0.0f);
    frameBar->setSpacing(6.0f);
    mFrameTools = frameBar->createChild<rt::ToolBar>();
    mFrameTools->setDraggable(false);
    mPlayItem = mFrameTools->addToggle(Icons::get("Play"), "Play the preview", true);
    const int stepItem = mFrameTools->addButton(Icons::get("Step"), "Next frame");
    mFrameTools->addSeparator();
    const int allItem = mFrameTools->addButton(Icons::get("SpriteBatch"),
                                                "Add every region of the image open in the Sprite Editor");
    const int removeItem = mFrameTools->addButton(Icons::get("Remove"), "Delete the selected frame");
    mFrameTools->toggleChanged.connect([this](int item, bool on)
    {
        if (item == mPlayItem)
            mPlaying = on;
    });
    mFrameTools->buttonClicked.connect([this, stepItem, allItem, removeItem](int item)
    {
        AnimationClip* current = clip();
        if (!current)
            return;
        if (item == stepItem)
        {
            const int count = clipFrameCount(*current);
            mPreviewFrame = count > 0 ? (mPreviewFrame + 1) % count : 0;
            mStrip->setSelected(mPreviewFrame);
            tick(0.0f);
        }
        else if (item == allItem && spriteRegions)
        {
            ct::Vector<SpriteRegionDrag> regions;
            if (!spriteRegions(regions) || regions.empty())
                return;
            int slot = clipFrameCount(*current);
            for (const SpriteRegionDrag& region : regions)
                if (insertRegion(region, slot))
                    ++slot;
            changed("Add frames");
        }
        else if (item == removeItem && mStrip->selected() >= 0)
            mStrip->removeRequested.emit(mStrip->selected());
    });
    mName = frameBar->createChild<ClipName>("");
    mName->setPlaceholder("Clip name");
    mName->submitted.connect([this](const rt::String& name)
    {
        Animation2D* anim = animation();
        AnimationClip* current = clip();
        if (!current || name.empty() || name == current->name)
            return;
        for (std::size_t i = 0; i < anim->clipCount(); ++i)
            if (anim->clipAt(i)->name == name)
            {
                mName->setText(current->name);   // taken
                return;
            }
        current->name = name;
        changed("Rename clip");
    });
    NumberSpec fpsSpec;
    fpsSpec.integer = true;
    fpsSpec.step = 1.0;
    fpsSpec.minimum = 1.0;
    fpsSpec.maximum = 120.0;
    fpsSpec.width = 110.0f;
    mFps = addNumberField(frameBar, "fps", 8.0, fpsSpec, [this](double value, bool final)
    {
        if (AnimationClip* current = clip(); current && !mSyncing)
        {
            current->framesPerSecond = static_cast<float>(value);
            if (final)
                changed("Clip speed");
        }
    });
    mMode = frameBar->createChild<rt::ComboBox>();
    mMode->addItem("One shot");
    mMode->addItem("Loop");
    mMode->addItem("Ping pong");
    mMode->selectionChanged.connect([this](int index)
    {
        if (AnimationClip* current = clip(); current && !mSyncing && index >= 0)
        {
            current->mode = static_cast<AnimationMode>(index);
            changed("Clip mode");
        }
    });

    mInfo = frameBar->createChild<rt::Label>("");
    mInfo->setStretch(1.0f);
    mInfo->setLayoutAlign(rt::Align::Center);

    mStrip = middle->createChild<FrameStrip>();
    mStrip->setStretch(1.0f);
    mStrip->selectionChanged.connect([this](int frame)
    {
        if (frame >= 0)
        {
            mPreviewFrame = frame;
            tick(0.0f);
        }
    });
    mStrip->moved.connect([this](int from, int to)
    {
        moveFrame(from, to);
        mStrip->setSelected(to);
        changed("Move frame");
    });
    mStrip->removeRequested.connect([this](int frame)
    {
        AnimationClip* current = clip();
        if (!current)
            return;
        makeExplicit(*current);
        const ct::String name = current->name;
        animation()->removeFrame(name.c_str(), static_cast<std::size_t>(frame));
        mStrip->setSelected(frame > 0 ? frame - 1 : 0);
        changed("Delete frame");
    });
    mStrip->acceptDrop = [](const rt::DragPayload& payload)
    {
        if (payload.type == kSpriteRegionPayload)
            return true;
        const ct::String* path = std::any_cast<ct::String>(&payload.data);
        return payload.type == kAssetPayload && path && isImage(*path);
    };
    mStrip->dropped = [this](const rt::DragPayload& payload, int slot)
    {
        if (const SpriteRegionDrag* region = std::any_cast<SpriteRegionDrag>(&payload.data))
        {
            if (insertRegion(*region, slot))
                changed("Add frame");
        }
        else if (const ct::String* path = std::any_cast<ct::String>(&payload.data))
        {
            SpriteRegionDrag whole;
            whole.texture = *path;
            whole.rect = Math::Vec4(0.0f);
            if (insertRegion(whole, slot))
                changed("Add frame");
        }
    };

    mPreview = mContent->createChild<ClipPreview>();

    show(nullptr);
}

Animation2D* AnimatorPanel::animation() const
{
    return mObject ? mObject->getComponent<Animation2D>() : nullptr;
}

AnimationClip* AnimatorPanel::clip() const
{
    Animation2D* anim = animation();
    return anim && mClip >= 0 ? anim->clipAt(static_cast<std::size_t>(mClip)) : nullptr;
}

void AnimatorPanel::show(GameObject* object)
{
    if (object != mObject)
        mClip = 0;
    mObject = object;
    Animation2D* anim = animation();
    mEmpty->setVisible(!anim);
    mEmpty->setText(object ? "This object has no Animation2D" : "Select an object to edit its animations");
    mAddAnimation->setVisible(object && !anim);
    mContent->setVisible(anim != nullptr);
    if (anim && (mClip < 0 || mClip >= static_cast<int>(anim->clipCount())))
        mClip = anim->clipCount() > 0 ? 0 : -1;
    syncClips();
    syncClip();
}

void AnimatorPanel::syncClips()
{
    mSyncing = true;
    mClips->clearItems();
    if (Animation2D* anim = animation())
    {
        const char* starting = anim->currentClip();
        for (std::size_t i = 0; i < anim->clipCount(); ++i)
        {
            const ct::String& name = anim->clipAt(i)->name;
            // The clip the scene starts with is marked.
            mClips->addItem(starting && name == starting ? name + "  >" : name);
        }
    }
    mClips->setSelectedIndex(mClip);
    mSyncing = false;
}

void AnimatorPanel::syncClip()
{
    mSyncing = true;
    AnimationClip* current = clip();
    mStrip->setClip(current);
    mName->setText(current ? current->name : ct::String());
    mFps->setValueNoSignal(current ? current->framesPerSecond : 8.0);
    mMode->setSelectedIndex(current ? static_cast<int>(current->mode) : -1);
    mName->setEnabled(current != nullptr);
    mFps->setEnabled(current != nullptr);
    mMode->setEnabled(current != nullptr);
    if (current)
    {
        char info[128];
        const int count = clipFrameCount(*current);
        std::snprintf(info, sizeof info, "%d frames, %.2f s", count,
                      current->framesPerSecond > 0.0f ? count / current->framesPerSecond : 0.0f);
        mInfo->setText(info);
    }
    else
        mInfo->setText(animation() ? "No clip: add one on the left" : "");
    mPreviewFrame = 0;
    mTime = 0.0f;
    mPreview->show(current, 0);
    mSyncing = false;
}

void AnimatorPanel::selectClip(int index)
{
    mClip = index;
    syncClip();
}

void AnimatorPanel::changed(const char* label)
{
    syncClips();
    const int selected = mStrip->selected();
    syncClip();
    mStrip->setSelected(selected);
    edited.emit(label);
}

// A sprite sheet clip becomes a list of the same frames, so frames can be
// added, moved and deleted one by one.
void AnimatorPanel::makeExplicit(AnimationClip& c)
{
    if (!c.frames.empty() || c.frameCount <= 0)
        return;
    const char* path = mAssets.FindTextureName(c.texture);
    ct::Vector<AnimationFrame> frames;
    for (int i = 0; i < c.frameCount; ++i)
    {
        AnimationFrame frame;
        if (!clipFrameImage(c, i, frame.texture, frame.rect))
            continue;
        frame.texturePath = path ? path : "";
        frames.push_back(frame);
    }
    c.frames = frames;
    c.texture = nullptr;
    c.frameWidth = c.frameHeight = c.frameCount = 0;
}

bool AnimatorPanel::insertRegion(const SpriteRegionDrag& region, int slot)
{
    AnimationClip* current = clip();
    Texture* texture = mAssets.GetTexture(region.texture.c_str());
    if (!current || !texture)
        return false;
    makeExplicit(*current);
    const ct::String name = current->name;
    if (!animation()->addFrame(name.c_str(), texture, region.rect, region.texture.c_str()))
        return false;
    moveFrame(static_cast<int>(clip()->frames.size()) - 1, slot);
    return true;
}

void AnimatorPanel::moveFrame(int from, int to)
{
    AnimationClip* current = clip();
    if (!current)
        return;
    makeExplicit(*current);
    ct::Vector<AnimationFrame>& frames = current->frames;
    const int count = static_cast<int>(frames.size());
    if (from < 0 || from >= count)
        return;
    to = to < 0 ? 0 : (to >= count ? count - 1 : to);
    for (; from < to; ++from)
        std::swap(frames[from], frames[from + 1]);
    for (; from > to; --from)
        std::swap(frames[from], frames[from - 1]);
}

void AnimatorPanel::tick(float deltaTime)
{
    AnimationClip* current = clip();
    if (!current || !mContent->isVisible())
        return;
    const int count = clipFrameCount(*current);
    if (count <= 0)
    {
        mPreview->show(current, 0);
        mStrip->setPlayhead(-1);
        return;
    }
    if (mPlaying && current->framesPerSecond > 0.0f)
    {
        mTime += deltaTime;
        const float step = 1.0f / current->framesPerSecond;
        while (mTime >= step)
        {
            mTime -= step;
            mPreviewFrame = (mPreviewFrame + 1) % count;
        }
    }
    mPreviewFrame = mPreviewFrame < count ? mPreviewFrame : 0;
    mPreview->show(current, mPreviewFrame);
    mStrip->setPlayhead(mPreviewFrame);
}

} // namespace k2d::editor
