#include "ParticlesPanel.h"

#include "Icons.h"
#include "PropertyFields.h"

#include "BasicWidgets.hpp"
#include "ComboBox.hpp"
#include "ToolBar.hpp"
#include "WidgetApp.hpp"

#include <k2d/ClassDB.h>
#include <k2d/GameObject.h>
#include <k2d/ParticleComponent.h>

#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{

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

// The emitter's properties as the ClassDB describes them, in three groups;
// the same rows and fields as the Inspector's.
void ParticlesPanel::rebuild()
{
    while (!mFields->children().empty())
        mFields->removeChild(mFields->children().back());
    ParticleComponent* particles = emitter();
    const ClassInfo* info = particles ? ClassDB::find(*particles) : nullptr;
    if (!info)
        return;
    static const char* const kEmission[] = {"texture", "mode", "emissionRate", "oneShotCount", "capacity",
                                            "emitterShape", "emitterSize", "gravity", "followOwner", nullptr};
    static const char* const kMotion[] = {"direction", "spreadDegrees", "speedMin", "speedMax", "drag",
                                          "lifeMin", "lifeMax", nullptr};
    static const char* const kLook[] = {"sizeMin", "sizeMax", "endSize", "rotationMin", "rotationMax",
                                        "angularVelocityMin", "angularVelocityMax", "fadeIn", "fadeOut",
                                        "faceDirection", "faceDirectionOffsetDegrees", "colorStart", "colorEnd",
                                        "blendMode", "ySort", nullptr};
    struct Group
    {
        const char* title;
        const char* const* names;
    };
    const Group groups[] = {{"Emission", kEmission}, {"Motion", kMotion}, {"Look", kLook}};
    for (const Group& group : groups)
    {
        auto* section = mFields->createChild<rt::Collapsible>(group.title);
        for (const char* const* name = group.names; *name; ++name)
        {
            const ClassProperty* property = info->find(*name);
            if (!property)
                continue;
            FieldBinding binding;
            binding.get = [this, property]() {
                ParticleComponent* p = emitter();
                return p ? property->get(*p) : PropertyValue();
            };
            binding.set = [this, property](const PropertyValue& v) {
                if (ParticleComponent* p = emitter())
                    property->set(*p, v);
            };
            addPropertyRow(section, property->info, binding, [this](bool final) { edited.emit("Edit particles", final); });
        }
    }
    rt::WidgetApp::instance().requestLayout();
}

} // namespace k2d::editor
