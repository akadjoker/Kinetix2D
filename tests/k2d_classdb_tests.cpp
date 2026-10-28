#include <k2d/Assets.h>
#include <k2d/ClassDB.h>
#include <k2d/ParticleComponent.h>
#include <k2d/SpriteComponent.h>
#include <k2d/GameObject.h>
#include <k2d/Light2D.h>
#include <k2d/Scene.h>

#include <cmath>
#include <cstdio>

namespace
{
bool near(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

bool testLightClass()
{
    k2d::RegisterCoreClasses();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("lamp");
    k2d::Light2D* light = object->addComponent<k2d::Light2D>();

    const k2d::ClassInfo* info = k2d::ClassDB::find(*light);
    bool ok = info && info->name == "Light2D" && info->properties.size() == 8;
    ok = ok && k2d::ClassDB::find("Light2D") == info;

    k2d::PropertyValue value;
    ok = ok && k2d::ClassDB::get(*light, "energy", value) && value.type() == k2d::PropertyType::Float;
    ok = ok && k2d::ClassDB::set(*light, "energy", k2d::PropertyValue::from(2.5f)) && near(light->energy(), 2.5f);
    ok = ok && k2d::ClassDB::set(*light, "castShadow", k2d::PropertyValue::from(true)) && light->castShadow();
    ok = ok && k2d::ClassDB::set(*light, "shadowFilter", k2d::PropertyValue::from(2)) &&
         light->shadowFilter() == k2d::SHADOW_FILTER_PCF13;
    ok = ok && k2d::ClassDB::set(*light, "color", k2d::PropertyValue::from(k2d::Color(0.5f, 0.25f, 1.0f, 0.75f))) &&
         near(light->color().g, 0.25f) && near(light->color().a, 0.75f);
    ok = ok && k2d::ClassDB::get(*light, "color", value) && near(value.asColor().b, 1.0f);
    ok = ok && !k2d::ClassDB::set(*light, "missing", value);

    const k2d::ClassProperty* filter = info ? info->find("shadowFilter") : nullptr;
    ok = ok && filter && filter->info.enumCount() == 3 && filter->info.enumName(1) == "PCF5" &&
         filter->info.enumName(2) == "PCF13";
    ok = ok && value.toString() == "0.5, 0.25, 1, 0.75";
    return ok;
}
} // namespace

// Texture properties show the path the Assets know the texture by. No GL here,
// so no texture is made: an empty path clears, and a path that cannot be
// loaded, or a write with no Assets set, leaves the sprite as it was.
bool testTextureProperty()
{
    k2d::RegisterCoreClasses();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("sprite");
    k2d::SpriteComponent* sprite = object->addComponent<k2d::SpriteComponent>();
    const k2d::ClassInfo* info = k2d::ClassDB::find(*sprite);
    const k2d::ClassProperty* texture = info ? info->find("texture") : nullptr;
    bool ok = texture && texture->info.type == k2d::PropertyType::ResourcePath &&
              texture->info.hint == k2d::PropertyHint::File && info->find("normalMap");

    k2d::PropertyValue value;
    k2d::ClassDB::setAssets(nullptr);
    ok = ok && k2d::ClassDB::get(*sprite, "texture", value) && value.type() == k2d::PropertyType::ResourcePath &&
         value.asString().empty();
    ok = ok && k2d::ClassDB::set(*sprite, "texture", k2d::PropertyValue::resourcePath("a.png")) && !sprite->texture();

    k2d::Assets assets;
    k2d::ClassDB::setAssets(&assets);
    ok = ok && k2d::ClassDB::set(*sprite, "texture", k2d::PropertyValue::resourcePath("missing/none.png")) &&
         !sprite->texture();
    ok = ok && k2d::ClassDB::set(*sprite, "texture", k2d::PropertyValue::resourcePath("")) && !sprite->texture();
    k2d::ClassDB::setAssets(nullptr);
    return ok;
}

// The emitter's settings and the prefab's fields are properties, under the
// names the scene file gives them: a write reaches the system, enums go by
// index, and a prefab field changes that field alone.
bool testParticleProperties()
{
    k2d::RegisterCoreClasses();
    k2d::Scene scene;
    k2d::GameObject* object = scene.createObject("sparks");
    k2d::ParticleComponent* particles = object->addComponent<k2d::ParticleComponent>();
    k2d::ParticleSystem& system = particles->system();
    const k2d::ClassInfo* info = k2d::ClassDB::find(*particles);
    bool ok = info && info->find("speedMin") && info->find("colorEnd") && info->find("emissionRate") &&
              info->find("mode") && info->find("mode")->info.enumCount() == 3;

    const float lifeMax = system.GetPrefab().lifeMax;
    ok = ok && k2d::ClassDB::set(*particles, "speedMin", k2d::PropertyValue::from(42.0f)) &&
         near(system.GetPrefab().speedMin, 42.0f) && near(system.GetPrefab().lifeMax, lifeMax);
    ok = ok && k2d::ClassDB::set(*particles, "colorStart", k2d::PropertyValue::from(k2d::Color(1.0f, 0.5f, 0.0f, 1.0f))) &&
         near(system.GetPrefab().colorStart.g, 0.5f);
    ok = ok && k2d::ClassDB::set(*particles, "faceDirection", k2d::PropertyValue::from(true)) &&
         system.GetPrefab().faceDirection;
    ok = ok && k2d::ClassDB::set(*particles, "mode", k2d::PropertyValue::from(2)) &&
         system.GetMode() == k2d::ParticleMode::Loop;
    ok = ok && k2d::ClassDB::set(*particles, "emissionRate", k2d::PropertyValue::from(30.0f)) &&
         near(system.EmissionRate(), 30.0f);
    ok = ok && k2d::ClassDB::set(*particles, "emitterSize", k2d::PropertyValue::from(Math::Vec2(8.0f, 4.0f))) &&
         near(system.EmitterSize().y, 4.0f);
    k2d::PropertyValue value;
    ok = ok && k2d::ClassDB::get(*particles, "gravity", value) && value.type() == k2d::PropertyType::Vec2;
    ok = ok && k2d::ClassDB::get(*particles, "speedMin", value) && near(value.asFloat(), 42.0f);
    return ok;
}

int main()
{
    const bool light = testLightClass();
    const bool texture = testTextureProperty();
    const bool particles = testParticleProperties();
    std::printf("classdb: light=%s texture=%s particles=%s\n", light ? "pass" : "fail", texture ? "pass" : "fail",
                particles ? "pass" : "fail");
    return light && texture && particles ? 0 : 1;
}
