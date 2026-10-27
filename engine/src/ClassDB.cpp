#include "k2d/ClassDB.h"

#include <cstring>

namespace k2d
{

namespace
{
ct::Vector<ClassInfo*>& classes()
{
    static ct::Vector<ClassInfo*> list;
    return list;
}
} // namespace

PropertyInfo::PropertyInfo(PropertyType type, const char* name, PropertyHint hint, const char* hintString)
    : type(type), name(name), hint(hint), hintString(hintString)
{
}

PropertyInfo PropertyInfo::range(PropertyType type, const char* name, float minimum, float maximum, float step)
{
    PropertyInfo info(type, name, PropertyHint::Range);
    info.rangeMin = minimum;
    info.rangeMax = maximum;
    info.rangeStep = step;
    return info;
}

PropertyInfo PropertyInfo::enumeration(const char* name, const char* names)
{
    return PropertyInfo(PropertyType::Int, name, PropertyHint::Enum, names);
}

PropertyInfo PropertyInfo::file(const char* name, const char* extensions)
{
    return PropertyInfo(PropertyType::ResourcePath, name, PropertyHint::File, extensions);
}

PropertyInfo PropertyInfo::color(const char* name, bool noAlpha)
{
    return PropertyInfo(PropertyType::Color, name, noAlpha ? PropertyHint::ColorNoAlpha : PropertyHint::None);
}

std::size_t PropertyInfo::enumCount() const
{
    if (hint != PropertyHint::Enum || hintString.empty())
        return 0;
    std::size_t count = 1;
    for (std::size_t i = 0; i < hintString.size(); ++i)
        if (hintString[i] == ',')
            ++count;
    return count;
}

ct::String PropertyInfo::enumName(std::size_t index) const
{
    std::size_t begin = 0;
    for (std::size_t i = 0; i < index; ++i)
    {
        const std::size_t comma = hintString.find(',', begin);
        if (comma == ct::String::npos)
            return ct::String();
        begin = comma + 1;
    }
    const std::size_t end = hintString.find(',', begin);
    return hintString.substr(begin, end == ct::String::npos ? ct::String::npos : end - begin);
}

const ClassProperty* ClassInfo::find(const char* property) const
{
    for (std::size_t i = 0; i < properties.size(); ++i)
        if (properties[i].info.name == property)
            return &properties[i];
    return nullptr;
}

ClassInfo& ClassDB::add(const char* name, ComponentType type, ComponentClass componentClass)
{
    for (std::size_t i = 0; i < classes().size(); ++i)
        if (classes()[i]->name == name)
            return *classes()[i];
    ClassInfo* info = new ClassInfo();
    info->name = name;
    info->type = type;
    info->componentClass = componentClass;
    classes().push_back(info);
    return *info;
}

void ClassDB::addProperty(ClassInfo& info, const PropertyInfo& property, PropertyGetter get, PropertySetter set)
{
    ClassProperty entry;
    entry.info = property;
    entry.get = ct::detail::move(get);
    entry.set = ct::detail::move(set);
    info.properties.push_back(ct::detail::move(entry));
}

const ClassInfo* ClassDB::find(const Component& component)
{
    const ClassInfo* byType = nullptr;
    for (std::size_t i = 0; i < classes().size(); ++i)
    {
        const ClassInfo* info = classes()[i];
        if (info->type != component.type())
            continue;
        if (info->componentClass == component.componentClass())
            return info;
        if (info->componentClass == ComponentClass::None)
            byType = info;
    }
    return byType;
}

const ClassInfo* ClassDB::find(const char* name)
{
    for (std::size_t i = 0; i < classes().size(); ++i)
        if (classes()[i]->name == name)
            return classes()[i];
    return nullptr;
}

std::size_t ClassDB::count()
{
    return classes().size();
}

const ClassInfo& ClassDB::at(std::size_t index)
{
    return *classes()[index];
}

bool ClassDB::get(const Component& component, const char* property, PropertyValue& out)
{
    const ClassInfo* info = find(component);
    const ClassProperty* entry = info ? info->find(property) : nullptr;
    if (!entry || !entry->get)
        return false;
    out = entry->get(component);
    return true;
}

bool ClassDB::set(Component& component, const char* property, const PropertyValue& value)
{
    const ClassInfo* info = find(component);
    const ClassProperty* entry = info ? info->find(property) : nullptr;
    if (!entry || !entry->set)
        return false;
    entry->set(component, value);
    return true;
}

void ClassDB::clear()
{
    for (std::size_t i = 0; i < classes().size(); ++i)
        delete classes()[i];
    classes().clear();
}

} // namespace k2d
