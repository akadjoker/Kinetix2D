#pragma once

#include "LayoutWidgets.hpp"
#include "ScrollWidgets.hpp"
#include "Signal.hpp"

#include <ct/function.hpp>
#include <ct/string.hpp>

namespace k2d
{
class GameObject;
class ParticleComponent;
}

namespace ig::retained
{
class Label;
class ToolBar;
}

namespace k2d::editor
{

// The selected object's particle emitter: a preset to start from, then every
// setting of the emitter and of the particles it makes, laid out by what they
// shape (emission, motion, look). Restart and Burst show the result at once,
// and the live preview keeps it moving while editing.
class ParticlesPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(ParticlesPanel, ig::retained::BoxLayout);

    ParticlesPanel();

    void show(GameObject* object);
    // Keeps the count of live particles current.
    void tick();

    // An edit; final is false while a value is being dragged.
    ig::retained::Signal<const char*, bool> edited;
    ig::retained::Signal<const char*> addComponentRequested;

private:
    ParticleComponent* emitter() const;
    void rebuild();
    void applyPreset(int preset);
    ig::retained::BoxLayout* row(ig::retained::Widget* parent, const char* name);
    void number(ig::retained::Widget* parent, const char* label, float value, double step, double minimum,
                ct::Function<void(float)> set);

    GameObject* mObject = nullptr;
    ig::retained::Label* mEmpty = nullptr;
    ig::retained::ToolBar* mAddEmitter = nullptr;
    ig::retained::BoxLayout* mContent = nullptr;
    ig::retained::Label* mCount = nullptr;
    ig::retained::ScrollView* mScroll = nullptr;
    ig::retained::BoxLayout* mFields = nullptr;
};

} // namespace k2d::editor
