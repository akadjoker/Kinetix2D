#include "k2d/ClassDB.h"

#include "k2d/ActionSequence2D.h"
#include "k2d/Animation2D.h"
#include "k2d/AudioPlayer.h"
#include "k2d/Bone2D.h"
#include "k2d/BoxCollider2D.h"
#include "k2d/CameraComponent.h"
#include "k2d/CapsuleShape.h"
#include "k2d/ChainCollider2D.h"
#include "k2d/CharacterBody2D.h"
#include "k2d/CircleCollider2D.h"
#include "k2d/CircleShape.h"
#include "k2d/DirectionalLight2D.h"
#include "k2d/DistanceJoint2D.h"
#include "k2d/EdgeCollider2D.h"
#include "k2d/Formation2D.h"
#include "k2d/GearJoint2D.h"
#include "k2d/Light2D.h"
#include "k2d/LightOccluder2D.h"
#include "k2d/Line2D.h"
#include "k2d/MotionStreak2D.h"
#include "k2d/MotionTween2D.h"
#include "k2d/MotorJoint2D.h"
#include "k2d/MouseJoint2D.h"
#include "k2d/NavigationAgent2D.h"
#include "k2d/NavigationRegion2D.h"
#include "k2d/NinePatchComponent.h"
#include "k2d/ParallaxLayerComponent.h"
#include "k2d/ParticleComponent.h"
#include "k2d/PathMotion2D.h"
#include "k2d/Polygon2D.h"
#include "k2d/PolygonCollider2D.h"
#include "k2d/RectShape.h"
#include "k2d/RevoluteJoint2D.h"
#include "k2d/RigidBody2D.h"
#include "k2d/ScriptComponent.h"
#include "k2d/Skeleton2D.h"
#include "k2d/SpriteBatch.h"
#include "k2d/SpriteComponent.h"
#include "k2d/Steering2D.h"
#include "k2d/TileMapCollider2D.h"
#include "k2d/TileMapComponent.h"
#include "k2d/WheelJoint2D.h"

namespace k2d
{

void RegisterCoreClasses()
{
    if (ClassDB::count() > 0)
        return;

    {
    ClassInfo& c = ClassDB::add("ActionSequence2D", ComponentType::ActionSequence, ComponentClass::None);
    ClassDB::addProperty<ActionSequence2D>(c, PropertyInfo::enumeration("loop", "OneShot,Loop"), &ActionSequence2D::loop, &ActionSequence2D::setLoop);
    ClassDB::addProperty<ActionSequence2D>(c, PropertyInfo(PropertyType::Bool, "autoplay"), &ActionSequence2D::autoplay, &ActionSequence2D::setAutoplay);
    }
    {
    ClassInfo& c = ClassDB::add("Animation2D", ComponentType::Animation, ComponentClass::None);
    ClassDB::addProperty<Animation2D>(c, PropertyInfo::enumeration("mode", "OneShot,Loop,PingPong"), &Animation2D::mode, &Animation2D::setMode);
    ClassDB::addProperty<Animation2D>(c, PropertyInfo(PropertyType::Bool, "loop"), &Animation2D::loop, &Animation2D::setLoop);
    ClassDB::addProperty<Animation2D>(c, PropertyInfo(PropertyType::Int, "frame"), &Animation2D::frame, &Animation2D::setFrame);
    }
    {
    ClassInfo& c = ClassDB::add("AudioPlayer", ComponentType::AudioPlayer, ComponentClass::None);
    ClassDB::addProperty<AudioPlayer>(c, PropertyInfo(PropertyType::Bool, "music"), &AudioPlayer::music, &AudioPlayer::setMusic);
    ClassDB::addProperty<AudioPlayer>(c, PropertyInfo(PropertyType::Bool, "autoplay"), &AudioPlayer::autoplay, &AudioPlayer::setAutoplay);
    ClassDB::addProperty<AudioPlayer>(c, PropertyInfo(PropertyType::Bool, "loop"), &AudioPlayer::loop, &AudioPlayer::setLoop);
    ClassDB::addProperty<AudioPlayer>(c, PropertyInfo(PropertyType::Float, "volume"), &AudioPlayer::volume, &AudioPlayer::setVolume);
    ClassDB::addProperty<AudioPlayer>(c, PropertyInfo(PropertyType::Float, "pitch"), &AudioPlayer::pitch, &AudioPlayer::setPitch);
    ClassDB::addProperty<AudioPlayer>(c, PropertyInfo(PropertyType::Float, "pan"), &AudioPlayer::pan, &AudioPlayer::setPan);
    ClassDB::addProperty<AudioPlayer>(c, PropertyInfo(PropertyType::Bool, "spatial"), &AudioPlayer::spatial, &AudioPlayer::setSpatial);
    ClassDB::addProperty<AudioPlayer>(c, PropertyInfo(PropertyType::Float, "minDistance"), &AudioPlayer::minDistance, &AudioPlayer::setMinDistance);
    ClassDB::addProperty<AudioPlayer>(c, PropertyInfo(PropertyType::Float, "maxDistance"), &AudioPlayer::maxDistance, &AudioPlayer::setMaxDistance);
    ClassDB::addProperty<AudioPlayer>(c, PropertyInfo(PropertyType::Float, "rolloff"), &AudioPlayer::rolloff, &AudioPlayer::setRolloff);
    }
    {
    ClassInfo& c = ClassDB::add("Bone2D", ComponentType::Bone, ComponentClass::None);
    ClassDB::addProperty<Bone2D>(c, PropertyInfo(PropertyType::Float, "length"), &Bone2D::length, &Bone2D::setLength);
    }
    {
    ClassInfo& c = ClassDB::add("BoxCollider2D", ComponentType::Collider, ComponentClass::BoxCollider2D);
    ClassDB::addProperty<BoxCollider2D>(c, PropertyInfo(PropertyType::Vec2, "size"), &BoxCollider2D::size, &BoxCollider2D::setSize);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Vec2, "offset"), &Collider2D::offset, &Collider2D::setOffset);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Bool, "sensor"), &Collider2D::isSensor, &Collider2D::setSensor);
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "category"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).category())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(static_cast<uint16_t>(value.asInt()), collider.mask());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "mask"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).mask())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(collider.category(), static_cast<uint16_t>(value.asInt()));
        });
    }
    {
    ClassInfo& c = ClassDB::add("CameraComponent", ComponentType::Camera, ComponentClass::None);
    ClassDB::addProperty<CameraComponent>(c, PropertyInfo::enumeration("viewportScaleMode", "Fit,Expand"), &CameraComponent::viewportScaleMode, &CameraComponent::setViewportScaleMode);
    ClassDB::addProperty<CameraComponent>(c, PropertyInfo(PropertyType::Bool, "integerScale"), &CameraComponent::integerScale, &CameraComponent::setIntegerScale);
    ClassDB::addProperty<CameraComponent>(c, PropertyInfo(PropertyType::Int, "renderPriority"), &CameraComponent::renderPriority, &CameraComponent::setRenderPriority);
    ClassDB::addProperty<CameraComponent>(c, PropertyInfo(PropertyType::String, "followTargetName"), &CameraComponent::followTargetName, &CameraComponent::setFollowTargetName);
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Float, "viewportWidth"),
        [](const Component& component) { return PropertyValue::from(static_cast<const CameraComponent&>(component).viewportWidth()); },
        [](Component& component, const PropertyValue& value)
        {
            CameraComponent& camera = static_cast<CameraComponent&>(component);
            camera.setViewport(value.asFloat(), camera.viewportHeight());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Float, "viewportHeight"),
        [](const Component& component) { return PropertyValue::from(static_cast<const CameraComponent&>(component).viewportHeight()); },
        [](Component& component, const PropertyValue& value)
        {
            CameraComponent& camera = static_cast<CameraComponent&>(component);
            camera.setViewport(camera.viewportWidth(), value.asFloat());
        });
    }
    {
    ClassInfo& c = ClassDB::add("CapsuleShape", ComponentType::CapsuleShape, ComponentClass::None);
    ClassDB::addProperty<CapsuleShape>(c, PropertyInfo(PropertyType::Vec2, "size"), &CapsuleShape::size, &CapsuleShape::setSize);
    ClassDB::addProperty<CapsuleShape>(c, PropertyInfo(PropertyType::Int, "segments"), &CapsuleShape::segments, &CapsuleShape::setSegments);
    ClassDB::addProperty<CapsuleShape>(c, PropertyInfo::enumeration("mode", "Fill,Line"), &CapsuleShape::mode, &CapsuleShape::setMode);
    ClassDB::addProperty<CapsuleShape>(c, PropertyInfo(PropertyType::Float, "lineWidth"), &CapsuleShape::lineWidth, &CapsuleShape::setLineWidth);
    ClassDB::addProperty<CapsuleShape>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &CapsuleShape::blendMode, &CapsuleShape::setBlendMode);
    ClassDB::addProperty(
        c, PropertyInfo::color("color"),
        [](const Component& component) { return PropertyValue::from(static_cast<const CapsuleShape&>(component).color()); },
        [](Component& component, const PropertyValue& value)
        {
            const Color color = value.asColor();
            static_cast<CapsuleShape&>(component).setColor(
                static_cast<unsigned char>(color.r * 255.0f + 0.5f), static_cast<unsigned char>(color.g * 255.0f + 0.5f),
                static_cast<unsigned char>(color.b * 255.0f + 0.5f), static_cast<unsigned char>(color.a * 255.0f + 0.5f));
        });
    }
    {
    ClassInfo& c = ClassDB::add("ChainCollider2D", ComponentType::Collider, ComponentClass::ChainCollider2D);
    ClassDB::addProperty<ChainCollider2D>(c, PropertyInfo(PropertyType::Bool, "loop"), &ChainCollider2D::loop, &ChainCollider2D::setLoop);
    ClassDB::addProperty<ChainCollider2D>(c, PropertyInfo(PropertyType::Bool, "oneWay"), &ChainCollider2D::oneWay, &ChainCollider2D::setOneWay);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Vec2, "offset"), &Collider2D::offset, &Collider2D::setOffset);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Bool, "sensor"), &Collider2D::isSensor, &Collider2D::setSensor);
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "category"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).category())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(static_cast<uint16_t>(value.asInt()), collider.mask());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "mask"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).mask())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(collider.category(), static_cast<uint16_t>(value.asInt()));
        });
    }
    {
    ClassInfo& c = ClassDB::add("CharacterBody2D", ComponentType::CharacterBody, ComponentClass::None);
    ClassDB::addProperty<CharacterBody2D>(c, PropertyInfo(PropertyType::Vec2, "velocity"), &CharacterBody2D::velocity, &CharacterBody2D::setVelocity);
    ClassDB::addProperty<CharacterBody2D>(c, PropertyInfo(PropertyType::Float, "safeMargin"), &CharacterBody2D::safeMargin, &CharacterBody2D::setSafeMargin);
    ClassDB::addProperty<CharacterBody2D>(c, PropertyInfo(PropertyType::Int, "maxSlides"), &CharacterBody2D::maxSlides, &CharacterBody2D::setMaxSlides);
    ClassDB::addProperty<CharacterBody2D>(c, PropertyInfo::enumeration("motionMode", "Floating,Grounded"), &CharacterBody2D::motionMode, &CharacterBody2D::setMotionMode);
    ClassDB::addProperty<CharacterBody2D>(c, PropertyInfo(PropertyType::Vec2, "upDirection"), &CharacterBody2D::upDirection, &CharacterBody2D::setUpDirection);
    ClassDB::addProperty<CharacterBody2D>(c, PropertyInfo(PropertyType::Float, "floorMaxAngleDegrees"), &CharacterBody2D::floorMaxAngleDegrees, &CharacterBody2D::setFloorMaxAngleDegrees);
    }
    {
    ClassInfo& c = ClassDB::add("CircleCollider2D", ComponentType::Collider, ComponentClass::CircleCollider2D);
    ClassDB::addProperty<CircleCollider2D>(c, PropertyInfo(PropertyType::Float, "radius"), &CircleCollider2D::radius, &CircleCollider2D::setRadius);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Vec2, "offset"), &Collider2D::offset, &Collider2D::setOffset);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Bool, "sensor"), &Collider2D::isSensor, &Collider2D::setSensor);
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "category"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).category())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(static_cast<uint16_t>(value.asInt()), collider.mask());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "mask"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).mask())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(collider.category(), static_cast<uint16_t>(value.asInt()));
        });
    }
    {
    ClassInfo& c = ClassDB::add("CircleShape", ComponentType::CircleShape, ComponentClass::None);
    ClassDB::addProperty<CircleShape>(c, PropertyInfo(PropertyType::Float, "radius"), &CircleShape::radius, &CircleShape::setRadius);
    ClassDB::addProperty<CircleShape>(c, PropertyInfo(PropertyType::Int, "segments"), &CircleShape::segments, &CircleShape::setSegments);
    ClassDB::addProperty<CircleShape>(c, PropertyInfo::enumeration("mode", "Fill,Line"), &CircleShape::mode, &CircleShape::setMode);
    ClassDB::addProperty<CircleShape>(c, PropertyInfo(PropertyType::Float, "lineWidth"), &CircleShape::lineWidth, &CircleShape::setLineWidth);
    ClassDB::addProperty<CircleShape>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &CircleShape::blendMode, &CircleShape::setBlendMode);
    ClassDB::addProperty(
        c, PropertyInfo::color("color"),
        [](const Component& component) { return PropertyValue::from(static_cast<const CircleShape&>(component).color()); },
        [](Component& component, const PropertyValue& value)
        {
            const Color color = value.asColor();
            static_cast<CircleShape&>(component).setColor(
                static_cast<unsigned char>(color.r * 255.0f + 0.5f), static_cast<unsigned char>(color.g * 255.0f + 0.5f),
                static_cast<unsigned char>(color.b * 255.0f + 0.5f), static_cast<unsigned char>(color.a * 255.0f + 0.5f));
        });
    }
    {
    ClassInfo& c = ClassDB::add("DirectionalLight2D", ComponentType::Light, ComponentClass::DirectionalLight2D);
    ClassDB::addProperty<DirectionalLight2D>(c, PropertyInfo(PropertyType::Float, "energy"), &DirectionalLight2D::energy, &DirectionalLight2D::setEnergy);
    ClassDB::addProperty<DirectionalLight2D>(c, PropertyInfo(PropertyType::Bool, "castShadow"), &DirectionalLight2D::castShadow, &DirectionalLight2D::setCastShadow);
    ClassDB::addProperty<DirectionalLight2D>(c, PropertyInfo::enumeration("shadowFilter", "Nearest,PCF5,PCF13"), &DirectionalLight2D::shadowFilter, &DirectionalLight2D::setShadowFilter);
    ClassDB::addProperty<DirectionalLight2D>(c, PropertyInfo(PropertyType::Int, "cullMask"), &DirectionalLight2D::cullMask, &DirectionalLight2D::setCullMask);
    ClassDB::addProperty<DirectionalLight2D>(c, PropertyInfo(PropertyType::Float, "height"), &DirectionalLight2D::height, &DirectionalLight2D::setHeight);
    ClassDB::addProperty(
        c, PropertyInfo::color("color"),
        [](const Component& component) { return PropertyValue::from(static_cast<const DirectionalLight2D&>(component).color()); },
        [](Component& component, const PropertyValue& value)
        {
            const Color color = value.asColor();
            static_cast<DirectionalLight2D&>(component).setColor(color.r, color.g, color.b, color.a);
        });
    ClassDB::addProperty(
        c, PropertyInfo::color("shadowColor"),
        [](const Component& component) { return PropertyValue::from(static_cast<const DirectionalLight2D&>(component).shadowColor()); },
        [](Component& component, const PropertyValue& value)
        {
            const Color color = value.asColor();
            static_cast<DirectionalLight2D&>(component).setShadowColor(color.r, color.g, color.b, color.a);
        });
    }
    {
    ClassInfo& c = ClassDB::add("DistanceJoint2D", ComponentType::Joint, ComponentClass::DistanceJoint2D);
    ClassDB::addProperty<DistanceJoint2D>(c, PropertyInfo(PropertyType::Vec2, "localAnchorA"), &DistanceJoint2D::localAnchorA, &DistanceJoint2D::setLocalAnchorA);
    ClassDB::addProperty<DistanceJoint2D>(c, PropertyInfo(PropertyType::Vec2, "localAnchorB"), &DistanceJoint2D::localAnchorB, &DistanceJoint2D::setLocalAnchorB);
    ClassDB::addProperty<DistanceJoint2D>(c, PropertyInfo(PropertyType::Float, "length"), &DistanceJoint2D::length, &DistanceJoint2D::setLength);
    ClassDB::addProperty<DistanceJoint2D>(c, PropertyInfo(PropertyType::Bool, "lengthConfigured"), &DistanceJoint2D::lengthConfigured, &DistanceJoint2D::setLengthConfigured);
    ClassDB::addProperty<Joint2D>(c, PropertyInfo(PropertyType::Bool, "collideConnected"), &Joint2D::collideConnected, &Joint2D::setCollideConnected);
    }
    {
    ClassInfo& c = ClassDB::add("EdgeCollider2D", ComponentType::Collider, ComponentClass::EdgeCollider2D);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Vec2, "offset"), &Collider2D::offset, &Collider2D::setOffset);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Bool, "sensor"), &Collider2D::isSensor, &Collider2D::setSensor);
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "category"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).category())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(static_cast<uint16_t>(value.asInt()), collider.mask());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "mask"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).mask())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(collider.category(), static_cast<uint16_t>(value.asInt()));
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Vec2, "start"),
        [](const Component& component) { return PropertyValue::from(static_cast<const EdgeCollider2D&>(component).start()); },
        [](Component& component, const PropertyValue& value)
        {
            EdgeCollider2D& edge = static_cast<EdgeCollider2D&>(component);
            edge.setPoints(value.asVec2(), edge.end());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Vec2, "end"),
        [](const Component& component) { return PropertyValue::from(static_cast<const EdgeCollider2D&>(component).end()); },
        [](Component& component, const PropertyValue& value)
        {
            EdgeCollider2D& edge = static_cast<EdgeCollider2D&>(component);
            edge.setPoints(edge.start(), value.asVec2());
        });
    }
    {
    ClassInfo& c = ClassDB::add("Formation2D", ComponentType::Formation, ComponentClass::None);
    ClassDB::addProperty<Formation2D>(c, PropertyInfo(PropertyType::String, "groupTag"), &Formation2D::groupTag, &Formation2D::setGroupTag);
    ClassDB::addProperty<Formation2D>(c, PropertyInfo(PropertyType::String, "anchorName"), &Formation2D::anchorName, &Formation2D::setAnchorName);
    ClassDB::addProperty<Formation2D>(c, PropertyInfo::enumeration("shape", "Surround,Abreast,Wedge,SingleFile"), &Formation2D::shape, &Formation2D::setShape);
    ClassDB::addProperty<Formation2D>(c, PropertyInfo(PropertyType::Float, "spacing"), &Formation2D::spacing, &Formation2D::setSpacing);
    ClassDB::addProperty<Formation2D>(c, PropertyInfo(PropertyType::Float, "updateInterval"), &Formation2D::updateInterval, &Formation2D::setUpdateInterval);
    }
    {
    ClassInfo& c = ClassDB::add("GearJoint2D", ComponentType::Joint, ComponentClass::GearJoint2D);
    ClassDB::addProperty<GearJoint2D>(c, PropertyInfo(PropertyType::Float, "ratio"), &GearJoint2D::ratio, &GearJoint2D::setRatio);
    ClassDB::addProperty<Joint2D>(c, PropertyInfo(PropertyType::Bool, "collideConnected"), &Joint2D::collideConnected, &Joint2D::setCollideConnected);
    }
    {
    ClassInfo& c = ClassDB::add("Light2D", ComponentType::Light, ComponentClass::Light2D);
    ClassDB::addProperty<Light2D>(c, PropertyInfo(PropertyType::Float, "energy"), &Light2D::energy, &Light2D::setEnergy);
    ClassDB::addProperty<Light2D>(c, PropertyInfo(PropertyType::Float, "radius"), &Light2D::radius, &Light2D::setRadius);
    ClassDB::addProperty<Light2D>(c, PropertyInfo(PropertyType::Bool, "castShadow"), &Light2D::castShadow, &Light2D::setCastShadow);
    ClassDB::addProperty<Light2D>(c, PropertyInfo::enumeration("shadowFilter", "Nearest,PCF5,PCF13"), &Light2D::shadowFilter, &Light2D::setShadowFilter);
    ClassDB::addProperty<Light2D>(c, PropertyInfo(PropertyType::Int, "cullMask"), &Light2D::cullMask, &Light2D::setCullMask);
    ClassDB::addProperty<Light2D>(c, PropertyInfo(PropertyType::Float, "height"), &Light2D::height, &Light2D::setHeight);
    ClassDB::addProperty(
        c, PropertyInfo::color("color"),
        [](const Component& component) { return PropertyValue::from(static_cast<const Light2D&>(component).color()); },
        [](Component& component, const PropertyValue& value)
        {
            const Color color = value.asColor();
            static_cast<Light2D&>(component).setColor(color.r, color.g, color.b, color.a);
        });
    ClassDB::addProperty(
        c, PropertyInfo::color("shadowColor"),
        [](const Component& component) { return PropertyValue::from(static_cast<const Light2D&>(component).shadowColor()); },
        [](Component& component, const PropertyValue& value)
        {
            const Color color = value.asColor();
            static_cast<Light2D&>(component).setShadowColor(color.r, color.g, color.b, color.a);
        });
    }
    {
    ClassInfo& c = ClassDB::add("LightOccluder2D", ComponentType::Occluder, ComponentClass::None);
    }
    {
    ClassInfo& c = ClassDB::add("Line2D", ComponentType::LinePath, ComponentClass::None);
    ClassDB::addProperty<Line2D>(c, PropertyInfo(PropertyType::Float, "width"), &Line2D::width, &Line2D::setWidth);
    ClassDB::addProperty<Line2D>(c, PropertyInfo(PropertyType::Bool, "closed"), &Line2D::closed, &Line2D::setClosed);
    ClassDB::addProperty<Line2D>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &Line2D::blendMode, &Line2D::setBlendMode);
    ClassDB::addProperty(
        c, PropertyInfo::color("color"),
        [](const Component& component) { return PropertyValue::from(static_cast<const Line2D&>(component).color()); },
        [](Component& component, const PropertyValue& value)
        {
            const Color color = value.asColor();
            static_cast<Line2D&>(component).setColor(
                static_cast<unsigned char>(color.r * 255.0f + 0.5f), static_cast<unsigned char>(color.g * 255.0f + 0.5f),
                static_cast<unsigned char>(color.b * 255.0f + 0.5f), static_cast<unsigned char>(color.a * 255.0f + 0.5f));
        });
    }
    {
    ClassInfo& c = ClassDB::add("MotionStreak2D", ComponentType::MotionStreak, ComponentClass::None);
    ClassDB::addProperty<MotionStreak2D>(c, PropertyInfo(PropertyType::Float, "lifetime"), &MotionStreak2D::lifetime, &MotionStreak2D::setLifetime);
    ClassDB::addProperty<MotionStreak2D>(c, PropertyInfo(PropertyType::Float, "width"), &MotionStreak2D::width, &MotionStreak2D::setWidth);
    ClassDB::addProperty<MotionStreak2D>(c, PropertyInfo(PropertyType::Float, "minDistance"), &MotionStreak2D::minDistance, &MotionStreak2D::setMinDistance);
    ClassDB::addProperty<MotionStreak2D>(c, PropertyInfo::color("color"), &MotionStreak2D::color, &MotionStreak2D::setColor);
    ClassDB::addProperty<MotionStreak2D>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &MotionStreak2D::blendMode, &MotionStreak2D::setBlendMode);
    }
    {
    ClassInfo& c = ClassDB::add("MotionTween2D", ComponentType::MotionTween, ComponentClass::None);
    ClassDB::addProperty<MotionTween2D>(c, PropertyInfo::enumeration("loop", "None,Repeat,PingPong"), &MotionTween2D::loop, &MotionTween2D::setLoop);
    ClassDB::addProperty<MotionTween2D>(c, PropertyInfo(PropertyType::Bool, "autoplay"), &MotionTween2D::autoplay, &MotionTween2D::setAutoplay);
    ClassDB::addProperty<MotionTween2D>(c, PropertyInfo(PropertyType::Bool, "oneShot"), &MotionTween2D::oneShot, &MotionTween2D::setOneShot);
    }
    {
    ClassInfo& c = ClassDB::add("MotorJoint2D", ComponentType::Joint, ComponentClass::MotorJoint2D);
    ClassDB::addProperty<MotorJoint2D>(c, PropertyInfo(PropertyType::Vec2, "linearOffset"), &MotorJoint2D::linearOffset, &MotorJoint2D::setLinearOffset);
    ClassDB::addProperty<MotorJoint2D>(c, PropertyInfo(PropertyType::Float, "angularOffset"), &MotorJoint2D::angularOffset, &MotorJoint2D::setAngularOffset);
    ClassDB::addProperty<MotorJoint2D>(c, PropertyInfo(PropertyType::Float, "maxForce"), &MotorJoint2D::maxForce, &MotorJoint2D::setMaxForce);
    ClassDB::addProperty<MotorJoint2D>(c, PropertyInfo(PropertyType::Float, "maxTorque"), &MotorJoint2D::maxTorque, &MotorJoint2D::setMaxTorque);
    ClassDB::addProperty<MotorJoint2D>(c, PropertyInfo(PropertyType::Float, "correctionFactor"), &MotorJoint2D::correctionFactor, &MotorJoint2D::setCorrectionFactor);
    ClassDB::addProperty<Joint2D>(c, PropertyInfo(PropertyType::Bool, "collideConnected"), &Joint2D::collideConnected, &Joint2D::setCollideConnected);
    }
    {
    ClassInfo& c = ClassDB::add("MouseJoint2D", ComponentType::Joint, ComponentClass::MouseJoint2D);
    ClassDB::addProperty<MouseJoint2D>(c, PropertyInfo(PropertyType::Vec2, "target"), &MouseJoint2D::target, &MouseJoint2D::setTarget);
    ClassDB::addProperty<MouseJoint2D>(c, PropertyInfo(PropertyType::Float, "maxForce"), &MouseJoint2D::maxForce, &MouseJoint2D::setMaxForce);
    ClassDB::addProperty<Joint2D>(c, PropertyInfo(PropertyType::Bool, "collideConnected"), &Joint2D::collideConnected, &Joint2D::setCollideConnected);
    }
    {
    ClassInfo& c = ClassDB::add("NavigationAgent2D", ComponentType::NavigationAgent, ComponentClass::None);
    ClassDB::addProperty<NavigationAgent2D>(c, PropertyInfo(PropertyType::String, "followTargetName"), &NavigationAgent2D::followTargetName, &NavigationAgent2D::setFollowTargetName);
    ClassDB::addProperty<NavigationAgent2D>(c, PropertyInfo(PropertyType::Float, "repathInterval"), &NavigationAgent2D::repathInterval, &NavigationAgent2D::setRepathInterval);
    ClassDB::addProperty<NavigationAgent2D>(c, PropertyInfo(PropertyType::Float, "repathMoveThreshold"), &NavigationAgent2D::repathMoveThreshold, &NavigationAgent2D::setRepathMoveThreshold);
    ClassDB::addProperty<NavigationAgent2D>(c, PropertyInfo(PropertyType::Float, "pathDesiredDistance"), &NavigationAgent2D::pathDesiredDistance, &NavigationAgent2D::setPathDesiredDistance);
    ClassDB::addProperty<NavigationAgent2D>(c, PropertyInfo(PropertyType::Float, "maxSpeed"), &NavigationAgent2D::maxSpeed, &NavigationAgent2D::setMaxSpeed);
    ClassDB::addProperty<NavigationAgent2D>(c, PropertyInfo(PropertyType::Bool, "autoMove"), &NavigationAgent2D::autoMove, &NavigationAgent2D::setAutoMove);
    ClassDB::addProperty<NavigationAgent2D>(c, PropertyInfo(PropertyType::Bool, "orientToPath"), &NavigationAgent2D::orientToPath, &NavigationAgent2D::setOrientToPath);
    ClassDB::addProperty<NavigationAgent2D>(c, PropertyInfo(PropertyType::Float, "rotationLerpSpeed"), &NavigationAgent2D::rotationLerpSpeed, &NavigationAgent2D::setRotationLerpSpeed);
    ClassDB::addProperty<NavigationAgent2D>(c, PropertyInfo(PropertyType::Float, "rotationOffsetDegrees"), &NavigationAgent2D::rotationOffsetDegrees, &NavigationAgent2D::setRotationOffsetDegrees);
    }
    {
    ClassInfo& c = ClassDB::add("NavigationRegion2D", ComponentType::NavigationRegion, ComponentClass::None);
    }
    {
    ClassInfo& c = ClassDB::add("NinePatchComponent", ComponentType::NinePatch, ComponentClass::None);
    ClassDB::addProperty<NinePatchComponent>(c, PropertyInfo(PropertyType::Vec2, "size"), &NinePatchComponent::size, &NinePatchComponent::setSize);
    ClassDB::addProperty<NinePatchComponent>(c, PropertyInfo(PropertyType::Vec2, "pivot"), &NinePatchComponent::pivot, &NinePatchComponent::setPivot);
    ClassDB::addProperty<NinePatchComponent>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &NinePatchComponent::blendMode, &NinePatchComponent::setBlendMode);
    ClassDB::addProperty(
        c, PropertyInfo::color("color"),
        [](const Component& component) { return PropertyValue::from(static_cast<const NinePatchComponent&>(component).color()); },
        [](Component& component, const PropertyValue& value)
        {
            const Color color = value.asColor();
            static_cast<NinePatchComponent&>(component).setColor(
                static_cast<unsigned char>(color.r * 255.0f + 0.5f), static_cast<unsigned char>(color.g * 255.0f + 0.5f),
                static_cast<unsigned char>(color.b * 255.0f + 0.5f), static_cast<unsigned char>(color.a * 255.0f + 0.5f));
        });
    }
    {
    ClassInfo& c = ClassDB::add("ParallaxLayerComponent", ComponentType::ParallaxLayer, ComponentClass::None);
    ClassDB::addProperty<ParallaxLayerComponent>(c, PropertyInfo(PropertyType::Vec2, "motionScale"), &ParallaxLayerComponent::motionScale, &ParallaxLayerComponent::setMotionScale);
    ClassDB::addProperty<ParallaxLayerComponent>(c, PropertyInfo(PropertyType::Vec2, "tileSize"), &ParallaxLayerComponent::tileSize, &ParallaxLayerComponent::setTileSize);
    ClassDB::addProperty<ParallaxLayerComponent>(c, PropertyInfo::color("color"), &ParallaxLayerComponent::color, &ParallaxLayerComponent::setColor);
    ClassDB::addProperty<ParallaxLayerComponent>(c, PropertyInfo(PropertyType::Int, "zIndex"), &ParallaxLayerComponent::zIndex, &ParallaxLayerComponent::setZIndex);
    }
    {
    ClassInfo& c = ClassDB::add("ParticleComponent", ComponentType::Particle, ComponentClass::None);
    ClassDB::addProperty<ParticleComponent>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &ParticleComponent::blendMode, &ParticleComponent::setBlendMode);
    ClassDB::addProperty<ParticleComponent>(c, PropertyInfo(PropertyType::Bool, "ySort"), &ParticleComponent::ySort, &ParticleComponent::setYSort);
    ClassDB::addProperty<ParticleComponent>(c, PropertyInfo(PropertyType::Bool, "followOwner"), &ParticleComponent::followOwner, &ParticleComponent::setFollowOwner);
    }
    {
    ClassInfo& c = ClassDB::add("PathMotion2D", ComponentType::PathMotion, ComponentClass::None);
    ClassDB::addProperty<PathMotion2D>(c, PropertyInfo::enumeration("loop", "None,Repeat,PingPong"), &PathMotion2D::loop, &PathMotion2D::setLoop);
    ClassDB::addProperty<PathMotion2D>(c, PropertyInfo(PropertyType::Bool, "autoplay"), &PathMotion2D::autoplay, &PathMotion2D::setAutoplay);
    ClassDB::addProperty<PathMotion2D>(c, PropertyInfo(PropertyType::Bool, "oneShot"), &PathMotion2D::oneShot, &PathMotion2D::setOneShot);
    }
    {
    ClassInfo& c = ClassDB::add("Polygon2D", ComponentType::Polygon2D, ComponentClass::None);
    ClassDB::addProperty<Polygon2D>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &Polygon2D::blendMode, &Polygon2D::setBlendMode);
    ClassDB::addProperty(
        c, PropertyInfo::color("color"),
        [](const Component& component) { return PropertyValue::from(static_cast<const Polygon2D&>(component).color()); },
        [](Component& component, const PropertyValue& value)
        {
            const Color color = value.asColor();
            static_cast<Polygon2D&>(component).setColor(
                static_cast<unsigned char>(color.r * 255.0f + 0.5f), static_cast<unsigned char>(color.g * 255.0f + 0.5f),
                static_cast<unsigned char>(color.b * 255.0f + 0.5f), static_cast<unsigned char>(color.a * 255.0f + 0.5f));
        });
    }
    {
    ClassInfo& c = ClassDB::add("PolygonCollider2D", ComponentType::Collider, ComponentClass::PolygonCollider2D);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Vec2, "offset"), &Collider2D::offset, &Collider2D::setOffset);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Bool, "sensor"), &Collider2D::isSensor, &Collider2D::setSensor);
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "category"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).category())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(static_cast<uint16_t>(value.asInt()), collider.mask());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "mask"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).mask())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(collider.category(), static_cast<uint16_t>(value.asInt()));
        });
    }
    {
    ClassInfo& c = ClassDB::add("RectShape", ComponentType::RectShape, ComponentClass::None);
    ClassDB::addProperty<RectShape>(c, PropertyInfo(PropertyType::Vec2, "size"), &RectShape::size, &RectShape::setSize);
    ClassDB::addProperty<RectShape>(c, PropertyInfo::enumeration("mode", "Fill,Line"), &RectShape::mode, &RectShape::setMode);
    ClassDB::addProperty<RectShape>(c, PropertyInfo(PropertyType::Float, "lineWidth"), &RectShape::lineWidth, &RectShape::setLineWidth);
    ClassDB::addProperty<RectShape>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &RectShape::blendMode, &RectShape::setBlendMode);
    ClassDB::addProperty(
        c, PropertyInfo::color("color"),
        [](const Component& component) { return PropertyValue::from(static_cast<const RectShape&>(component).color()); },
        [](Component& component, const PropertyValue& value)
        {
            const Color color = value.asColor();
            static_cast<RectShape&>(component).setColor(
                static_cast<unsigned char>(color.r * 255.0f + 0.5f), static_cast<unsigned char>(color.g * 255.0f + 0.5f),
                static_cast<unsigned char>(color.b * 255.0f + 0.5f), static_cast<unsigned char>(color.a * 255.0f + 0.5f));
        });
    }
    {
    ClassInfo& c = ClassDB::add("RevoluteJoint2D", ComponentType::Joint, ComponentClass::RevoluteJoint2D);
    ClassDB::addProperty<RevoluteJoint2D>(c, PropertyInfo(PropertyType::Vec2, "localAnchorA"), &RevoluteJoint2D::localAnchorA, &RevoluteJoint2D::setLocalAnchorA);
    ClassDB::addProperty<RevoluteJoint2D>(c, PropertyInfo(PropertyType::Vec2, "localAnchorB"), &RevoluteJoint2D::localAnchorB, &RevoluteJoint2D::setLocalAnchorB);
    ClassDB::addProperty<RevoluteJoint2D>(c, PropertyInfo(PropertyType::Bool, "anchorsConfigured"), &RevoluteJoint2D::anchorsConfigured, &RevoluteJoint2D::setAnchorsConfigured);
    ClassDB::addProperty<RevoluteJoint2D>(c, PropertyInfo(PropertyType::Float, "referenceAngle"), &RevoluteJoint2D::referenceAngle, &RevoluteJoint2D::setReferenceAngle);
    ClassDB::addProperty<Joint2D>(c, PropertyInfo(PropertyType::Bool, "collideConnected"), &Joint2D::collideConnected, &Joint2D::setCollideConnected);
    }
    {
    ClassInfo& c = ClassDB::add("RigidBody2D", ComponentType::RigidBody, ComponentClass::None);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo::enumeration("bodyType", "Static,Kinematic,Dynamic"), &RigidBody2D::bodyType, &RigidBody2D::setBodyType);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo(PropertyType::Float, "density"), &RigidBody2D::density, &RigidBody2D::setDensity);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo(PropertyType::Float, "friction"), &RigidBody2D::friction, &RigidBody2D::setFriction);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo(PropertyType::Float, "restitution"), &RigidBody2D::restitution, &RigidBody2D::setRestitution);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo(PropertyType::Float, "linearDamping"), &RigidBody2D::linearDamping, &RigidBody2D::setLinearDamping);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo(PropertyType::Float, "angularDamping"), &RigidBody2D::angularDamping, &RigidBody2D::setAngularDamping);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo(PropertyType::Float, "gravityScale"), &RigidBody2D::gravityScale, &RigidBody2D::setGravityScale);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo(PropertyType::Bool, "fixedRotation"), &RigidBody2D::fixedRotation, &RigidBody2D::setFixedRotation);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo(PropertyType::Bool, "bullet"), &RigidBody2D::bullet, &RigidBody2D::setBullet);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo(PropertyType::Vec2, "velocity"), &RigidBody2D::velocity, &RigidBody2D::setVelocity);
    ClassDB::addProperty<RigidBody2D>(c, PropertyInfo(PropertyType::Float, "angularVelocity"), &RigidBody2D::angularVelocity, &RigidBody2D::setAngularVelocity);
    }
    {
    ClassInfo& c = ClassDB::add("ScriptComponent", ComponentType::Script, ComponentClass::None);
    }
    {
    ClassInfo& c = ClassDB::add("Skeleton2D", ComponentType::Skeleton, ComponentClass::None);
    ClassDB::addProperty<Skeleton2D>(c, PropertyInfo(PropertyType::Float, "speed"), &Skeleton2D::speed, &Skeleton2D::setSpeed);
    }
    {
    ClassInfo& c = ClassDB::add("SpriteBatch", ComponentType::SpriteBatch, ComponentClass::None);
    ClassDB::addProperty<SpriteBatch>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &SpriteBatch::blendMode, &SpriteBatch::setBlendMode);
    }
    {
    ClassInfo& c = ClassDB::add("SpriteComponent", ComponentType::Sprite, ComponentClass::None);
    ClassDB::addProperty<SpriteComponent>(c, PropertyInfo(PropertyType::Vec2, "size"), &SpriteComponent::size, &SpriteComponent::setSize);
    ClassDB::addProperty<SpriteComponent>(c, PropertyInfo(PropertyType::Vec2, "pivot"), &SpriteComponent::pivot, &SpriteComponent::setPivot);
    ClassDB::addProperty<SpriteComponent>(c, PropertyInfo(PropertyType::Vec2, "renderOffset"), &SpriteComponent::renderOffset, &SpriteComponent::setRenderOffset);
    ClassDB::addProperty<SpriteComponent>(c, PropertyInfo(PropertyType::Int, "lightMask"), &SpriteComponent::lightMask, &SpriteComponent::setLightMask);
    ClassDB::addProperty<SpriteComponent>(c, PropertyInfo(PropertyType::Int, "customShader"), &SpriteComponent::customShader, &SpriteComponent::setCustomShader);
    ClassDB::addProperty<SpriteComponent>(c, PropertyInfo(PropertyType::Bool, "waterEnabled"), &SpriteComponent::waterEnabled, &SpriteComponent::setWaterEnabled);
    ClassDB::addProperty<SpriteComponent>(c, PropertyInfo(PropertyType::Bool, "ySort"), &SpriteComponent::ySort, &SpriteComponent::setYSort);
    ClassDB::addProperty<SpriteComponent>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &SpriteComponent::blendMode, &SpriteComponent::setBlendMode);
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Bool, "flipX"),
        [](const Component& component) { return PropertyValue::from(static_cast<const SpriteComponent&>(component).flipX()); },
        [](Component& component, const PropertyValue& value)
        {
            SpriteComponent& sprite = static_cast<SpriteComponent&>(component);
            sprite.setFlip(value.asBool(), sprite.flipY());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Bool, "flipY"),
        [](const Component& component) { return PropertyValue::from(static_cast<const SpriteComponent&>(component).flipY()); },
        [](Component& component, const PropertyValue& value)
        {
            SpriteComponent& sprite = static_cast<SpriteComponent&>(component);
            sprite.setFlip(sprite.flipX(), value.asBool());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Vec2, "tiling"),
        [](const Component& component) { return PropertyValue::from(static_cast<const SpriteComponent&>(component).tiling()); },
        [](Component& component, const PropertyValue& value)
        {
            const Math::Vec2 tiling = value.asVec2();
            static_cast<SpriteComponent&>(component).setTiling(tiling.x, tiling.y);
        });
    }
    {
    ClassInfo& c = ClassDB::add("Steering2D", ComponentType::Steering, ComponentClass::None);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Float, "weight"), &Steering2D::weight, &Steering2D::setWeight);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Bool, "separationEnabled"), &Steering2D::separationEnabled, &Steering2D::setSeparationEnabled);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Float, "separationRadius"), &Steering2D::separationRadius, &Steering2D::setSeparationRadius);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Bool, "avoidanceEnabled"), &Steering2D::avoidanceEnabled, &Steering2D::setAvoidanceEnabled);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Float, "lookAhead"), &Steering2D::lookAhead, &Steering2D::setLookAhead);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::String, "groupTag"), &Steering2D::groupTag, &Steering2D::setGroupTag);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Int, "mask"), &Steering2D::mask, &Steering2D::setMask);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Float, "slowRadius"), &Steering2D::slowRadius, &Steering2D::setSlowRadius);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Float, "wanderRadius"), &Steering2D::wanderRadius, &Steering2D::setWanderRadius);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Float, "wanderDistance"), &Steering2D::wanderDistance, &Steering2D::setWanderDistance);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Float, "wanderJitter"), &Steering2D::wanderJitter, &Steering2D::setWanderJitter);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::String, "targetName"), &Steering2D::targetName, &Steering2D::setTargetName);
    ClassDB::addProperty<Steering2D>(c, PropertyInfo(PropertyType::Vec2, "targetPosition"), &Steering2D::targetPosition, &Steering2D::setTargetPosition);
    }
    {
    ClassInfo& c = ClassDB::add("TileMapCollider2D", ComponentType::Collider, ComponentClass::TileMapCollider2D);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Vec2, "offset"), &Collider2D::offset, &Collider2D::setOffset);
    ClassDB::addProperty<Collider2D>(c, PropertyInfo(PropertyType::Bool, "sensor"), &Collider2D::isSensor, &Collider2D::setSensor);
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "category"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).category())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(static_cast<uint16_t>(value.asInt()), collider.mask());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "mask"),
        [](const Component& component) { return PropertyValue::from(static_cast<unsigned int>(static_cast<const Collider2D&>(component).mask())); },
        [](Component& component, const PropertyValue& value)
        {
            Collider2D& collider = static_cast<Collider2D&>(component);
            collider.setFilter(collider.category(), static_cast<uint16_t>(value.asInt()));
        });
    }
    {
    ClassInfo& c = ClassDB::add("TileMapComponent", ComponentType::TileMap, ComponentClass::None);
    ClassDB::addProperty<TileMapComponent>(c, PropertyInfo(PropertyType::Int, "atlasTilesX"), &TileMapComponent::atlasTilesX, &TileMapComponent::setAtlasTilesX);
    ClassDB::addProperty<TileMapComponent>(c, PropertyInfo::enumeration("blendMode", "Alpha,Additive,Multiplied,AddColors,SubtractColors"), &TileMapComponent::blendMode, &TileMapComponent::setBlendMode);
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Vec2, "cellSize"),
        [](const Component& component)
        {
            const TileMapComponent& map = static_cast<const TileMapComponent&>(component);
            return PropertyValue::from(Math::Vec2(map.cellWidth(), map.cellHeight()));
        },
        [](Component& component, const PropertyValue& value)
        {
            const Math::Vec2 size = value.asVec2();
            static_cast<TileMapComponent&>(component).setCellSize(size.x, size.y);
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "columns"),
        [](const Component& component) { return PropertyValue::from(static_cast<const TileMapComponent&>(component).columns()); },
        [](Component& component, const PropertyValue& value)
        {
            TileMapComponent& map = static_cast<TileMapComponent&>(component);
            map.setMapSize(value.asInt(), map.rows());
        });
    ClassDB::addProperty(
        c, PropertyInfo(PropertyType::Int, "rows"),
        [](const Component& component) { return PropertyValue::from(static_cast<const TileMapComponent&>(component).rows()); },
        [](Component& component, const PropertyValue& value)
        {
            TileMapComponent& map = static_cast<TileMapComponent&>(component);
            map.setMapSize(map.columns(), value.asInt());
        });
    }
    {
    ClassInfo& c = ClassDB::add("WheelJoint2D", ComponentType::Joint, ComponentClass::WheelJoint2D);
    ClassDB::addProperty<WheelJoint2D>(c, PropertyInfo(PropertyType::Vec2, "localAnchorA"), &WheelJoint2D::localAnchorA, &WheelJoint2D::setLocalAnchorA);
    ClassDB::addProperty<WheelJoint2D>(c, PropertyInfo(PropertyType::Vec2, "localAnchorB"), &WheelJoint2D::localAnchorB, &WheelJoint2D::setLocalAnchorB);
    ClassDB::addProperty<WheelJoint2D>(c, PropertyInfo(PropertyType::Bool, "anchorsConfigured"), &WheelJoint2D::anchorsConfigured, &WheelJoint2D::setAnchorsConfigured);
    ClassDB::addProperty<WheelJoint2D>(c, PropertyInfo(PropertyType::Vec2, "localAxisA"), &WheelJoint2D::localAxisA, &WheelJoint2D::setLocalAxisA);
    ClassDB::addProperty<Joint2D>(c, PropertyInfo(PropertyType::Bool, "collideConnected"), &Joint2D::collideConnected, &Joint2D::setCollideConnected);
    }
}

} // namespace k2d
