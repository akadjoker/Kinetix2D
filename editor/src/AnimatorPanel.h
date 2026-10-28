#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"
#include "SpriteEditorPanel.h"

#include <ct/function.hpp>
#include <ct/vector.hpp>

namespace k2d
{
class Animation2D;
class Assets;
class GameObject;
struct AnimationClip;
}

namespace ig::retained
{
class ComboBox;
class Label;
class ListBox;
class SpinSlider;
class TextInput;
class ToolBar;
}

namespace k2d::editor
{

class FrameStrip;
class ClipPreview;

// The clips of the selected object's Animation2D: made, named and timed in the
// column on the left, their frames in a strip that takes sprite regions and
// images dropped on it, and a preview playing the clip in the editor.
class AnimatorPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(AnimatorPanel, ig::retained::BoxLayout);

    explicit AnimatorPanel(Assets& assets);

    void show(GameObject* object);
    // Advances the preview.
    void tick(float deltaTime);

    // Every region of the image open in the Sprite Editor, for "add them all".
    ct::Function<bool(ct::Vector<SpriteRegionDrag>&)> spriteRegions;

    ig::retained::Signal<const char*> edited;
    ig::retained::Signal<const char*> addComponentRequested;

private:
    Animation2D* animation() const;
    AnimationClip* clip() const;
    void syncClips();
    void syncClip();
    void selectClip(int index);
    void makeExplicit(AnimationClip& clip);
    bool insertRegion(const SpriteRegionDrag& region, int slot);
    void moveFrame(int from, int to);
    void changed(const char* label);

    Assets& mAssets;
    GameObject* mObject = nullptr;
    int mClip = -1;
    bool mSyncing = false;
    bool mPlaying = true;
    float mTime = 0.0f;
    int mPreviewFrame = 0;

    ig::retained::Label* mEmpty = nullptr;
    ig::retained::ToolBar* mAddAnimation = nullptr;
    ig::retained::BoxLayout* mContent = nullptr;
    ig::retained::ListBox* mClips = nullptr;
    ig::retained::TextInput* mName = nullptr;
    ig::retained::SpinSlider* mFps = nullptr;
    ig::retained::ComboBox* mMode = nullptr;
    ig::retained::ToolBar* mFrameTools = nullptr;
    int mPlayItem = -1;
    ig::retained::Label* mInfo = nullptr;
    FrameStrip* mStrip = nullptr;
    ClipPreview* mPreview = nullptr;
};

} // namespace k2d::editor
