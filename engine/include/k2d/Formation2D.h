#pragma once

#include "k2d/Component.h"

#include <ct/string.hpp>
#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{

class GameObject;

// Port of the Game Institute AI demo's cFormationBehavior. A formation is not
// a force: it works out where this member belongs relative to the thing the
// group forms around, and hands that position to the NavigationAgent2D as its
// goal. Steering forces fight the path; a goal is the path.
class Formation2D final : public Component
{
  public:
    static const ComponentType Type = ComponentType::Formation;

    enum class Shape : unsigned char
    {
        Surround,
        Abreast,
        Wedge,
        SingleFile
    };

    Formation2D();

    // Members are every object carrying this tag and a Formation2D of its own.
    // Slots are handed out in id order, so nobody has to be numbered by hand
    // and two members never claim the same place.
    const ct::String& groupTag() const
    {
        return mGroupTag;
    }
    void setGroupTag(const char* tag);

    const ct::String& anchorName() const
    {
        return mAnchorName;
    }
    void setAnchorName(const char* name);

    Shape shape() const
    {
        return mShape;
    }
    void setShape(Shape shape)
    {
        mShape = shape;
    }

    float spacing() const
    {
        return mSpacing;
    }
    void setSpacing(float spacing);

    float updateInterval() const
    {
        return mUpdateInterval;
    }
    void setUpdateInterval(float seconds);

    int slot() const
    {
        return mSlot;
    }
    int memberCount() const
    {
        return mMemberCount;
    }
    // Where this member is trying to stand. Valid once the group has resolved.
    const Math::Vec2& goal() const
    {
        return mGoal;
    }

  protected:
    void onAwake() override;
    void onUpdate(float deltaTime) override;

  private:
    void resolveGroup();
    bool computeGoal(Math::Vec2& out) const;

    ct::String mGroupTag;
    ct::String mAnchorName;
    GameObject* mAnchor = nullptr;
    Math::Vec2 mGoal{0.0f, 0.0f};
    float mSpacing = 60.0f;
    float mUpdateInterval = 0.25f;
    float mTimer = 0.0f;
    uint32_t mGroupVersion = 0xFFFFFFFFu;
    bool mHolding = false;
    int mSlot = 0;
    int mMemberCount = 1;
    Shape mShape = Shape::Surround;
};

template <> struct ComponentMatch<Formation2D>
{
    static bool test(const Component* component)
    {
        return dynamic_cast<const Formation2D*>(component) != nullptr;
    }
};

} // namespace k2d
