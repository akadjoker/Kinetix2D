#pragma once

#include "k2d/Component.h"
#include "k2d/PropertyValue.h"

#include <ct/detail/utils.hpp>
#include <ct/function.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{

enum class PropertyHint : uint8_t
{
    None,
    Range,
    Enum,
    File,
    ColorNoAlpha
};

struct PropertyInfo
{
    PropertyType type = PropertyType::Nil;
    ct::String name;
    PropertyHint hint = PropertyHint::None;
    ct::String hintString;
    float rangeMin = 0.0f;
    float rangeMax = 0.0f;
    float rangeStep = 0.0f;

    PropertyInfo() = default;
    PropertyInfo(PropertyType type, const char* name, PropertyHint hint = PropertyHint::None,
                 const char* hintString = "");

    static PropertyInfo range(PropertyType type, const char* name, float minimum, float maximum, float step);
    static PropertyInfo enumeration(const char* name, const char* names);
    static PropertyInfo file(const char* name, const char* extensions);
    static PropertyInfo color(const char* name, bool noAlpha = false);

    std::size_t enumCount() const;
    ct::String enumName(std::size_t index) const;
};

using PropertyGetter = ct::Function<PropertyValue(const Component&)>;
using PropertySetter = ct::Function<void(Component&, const PropertyValue&)>;

struct ClassProperty
{
    PropertyInfo info;
    PropertyGetter get;
    PropertySetter set;
};

struct ClassInfo
{
    ct::String name;
    ComponentType type = ComponentType::Count;
    ComponentClass componentClass = ComponentClass::None;
    ct::Vector<ClassProperty> properties;

    const ClassProperty* find(const char* property) const;
};

class ClassDB
{
  public:
    static ClassInfo& add(const char* name, ComponentType type, ComponentClass componentClass = ComponentClass::None);
    static void addProperty(ClassInfo& info, const PropertyInfo& property, PropertyGetter get, PropertySetter set);

    template <class T, class R, class A>
    static void addProperty(ClassInfo& info, const PropertyInfo& property, R (T::*get)() const, void (T::*set)(A))
    {
        using Value = typename ct::detail::remove_cv<typename ct::detail::remove_ref<A>::type>::type;
        addProperty(info, property,
                    [get](const Component& component) { return PropertyValue::from((static_cast<const T&>(component).*get)()); },
                    [set](Component& component, const PropertyValue& value) { (static_cast<T&>(component).*set)(value.as<Value>()); });
    }

    static const ClassInfo* find(const Component& component);
    static const ClassInfo* find(const char* name);
    static std::size_t count();
    static const ClassInfo& at(std::size_t index);
    static bool get(const Component& component, const char* property, PropertyValue& out);
    static bool set(Component& component, const char* property, const PropertyValue& value);
    static void clear();
};

void RegisterCoreClasses();

} // namespace k2d
