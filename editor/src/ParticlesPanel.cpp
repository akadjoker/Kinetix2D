#include "ParticlesPanel.h"

#include "Icons.h"

#include "BasicWidgets.hpp"
#include "ComboBox.hpp"
#include "SpinSlider.hpp"
#include "ToolBar.hpp"
#include "WidgetApp.hpp"

#include <k2d/GameObject.h>
#include <k2d/ParticleComponent.h>

#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{

constexpr float kLabelWidth = 96.0f;

class FieldLabel : public rt::Label
{
public:
    IG_WIDGET_CLASS(FieldLabel, rt::Label);

    explicit FieldLabel(const rt::String& text) : rt::Label(text) {}

    Vec2f sizeHint() const override { return {kLabelWidth, rt::Label::sizeHint().y}; }
};

const char* const kPresets[] = {"Preset...", "Fire", "Smoke", "Sparks", "Magic", "Rain", "Explosion"};

ParticlePrefab presetPrefab(int preset, ParticleSystem& system)
{
    ParticlePrefab p;
    system.SetEmitterShape(ParticleEmitterShape::Circle);
    switch (preset)
    {
    case 1: // fire: rises, shrinks, yellow to red and away
        p.direction = Math::Vec2(0.0f, -1.0f);
        p.spreadDegrees = 25.0f;
        p.speedMin = 40.0f;
        p.speedMax = 90.0f;
        p.lifeMin = 0.5f;
        p.lifeMax = 1.0f;
        p.sizeMin = 10.0f;
        p.sizeMax = 18.0f;
        p.endSize = 2.0f;
        p.fadeOut = 0.4f;
        p.colorStart = Color(1.0f, 0.85f, 0.3f, 1.0f);
        p.colorEnd = Color(0.9f, 0.15f, 0.05f, 0.0f);
        system.SetMode(ParticleMode::Loop);
        system.SetEmissionRate(60.0f);
        system.SetEmitterSize(Math::Vec2(8.0f));
        system.SetGravity(Math::Vec2(0.0f, -20.0f));
        break;
    case 2: // smoke: slow, grows, grey and fading
        p.direction = Math::Vec2(0.0f, -1.0f);
        p.spreadDegrees = 20.0f;
        p.speedMin = 15.0f;
        p.speedMax = 35.0f;
        p.lifeMin = 2.0f;
        p.lifeMax = 3.5f;
        p.sizeMin = 12.0f;
        p.sizeMax = 20.0f;
        p.endSize = 48.0f;
        p.angularVelocityMin = -30.0f;
        p.angularVelocityMax = 30.0f;
        p.fadeIn = 0.3f;
        p.fadeOut = 1.2f;
        p.colorStart = Color(0.55f, 0.55f, 0.58f, 0.7f);
        p.colorEnd = Color(0.3f, 0.3f, 0.32f, 0.0f);
        system.SetMode(ParticleMode::Loop);
        system.SetEmissionRate(12.0f);
        system.SetEmitterSize(Math::Vec2(6.0f));
        system.SetGravity(Math::Vec2(0.0f, -5.0f));
        break;
    case 3: // sparks: fast, every way, falling
        p.spreadDegrees = 180.0f;
        p.speedMin = 120.0f;
        p.speedMax = 260.0f;
        p.lifeMin = 0.3f;
        p.lifeMax = 0.7f;
        p.sizeMin = 2.0f;
        p.sizeMax = 4.0f;
        p.endSize = 1.0f;
        p.drag = 1.5f;
        p.faceDirection = true;
        p.colorStart = Color(1.0f, 0.95f, 0.6f, 1.0f);
        p.colorEnd = Color(1.0f, 0.45f, 0.1f, 0.0f);
        system.SetMode(ParticleMode::Loop);
        system.SetEmissionRate(80.0f);
        system.SetEmitterSize(Math::Vec2(2.0f));
        system.SetGravity(Math::Vec2(0.0f, 400.0f));
        break;
    case 4: // magic: drifting, spinning, violet to cyan
        p.spreadDegrees = 180.0f;
        p.speedMin = 10.0f;
        p.speedMax = 40.0f;
        p.lifeMin = 1.0f;
        p.lifeMax = 2.0f;
        p.sizeMin = 4.0f;
        p.sizeMax = 9.0f;
        p.endSize = 0.0f;
        p.angularVelocityMin = -180.0f;
        p.angularVelocityMax = 180.0f;
        p.fadeIn = 0.2f;
        p.fadeOut = 0.6f;
        p.colorStart = Color(0.75f, 0.4f, 1.0f, 1.0f);
        p.colorEnd = Color(0.3f, 0.9f, 1.0f, 0.0f);
        system.SetMode(ParticleMode::Loop);
        system.SetEmissionRate(30.0f);
        system.SetEmitterSize(Math::Vec2(24.0f));
        system.SetGravity(Math::Vec2(0.0f, -10.0f));
        break;
    case 5: // rain: a wide band falling straight
        p.direction = Math::Vec2(0.1f, 1.0f);
        p.spreadDegrees = 2.0f;
        p.speedMin = 500.0f;
        p.speedMax = 650.0f;
        p.lifeMin = 0.8f;
        p.lifeMax = 1.0f;
        p.sizeMin = 2.0f;
        p.sizeMax = 3.0f;
        p.endSize = 2.0f;
        p.faceDirection = true;
        p.colorStart = Color(0.6f, 0.7f, 0.9f, 0.7f);
        p.colorEnd = Color(0.6f, 0.7f, 0.9f, 0.4f);
        system.SetMode(ParticleMode::Loop);
        system.SetEmissionRate(200.0f);
        system.SetEmitterShape(ParticleEmitterShape::Rectangle);
        system.SetEmitterSize(Math::Vec2(600.0f, 4.0f));
        system.SetGravity(Math::Vec2(0.0f, 0.0f));
        break;
    case 6: // explosion: one burst out and slowing
        p.spreadDegrees = 180.0f;
        p.speedMin = 150.0f;
        p.speedMax = 380.0f;
        p.lifeMin = 0.4f;
        p.lifeMax = 0.9f;
        p.sizeMin = 8.0f;
        p.sizeMax = 16.0f;
        p.endSize = 2.0f;
        p.drag = 3.0f;
        p.fadeOut = 0.3f;
        p.colorStart = Color(1.0f, 0.9f, 0.5f, 1.0f);
        p.colorEnd = Color(0.6f, 0.1f, 0.05f, 0.0f);
        system.SetMode(ParticleMode::OneShot);
        system.SetOneShotCount(80);
        system.SetEmitterSize(Math::Vec2(4.0f));
        system.SetGravity(Math::Vec2(0.0f, 60.0f));
        break;
    default:
        return system.GetPrefab();
    }
    return p;
}

} // namespace

ParticlesPanel::ParticlesPanel() : rt::BoxLayout(rt::LayoutDir::Vertical)
{
    setPadding(0.0f);
    setSpacing(0.0f);

    mEmpty = createChild<rt::Label>("Select an object to edit its particles");
    mEmpty->setMargins(12.0f);
    mAddEmitter = iconButton(this, "Particle", "Add a particle emitter to this object",
                             [this]() { addComponentRequested.emit("Particle"); });

    mContent = createChild<rt::BoxLayout>(rt::LayoutDir::Vertical);
    mContent->setStretch(1.0f);
    mContent->setPadding(4.0f);
    mContent->setSpacing(4.0f);
    auto* bar = mContent->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    bar->setPadding(0.0f);
    bar->setSpacing(6.0f);
    auto* tools = bar->createChild<rt::ToolBar>();
    tools->setDraggable(false);
    const int restartItem = tools->addButton(Icons::get("Refresh"), "Restart the emitter");
    const int burstItem = tools->addButton(Icons::get("Play"), "Burst: the one-shot count at once");
    tools->buttonClicked.connect([this, restartItem, burstItem](int item)
    {
        ParticleComponent* particles = emitter();
        if (!particles)
            return;
        ParticleSystem& system = particles->system();
        if (item == restartItem)
            system.Reset();
        else if (item == burstItem)
            for (std::size_t i = 0; i < system.OneShotCount(); ++i)
                system.Emit(mObject->globalPosition(), system.GetPrefab());
    });
    auto* presets = bar->createChild<rt::ComboBox>();
    for (const char* name : kPresets)
        presets->addItem(name);
    presets->setSelectedIndex(0);
    presets->selectionChanged.connect([this, presets](int index)
    {
        if (index <= 0)
            return;
        applyPreset(index);
        presets->setSelectedIndex(0);
    });
    mCount = bar->createChild<rt::Label>("");
    mCount->setStretch(1.0f);
    mCount->setLayoutAlign(rt::Align::Center);

    mScroll = mContent->createChild<rt::ScrollView>();
    mScroll->setStretch(1.0f);
    mFields = mScroll->setContent<rt::BoxLayout>(rt::LayoutDir::Vertical);
    mFields->setPadding(2.0f);
    mFields->setSpacing(3.0f);

    show(nullptr);
}

ParticleComponent* ParticlesPanel::emitter() const
{
    return mObject ? mObject->getComponent<ParticleComponent>() : nullptr;
}

void ParticlesPanel::show(GameObject* object)
{
    mObject = object;
    ParticleComponent* particles = emitter();
    mEmpty->setVisible(!particles);
    mEmpty->setText(object ? "This object has no particle emitter" : "Select an object to edit its particles");
    mAddEmitter->setVisible(object && !particles);
    mContent->setVisible(particles != nullptr);
    rebuild();
}

void ParticlesPanel::tick()
{
    ParticleComponent* particles = emitter();
    if (!particles || !mContent->isVisible())
        return;
    char text[64];
    std::snprintf(text, sizeof text, "%d particles", static_cast<int>(particles->system().ActiveCount()));
    mCount->setText(text);
}

void ParticlesPanel::applyPreset(int preset)
{
    ParticleComponent* particles = emitter();
    if (!particles)
        return;
    ParticleSystem& system = particles->system();
    system.SetPrefab(presetPrefab(preset, system));
    system.Reset();
    rebuild();
    edited.emit("Particle preset", true);
}

rt::BoxLayout* ParticlesPanel::row(rt::Widget* parent, const char* name)
{
    auto* line = parent->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    line->setPadding(0.0f);
    line->setSpacing(4.0f);
    line->createChild<FieldLabel>(name);
    return line;
}

void ParticlesPanel::number(rt::Widget* parent, const char* label, float value, double step, double minimum,
                            ct::Function<void(float)> set)
{
    auto* field = parent->createChild<rt::SpinSlider>();
    field->setLabel(label);
    field->setStep(step);
    field->setMin(minimum);
    field->setMax(minimum >= 0.0 ? 100000.0 : 100000.0);
    field->setAllowGreater(true);
    field->setAllowLesser(minimum < 0.0);
    field->setControlState(rt::SpinSlider::ControlState::Hide);
    field->setValueNoSignal(value);
    field->setStretch(1.0f);
    field->valueChanged.connect([this, field, set](double v) {
        set(static_cast<float>(v));
        edited.emit("Edit particles", !field->isGrabbing());
    });
    field->ungrabbed.connect([this]() { edited.emit("Edit particles", true); });
}

void ParticlesPanel::rebuild()
{
    while (!mFields->children().empty())
        mFields->removeChild(mFields->children().back());
    ParticleComponent* particles = emitter();
    if (!particles)
        return;
    ParticleSystem* system = &particles->system();
    // Every prefab field goes through a copy: SetPrefab is the only way in.
    auto edit = [system](ct::Function<void(ParticlePrefab&)> change) {
        ParticlePrefab prefab = system->GetPrefab();
        change(prefab);
        system->SetPrefab(prefab);
    };
    const ParticlePrefab& p = system->GetPrefab();

    auto* emission = mFields->createChild<rt::Collapsible>("Emission");
    {
        auto* modes = row(emission, "mode")->createChild<rt::ComboBox>();
        modes->addItem("One shot");
        modes->addItem("Persistent");
        modes->addItem("Loop");
        modes->setSelectedIndex(static_cast<int>(system->GetMode()));
        modes->setStretch(1.0f);
        modes->selectionChanged.connect([this, system](int i) {
            system->SetMode(static_cast<ParticleMode>(i));
            system->Reset();
            edited.emit("Particle mode", true);
        });
        number(row(emission, "rate"), "per s", system->EmissionRate(), 1.0, 0.0,
               [system](float v) { system->SetEmissionRate(v); });
        number(row(emission, "one shot"), "count", static_cast<float>(system->OneShotCount()), 1.0, 0.0,
               [system](float v) { system->SetOneShotCount(static_cast<std::size_t>(v)); });
        auto* shapes = row(emission, "shape")->createChild<rt::ComboBox>();
        shapes->addItem("Point");
        shapes->addItem("Circle");
        shapes->addItem("Rectangle");
        shapes->setSelectedIndex(static_cast<int>(system->GetEmitterShape()));
        shapes->setStretch(1.0f);
        shapes->selectionChanged.connect([this, system](int i) {
            system->SetEmitterShape(static_cast<ParticleEmitterShape>(i));
            edited.emit("Emitter shape", true);
        });
        rt::BoxLayout* size = row(emission, "area");
        number(size, "w", system->EmitterSize().x, 1.0, 0.0, [system](float v) {
            system->SetEmitterSize(Math::Vec2(v, system->EmitterSize().y));
        });
        number(size, "h", system->EmitterSize().y, 1.0, 0.0, [system](float v) {
            system->SetEmitterSize(Math::Vec2(system->EmitterSize().x, v));
        });
        rt::BoxLayout* gravity = row(emission, "gravity");
        number(gravity, "x", system->Gravity().x, 1.0, -1.0, [system](float v) {
            system->SetGravity(Math::Vec2(v, system->Gravity().y));
        });
        number(gravity, "y", system->Gravity().y, 1.0, -1.0, [system](float v) {
            system->SetGravity(Math::Vec2(system->Gravity().x, v));
        });
    }

    auto* motion = mFields->createChild<rt::Collapsible>("Motion");
    {
        rt::BoxLayout* direction = row(motion, "direction");
        number(direction, "x", p.direction.x, 0.05, -1.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.direction.x = v; }); });
        number(direction, "y", p.direction.y, 0.05, -1.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.direction.y = v; }); });
        number(row(motion, "spread"), "deg", p.spreadDegrees, 1.0, 0.0,
               [edit](float v) { edit([v](ParticlePrefab& q) { q.spreadDegrees = v; }); });
        rt::BoxLayout* speed = row(motion, "speed");
        number(speed, "min", p.speedMin, 1.0, 0.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.speedMin = v; }); });
        number(speed, "max", p.speedMax, 1.0, 0.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.speedMax = v; }); });
        number(row(motion, "drag"), "", p.drag, 0.05, 0.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.drag = v; }); });
        rt::BoxLayout* life = row(motion, "life");
        number(life, "min", p.lifeMin, 0.05, 0.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.lifeMin = v; }); });
        number(life, "max", p.lifeMax, 0.05, 0.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.lifeMax = v; }); });
    }

    auto* look = mFields->createChild<rt::Collapsible>("Look");
    {
        rt::BoxLayout* size = row(look, "size");
        number(size, "min", p.sizeMin, 0.5, 0.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.sizeMin = v; }); });
        number(size, "max", p.sizeMax, 0.5, 0.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.sizeMax = v; }); });
        number(row(look, "end size"), "", p.endSize, 0.5, 0.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.endSize = v; }); });
        rt::BoxLayout* rotation = row(look, "rotation");
        number(rotation, "min", p.rotationMin, 1.0, -1.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.rotationMin = v; }); });
        number(rotation, "max", p.rotationMax, 1.0, -1.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.rotationMax = v; }); });
        rt::BoxLayout* spin = row(look, "spin");
        number(spin, "min", p.angularVelocityMin, 1.0, -1.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.angularVelocityMin = v; }); });
        number(spin, "max", p.angularVelocityMax, 1.0, -1.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.angularVelocityMax = v; }); });
        rt::BoxLayout* fade = row(look, "fade");
        number(fade, "in", p.fadeIn, 0.05, 0.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.fadeIn = v; }); });
        number(fade, "out", p.fadeOut, 0.05, 0.0, [edit](float v) { edit([v](ParticlePrefab& q) { q.fadeOut = v; }); });
        rt::BoxLayout* face = row(look, "face motion");
        auto* faceCheck = face->createChild<rt::CheckBox>();
        faceCheck->setChecked(p.faceDirection);
        faceCheck->toggled.connect([this, edit](bool on) {
            edit([on](ParticlePrefab& q) { q.faceDirection = on; });
            edited.emit("Edit particles", true);
        });
        number(face, "offset", p.faceDirectionOffsetDegrees, 1.0, -1.0,
               [edit](float v) { edit([v](ParticlePrefab& q) { q.faceDirectionOffsetDegrees = v; }); });

        // Colours as four sliders each, until there is a swatch with a picker.
        const char* channels[4] = {"r", "g", "b", "a"};
        for (int which = 0; which < 2; ++which)
        {
            rt::BoxLayout* line = row(look, which == 0 ? "colour start" : "colour end");
            const Color c = which == 0 ? p.colorStart : p.colorEnd;
            const float values[4] = {c.r, c.g, c.b, c.a};
            for (int i = 0; i < 4; ++i)
                number(line, channels[i], values[i], 0.01, 0.0, [edit, which, i](float v) {
                    edit([v, which, i](ParticlePrefab& q) {
                        Color& target = which == 0 ? q.colorStart : q.colorEnd;
                        float* parts[4] = {&target.r, &target.g, &target.b, &target.a};
                        *parts[i] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
                    });
                });
        }
    }
    rt::WidgetApp::instance().requestLayout();
}

} // namespace k2d::editor
