#pragma once
#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"
#include <ct/vector.hpp>
#include <mathc.h>
namespace k2d
{
class MotionStreak2D final : public Component
{
  public:
    static const ComponentType Type = ComponentType::MotionStreak;
    MotionStreak2D();
    void reset();
    void setLifetime(float value);
    float lifetime() const
    {
        return mLifetime;
    }
    void setWidth(float value);
    float width() const
    {
        return mWidth;
    }
    void setMinDistance(float value);
    float minDistance() const
    {
        return mMinDistance;
    }
    void setColor(const Color& value)
    {
        mColor = value;
    }
    const Color& color() const
    {
        return mColor;
    }
    void setBlendMode(BlendMode value)
    {
        mBlend = value;
    }
    BlendMode blendMode() const
    {
        return mBlend;
    }

  protected:
    void onUpdate(float deltaTime) override;
    void onRender(RenderQueue& queue) override;

  private:
    struct Point
    {
        Math::Vec2 position;
        float age;
    };
    ct::Vector<Point> mPoints;
    Math::Vec2 mLastPosition{0.0f};
    float mLifetime = 0.6f, mWidth = 16.0f, mMinDistance = 8.0f;
    Color mColor = Color::White();
    BlendMode mBlend = BLEND_ADD;
    bool mInitialized = false;
};
} // namespace k2d
