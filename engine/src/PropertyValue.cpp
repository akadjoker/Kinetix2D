#include "k2d/PropertyValue.h"

#include <cstdio>

namespace k2d
{

PropertyValue::PropertyValue() : mType(PropertyType::Nil), mInt(0), mText()
{
    mNumbers[0] = mNumbers[1] = mNumbers[2] = mNumbers[3] = 0.0f;
}

PropertyValue PropertyValue::from(bool value)
{
    PropertyValue result;
    result.mType = PropertyType::Bool;
    result.mInt = value ? 1 : 0;
    return result;
}

PropertyValue PropertyValue::from(int value)
{
    PropertyValue result;
    result.mType = PropertyType::Int;
    result.mInt = value;
    return result;
}

PropertyValue PropertyValue::from(unsigned int value)
{
    return from(static_cast<int>(value));
}

PropertyValue PropertyValue::from(float value)
{
    PropertyValue result;
    result.mType = PropertyType::Float;
    result.mNumbers[0] = value;
    return result;
}

PropertyValue PropertyValue::from(const Math::Vec2& value)
{
    PropertyValue result;
    result.mType = PropertyType::Vec2;
    result.mNumbers[0] = value.x;
    result.mNumbers[1] = value.y;
    return result;
}

PropertyValue PropertyValue::from(const k2d::Color& value)
{
    PropertyValue result;
    result.mType = PropertyType::Color;
    result.mNumbers[0] = value.r;
    result.mNumbers[1] = value.g;
    result.mNumbers[2] = value.b;
    result.mNumbers[3] = value.a;
    return result;
}

PropertyValue PropertyValue::from(const ct::String& value)
{
    PropertyValue result;
    result.mType = PropertyType::String;
    result.mText = value;
    return result;
}

PropertyValue PropertyValue::from(const char* value)
{
    return from(ct::String(value ? value : ""));
}

PropertyValue PropertyValue::resourcePath(const ct::String& value)
{
    PropertyValue result = from(value);
    result.mType = PropertyType::ResourcePath;
    return result;
}

bool PropertyValue::asBool() const
{
    switch (mType)
    {
    case PropertyType::Bool:
    case PropertyType::Int:
        return mInt != 0;
    case PropertyType::Float:
        return mNumbers[0] != 0.0f;
    default:
        return false;
    }
}

int PropertyValue::asInt() const
{
    switch (mType)
    {
    case PropertyType::Bool:
    case PropertyType::Int:
        return static_cast<int>(mInt);
    case PropertyType::Float:
        return static_cast<int>(mNumbers[0]);
    default:
        return 0;
    }
}

float PropertyValue::asFloat() const
{
    switch (mType)
    {
    case PropertyType::Bool:
    case PropertyType::Int:
        return static_cast<float>(mInt);
    case PropertyType::Float:
        return mNumbers[0];
    default:
        return 0.0f;
    }
}

Math::Vec2 PropertyValue::asVec2() const
{
    return mType == PropertyType::Vec2 ? Math::Vec2(mNumbers[0], mNumbers[1]) : Math::Vec2(0.0f, 0.0f);
}

k2d::Color PropertyValue::asColor() const
{
    return mType == PropertyType::Color ? k2d::Color(mNumbers[0], mNumbers[1], mNumbers[2], mNumbers[3])
                                        : k2d::Color();
}

ct::String PropertyValue::toString() const
{
    char buffer[96];
    switch (mType)
    {
    case PropertyType::Bool:
        return ct::String(mInt ? "true" : "false");
    case PropertyType::Int:
        std::snprintf(buffer, sizeof buffer, "%lld", static_cast<long long>(mInt));
        return ct::String(buffer);
    case PropertyType::Float:
        std::snprintf(buffer, sizeof buffer, "%g", mNumbers[0]);
        return ct::String(buffer);
    case PropertyType::Vec2:
        std::snprintf(buffer, sizeof buffer, "%g, %g", mNumbers[0], mNumbers[1]);
        return ct::String(buffer);
    case PropertyType::Color:
        std::snprintf(buffer, sizeof buffer, "%g, %g, %g, %g", mNumbers[0], mNumbers[1], mNumbers[2], mNumbers[3]);
        return ct::String(buffer);
    case PropertyType::String:
    case PropertyType::ResourcePath:
        return mText;
    default:
        return ct::String();
    }
}

} // namespace k2d
