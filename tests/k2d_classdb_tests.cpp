#include <k2d/ClassDB.h>
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

int main()
{
    const bool light = testLightClass();
    std::printf("classdb: light=%s\n", light ? "pass" : "fail");
    return light ? 0 : 1;
}
