#pragma once

#include "k2d/Color.h"
#include "k2d/Component.h"

#include <ct/string.hpp>
#include <mathc.h>

namespace k2d
{
    class Input;
    class RenderQueue;
    struct RenderItem;
    class Texture;

    struct UiViewport
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 1280.0f;
        float height = 720.0f;
        bool valid = false;
    };

    // The host (runner or Game panel) supplies window input and its drawable
    // rectangle once per frame. UI coordinates remain local to that rectangle.
    void SetUiInput(Input *input);
    void SetUiViewport(float x, float y, float width, float height);
    void SetUiThemeTexture(Texture *texture);
    const UiViewport &GetUiViewport();

    class UiCanvas : public Component
    {
    public:
        static const ComponentType Type = ComponentType::UiCanvas;
        UiCanvas() : Component(Type) {}
    };

    class UiControl : public Component
    {
    public:
        UiControl(ComponentType type, bool interactive);

        UiControl *uiControl() override { return this; }
        const Math::Vec4 &anchors() const { return mAnchors; }
        const Math::Vec4 &offsets() const { return mOffsets; }
        void setAnchors(const Math::Vec4 &value) { mAnchors = value; }
        void setOffsets(const Math::Vec4 &value) { mOffsets = value; }
        void setRect(float x, float y, float width, float height);
        Math::Vec4 rect() const;
        bool hovered() const { return mHovered; }
        bool pressed() const { return mPressed; }
        bool clicked() const { return mClicked; }
        bool interactive() const { return mInteractive; }
        void setInteractive(bool value) { mInteractive = value; }

        void updateLayout();
        bool contains(float x, float y) const;
        void resetInput();
        void handleInput(float x, float y, bool down, bool pressed, bool released);

    protected:
        RenderItem &beginUiItem(RenderQueue &queue) const;
        void addSolidRect(RenderItem &item, float x, float y, float width, float height, const Color &color) const;
        void addThemeRect(RenderItem &item, float x, float y, float width, float height,
                          float srcX, float srcY, float srcWidth, float srcHeight, const Color &color) const;
        void addText(RenderItem &item, float x, float y, float size, const ct::String &text, const Color &color) const;
        virtual void onUiInput(bool down, bool pressed, bool released);

    private:
        Math::Vec4 mAnchors;
        Math::Vec4 mOffsets;
        Math::Vec4 mRect;
        bool mInteractive;
        bool mHovered;
        bool mPressed;
        bool mClicked;
    };

    class UiPanel : public UiControl
    {
    public:
        static const ComponentType Type = ComponentType::UiPanel;
        UiPanel();
        const Color &color() const { return mColor; }
        void setColor(const Color &value) { mColor = value; }
    protected:
        void onRender(RenderQueue &queue) override;
    private:
        Color mColor;
    };

    class UiLabel : public UiControl
    {
    public:
        static const ComponentType Type = ComponentType::UiLabel;
        UiLabel();
        const ct::String &text() const { return mText; }
        void setText(const char *value) { mText = value ? value : ""; }
        float fontSize() const { return mFontSize; }
        void setFontSize(float value) { mFontSize = value > 1.0f ? value : 1.0f; }
        const Color &color() const { return mColor; }
        void setColor(const Color &value) { mColor = value; }
    protected:
        void onRender(RenderQueue &queue) override;
    private:
        ct::String mText;
        float mFontSize;
        Color mColor;
    };

    class UiButton : public UiControl
    {
    public:
        static const ComponentType Type = ComponentType::UiButton;
        UiButton();
        const ct::String &text() const { return mText; }
        void setText(const char *value) { mText = value ? value : ""; }
        bool consumeClick();
    protected:
        void onRender(RenderQueue &queue) override;
        void onUiInput(bool down, bool pressed, bool released) override;
    private:
        ct::String mText;
        bool mActivated;
    };

    class UiCheckBox : public UiControl
    {
    public:
        static const ComponentType Type = ComponentType::UiCheckBox;
        UiCheckBox();
        const ct::String &text() const { return mText; }
        void setText(const char *value) { mText = value ? value : ""; }
        bool checked() const { return mChecked; }
        void setChecked(bool value) { mChecked = value; }
        bool consumeChanged();
    protected:
        void onRender(RenderQueue &queue) override;
        void onUiInput(bool down, bool pressed, bool released) override;
    private:
        ct::String mText;
        bool mChecked;
        bool mChanged;
    };

    class UiSlider : public UiControl
    {
    public:
        static const ComponentType Type = ComponentType::UiSlider;
        UiSlider();
        float value() const { return mValue; }
        void setValue(float value);
        float minimum() const { return mMinimum; }
        float maximum() const { return mMaximum; }
        void setRange(float minimum, float maximum);
        bool consumeChanged();
    protected:
        void onRender(RenderQueue &queue) override;
        void onUiInput(bool down, bool pressed, bool released) override;
    private:
        float mMinimum;
        float mMaximum;
        float mValue;
        bool mChanged;
    };
}
