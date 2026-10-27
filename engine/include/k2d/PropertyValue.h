#pragma once

#include "k2d/Color.h"

#include <ct/string.hpp>
#include <mathc.h>

#include <cstdint>
#include <type_traits>

namespace k2d
{

enum class PropertyType : uint8_t
{
    Nil,
    Bool,
    Int,
    Float,
    Vec2,
    Color,
    String,
    ResourcePath
};

class PropertyValue
{
  public:
    PropertyValue();

    static PropertyValue from(bool value);
    static PropertyValue from(int value);
    static PropertyValue from(unsigned int value);
    static PropertyValue from(float value);
    static PropertyValue from(const Math::Vec2& value);
    static PropertyValue from(const k2d::Color& value);
    static PropertyValue from(const ct::String& value);
    static PropertyValue from(const char* value);
    static PropertyValue resourcePath(const ct::String& value);
    template <class E, typename std::enable_if<std::is_enum<E>::value, int>::type = 0>
    static PropertyValue from(E value)
    {
        return from(static_cast<int>(value));
    }

    PropertyType type() const { return mType; }
    bool isNil() const { return mType == PropertyType::Nil; }
    bool asBool() const;
    int asInt() const;
    float asFloat() const;
    Math::Vec2 asVec2() const;
    k2d::Color asColor() const;
    const ct::String& asString() const { return mText; }
    ct::String toString() const;

    template <class T> T as() const;

  private:
    PropertyType mType;
    int64_t mInt;
    float mNumbers[4];
    ct::String mText;
};

template <> inline bool PropertyValue::as<bool>() const { return asBool(); }
template <> inline int PropertyValue::as<int>() const { return asInt(); }
template <> inline unsigned int PropertyValue::as<unsigned int>() const { return static_cast<unsigned int>(asInt()); }
template <> inline float PropertyValue::as<float>() const { return asFloat(); }
template <> inline Math::Vec2 PropertyValue::as<Math::Vec2>() const { return asVec2(); }
template <> inline k2d::Color PropertyValue::as<k2d::Color>() const { return asColor(); }
template <> inline ct::String PropertyValue::as<ct::String>() const { return mText; }

template <class T> inline T PropertyValue::as() const
{
    static_assert(std::is_enum<T>::value, "no conversion from PropertyValue to this type");
    return static_cast<T>(asInt());
}

} // namespace k2d
