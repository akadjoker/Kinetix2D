#include "k2d/UiControls.h"

#include "k2d/Camera2D.h"
#include "k2d/CameraComponent.h"
#include "k2d/GameObject.h"
#include "k2d/Input.h"
#include "k2d/RenderQueue.h"
#include "k2d/Scene.h"
#include "k2d/Texture.h"

#include <limits>

namespace k2d
{
    namespace
    {
        Input *gUiInput = nullptr;
        Texture *gUiThemeTexture = nullptr;
        UiViewport gUiViewport;
        constexpr int kUiZBase = (std::numeric_limits<int>::max)() - 100000;

        // Regions in the embedded assets/menu.png default UI atlas.
        constexpr float kPanelX = 2.0f, kPanelY = 2.0f, kPanelW = 72.0f, kPanelH = 72.0f;
        constexpr float kButtonX = 79.0f, kButtonY = 2.0f, kButtonW = 44.0f, kButtonH = 44.0f;
        constexpr float kCheckOffX = 79.0f, kCheckOffY = 2.0f;
        constexpr float kCheckOnX = 79.0f, kCheckOnY = 47.0f, kCheckW = 44.0f, kCheckH = 44.0f;
        constexpr float kSliderTrackX = 3.0f, kSliderTrackY = 82.0f, kSliderTrackW = 71.0f, kSliderTrackH = 5.0f;
        constexpr float kSliderKnobX = 6.0f, kSliderKnobY = 150.0f, kSliderKnobW = 9.0f, kSliderKnobH = 23.0f;

        float MeasureUiText(const ct::String &text, float glyphSize)
        {
            float widest = 0.0f;
            float width = 0.0f;
            for (const char *character = text.c_str(); *character; ++character)
            {
                if (*character == '\n')
                {
                    widest = widest > width ? widest : width;
                    width = 0.0f;
                }
                else
                    width += glyphSize;
            }
            return widest > width ? widest : width;
        }

    }

    void SetUiInput(Input *input) { gUiInput = input; }
    Input *GetUiInputInternal() { return gUiInput; }
    void SetUiThemeTexture(Texture *texture) { gUiThemeTexture = texture; }
    void SetUiViewport(float x, float y, float width, float height)
    {
        gUiViewport.x = x;
        gUiViewport.y = y;
        gUiViewport.width = width > 0.0f ? width : 0.0f;
        gUiViewport.height = height > 0.0f ? height : 0.0f;
        gUiViewport.valid = gUiViewport.width > 0.0f && gUiViewport.height > 0.0f;
    }
    const UiViewport &GetUiViewport() { return gUiViewport; }

    UiControl::UiControl(ComponentType type, bool interactive)
        : Component(type, ComponentEventRender), mAnchors(0.0f), mOffsets(0.0f, 0.0f, 120.0f, 32.0f),
          mRect(0.0f), mInteractive(interactive), mHovered(false), mPressed(false), mClicked(false)
    {
    }

    void UiControl::setRect(float x, float y, float width, float height)
    {
        mAnchors = Math::Vec4(0.0f);
        mOffsets = Math::Vec4(x, y, x + width, y + height);
    }

    Math::Vec4 UiControl::rect() const { return mRect; }

    void UiControl::updateLayout()
    {
        float baseX = 0.0f, baseY = 0.0f;
        float baseW = gUiViewport.width, baseH = gUiViewport.height;
        for (GameObject *parent = owner() ? owner()->parent() : nullptr; parent; parent = parent->parent())
        {
            Component *component = parent->uiComponent();
            if (!component)
                continue;
            const Math::Vec4 parentRect = component->uiControl()->rect();
            baseX = parentRect.x;
            baseY = parentRect.y;
            baseW = parentRect.z;
            baseH = parentRect.w;
            break;
        }
        const float left = baseX + baseW * mAnchors.x + mOffsets.x;
        const float top = baseY + baseH * mAnchors.y + mOffsets.y;
        mRect = Math::Vec4(left, top,
                           baseW * (mAnchors.z - mAnchors.x) + mOffsets.z - mOffsets.x,
                           baseH * (mAnchors.w - mAnchors.y) + mOffsets.w - mOffsets.y);
    }

    bool UiControl::contains(float x, float y) const
    {
        return x >= mRect.x && y >= mRect.y && x < mRect.x + mRect.z && y < mRect.y + mRect.w;
    }

    void UiControl::resetInput()
    {
        mHovered = false;
        mClicked = false;
    }

    void UiControl::handleInput(float x, float y, bool down, bool pressed, bool released)
    {
        mHovered = contains(x, y);
        if (!mHovered)
        {
            if (released)
                mPressed = false;
            return;
        }
        if (pressed)
            mPressed = true;
        if (released)
        {
            mClicked = mPressed;
            mPressed = false;
        }
        onUiInput(down, pressed, released);
    }

    RenderItem &UiControl::beginUiItem(RenderQueue &queue) const
    {
        RenderItem &item = queue.AddItem(kUiZBase + owner()->zIndex());
        const UiViewport &viewport = GetUiViewport();
        if (Scene *scene = owner()->scene())
            if (CameraComponent *camera = scene->activeCamera())
            {
                item.xform = camera->camera().CameraXform(viewport.width, viewport.height);
                return item;
            }

        // The runner's fallback projection is also camera based, so UI must
        // cancel its view transform even when the scene has no CameraComponent.
        Camera2D defaultCamera;
        item.xform = defaultCamera.CameraXform(viewport.width, viewport.height);
        return item;
    }

    void UiControl::addSolidRect(RenderItem &item, float x, float y, float width, float height, const Color &color) const
    {
        RenderCommand command = RenderCommand::MakeRect(0, x, y, width, height);
        // UI rect coordinates are their top-left corner. RenderCommand's
        // generic default pivot is centered, which displaces every box by
        // half of its own size while text (whose pivot is top-left) remains
        // correctly placed.
        command.pivotX = 0.0f;
        command.pivotY = 0.0f;
        command.color = color;
        item.commands.push_back(command);
    }

    void UiControl::addThemeRect(RenderItem &item, float x, float y, float width, float height,
                                 float srcX, float srcY, float srcWidth, float srcHeight, const Color &color) const
    {
        if (!gUiThemeTexture)
            return;
        RenderCommand command = RenderCommand::MakeRect(gUiThemeTexture->Id(), x, y, width, height);
        command.pivotX = 0.0f;
        command.pivotY = 0.0f;
        command.srcX = srcX;
        command.srcY = srcY;
        command.srcW = srcWidth;
        command.srcH = srcHeight;
        command.texWidth = gUiThemeTexture->Width();
        command.texHeight = gUiThemeTexture->Height();
        command.color = color;
        item.commands.push_back(command);
    }

    void UiControl::addText(RenderItem &item, float x, float y, float size, const ct::String &text, const Color &color) const
    {
        if (text.empty())
            return;
        RenderCommand command;
        command.type = RenderCommand::kText;
        command.x = x;
        command.y = y;
        command.width = size;
        command.text = text;
        command.color = color;
        item.commands.push_back(command);
    }

    void UiControl::onUiInput(bool, bool, bool) {}

    UiPanel::UiPanel() : UiControl(Type, false), mColor(0.10f, 0.12f, 0.16f, 0.94f) {}
    void UiPanel::onRender(RenderQueue &queue)
    {
        const Math::Vec4 r = rect();
        RenderItem &item = beginUiItem(queue);
        if (gUiThemeTexture)
            addThemeRect(item, r.x, r.y, r.z, r.w, kPanelX, kPanelY, kPanelW, kPanelH, mColor);
        else
            addSolidRect(item, r.x, r.y, r.z, r.w, mColor);
    }

    UiLabel::UiLabel() : UiControl(Type, false), mText("Label"), mFontSize(16.0f), mColor(Color::White()) {}
    void UiLabel::onRender(RenderQueue &queue)
    {
        const Math::Vec4 r = rect();
        RenderItem &item = beginUiItem(queue);
        addText(item, r.x, r.y + (r.w - mFontSize) * 0.5f, mFontSize, mText, mColor);
    }

    UiButton::UiButton() : UiControl(Type, true), mText("Button"), mActivated(false) {}
    bool UiButton::consumeClick() { const bool value = mActivated; mActivated = false; return value; }
    void UiButton::onUiInput(bool, bool, bool released) { if (released && clicked()) mActivated = true; }
    void UiButton::onRender(RenderQueue &queue)
    {
        const Math::Vec4 r = rect();
        const Color base = pressed() ? Color(0.18f, 0.42f, 0.72f) : hovered() ? Color(0.25f, 0.55f, 0.90f) : Color(0.20f, 0.46f, 0.78f);
        RenderItem &item = beginUiItem(queue);
        if (gUiThemeTexture)
            addThemeRect(item, r.x, r.y, r.z, r.w, kButtonX, kButtonY, kButtonW, kButtonH, base);
        else
            addSolidRect(item, r.x, r.y, r.z, r.w, base);
        constexpr float textSize = 16.0f;
        const float textWidth = MeasureUiText(mText, textSize);
        const float textX = r.z > textWidth ? r.x + (r.z - textWidth) * 0.5f : r.x + 8.0f;
        addText(item, textX, r.y + (r.w - textSize) * 0.5f, textSize, mText, Color::White());
    }

    UiCheckBox::UiCheckBox() : UiControl(Type, true), mText("CheckBox"), mChecked(false), mChanged(false) {}
    bool UiCheckBox::consumeChanged() { const bool value = mChanged; mChanged = false; return value; }
    void UiCheckBox::onUiInput(bool, bool, bool released)
    {
        if (released && clicked()) { mChecked = !mChecked; mChanged = true; }
    }
    void UiCheckBox::onRender(RenderQueue &queue)
    {
        const Math::Vec4 r = rect();
        const float side = r.w < 22.0f ? r.w : 22.0f;
        const float top = r.y + (r.w - side) * 0.5f;
        RenderItem &item = beginUiItem(queue);
        if (gUiThemeTexture)
        {
            const Color tint = mChecked ? Color(0.35f, 0.78f, 0.50f) : Color(0.72f, 0.80f, 0.93f);
            addThemeRect(item, r.x, top, side, side,
                         mChecked ? kCheckOnX : kCheckOffX, mChecked ? kCheckOnY : kCheckOffY,
                         kCheckW, kCheckH, tint);
        }
        else
        {
            addSolidRect(item, r.x, top, side, side, Color(0.18f, 0.22f, 0.30f));
            if (mChecked)
                addSolidRect(item, r.x + 5.0f, top + 5.0f, side - 10.0f, side - 10.0f, Color(0.26f, 0.75f, 0.45f));
        }
        addText(item, r.x + side + 8.0f, r.y + (r.w - 16.0f) * 0.5f, 16.0f, mText, Color::White());
    }

    UiSlider::UiSlider() : UiControl(Type, true), mMinimum(0.0f), mMaximum(1.0f), mValue(0.5f), mChanged(false) {}
    void UiSlider::setRange(float minimum, float maximum)
    {
        mMinimum = minimum;
        mMaximum = maximum > minimum ? maximum : minimum + 1.0f;
        setValue(mValue);
    }
    void UiSlider::setValue(float value)
    {
        mValue = value < mMinimum ? mMinimum : (value > mMaximum ? mMaximum : value);
    }
    bool UiSlider::consumeChanged() { const bool value = mChanged; mChanged = false; return value; }
    void UiSlider::onUiInput(bool down, bool pressed, bool)
    {
        if (!down && !pressed) return;
        const Math::Vec4 r = rect();
        const float mouseX = gUiInput ? gUiInput->MouseX() - gUiViewport.x : r.x;
        const float t = r.z > 1.0f ? (mouseX - r.x) / r.z : 0.0f;
        const float next = mMinimum + (t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t)) * (mMaximum - mMinimum);
        if (next != mValue) { mValue = next; mChanged = true; }
    }
    void UiSlider::onRender(RenderQueue &queue)
    {
        const Math::Vec4 r = rect();
        const float t = (mValue - mMinimum) / (mMaximum - mMinimum);
        RenderItem &item = beginUiItem(queue);
        if (gUiThemeTexture)
        {
            const float trackY = r.y + r.w * 0.5f - kSliderTrackH * 0.5f;
            const float knobX = r.x + t * (r.z - kSliderKnobW);
            const float knobY = r.y + (r.w - kSliderKnobH) * 0.5f;
            addThemeRect(item, r.x, trackY, r.z, kSliderTrackH,
                         kSliderTrackX, kSliderTrackY, kSliderTrackW, kSliderTrackH, Color(0.20f, 0.26f, 0.36f));
            addThemeRect(item, r.x, trackY, t * r.z, kSliderTrackH,
                         kSliderTrackX, kSliderTrackY, kSliderTrackW, kSliderTrackH, Color(0.24f, 0.58f, 0.93f));
            addThemeRect(item, knobX, knobY, kSliderKnobW, kSliderKnobH,
                         kSliderKnobX, kSliderKnobY, kSliderKnobW, kSliderKnobH,
                         hovered() ? Color(0.85f, 0.90f, 1.0f) : Color(0.70f, 0.78f, 0.92f));
        }
        else
        {
            const float knobX = r.x + t * (r.z - 14.0f);
            addSolidRect(item, r.x, r.y + r.w * 0.5f - 3.0f, r.z, 6.0f, Color(0.16f, 0.20f, 0.27f));
            addSolidRect(item, r.x, r.y + r.w * 0.5f - 3.0f, t * r.z, 6.0f, Color(0.24f, 0.58f, 0.93f));
            addSolidRect(item, knobX, r.y + r.w * 0.5f - 9.0f, 14.0f, 18.0f, hovered() ? Color(0.85f, 0.90f, 1.0f) : Color(0.70f, 0.78f, 0.92f));
        }
    }
}
