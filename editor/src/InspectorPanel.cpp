#include "InspectorPanel.h"

#include <k2d/ClassDB.h>
#include <k2d/GameObject.h>

#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

InspectorPanel::InspectorPanel() : rt::BoxLayout(rt::LayoutDir::Vertical)
{
    setPadding(8.0f);
    setSpacing(2.0f);
}

rt::Label* InspectorPanel::line(std::size_t index)
{
    while (mLines.size() <= index)
        mLines.push_back(createChild<rt::Label>(""));
    rt::Label* label = mLines[index];
    label->setVisible(true);
    return label;
}

void InspectorPanel::show(GameObject* object)
{
    mUsed = 0;
    if (!object)
        line(mUsed++)->setText("Nothing selected");
    else
    {
        char text[256];
        line(mUsed++)->setText(object->name().c_str());
        const Math::Vec2 position = object->position();
        std::snprintf(text, sizeof text, "position  %g, %g", position.x, position.y);
        line(mUsed++)->setText(text);
        std::snprintf(text, sizeof text, "rotation  %g", object->rotationDegrees());
        line(mUsed++)->setText(text);

        for (std::size_t type = 0; type < static_cast<std::size_t>(ComponentType::Count); ++type)
        {
            const ComponentType componentType = static_cast<ComponentType>(type);
            const std::size_t count = object->rawComponentCount(componentType);
            for (std::size_t index = 0; index < count; ++index)
            {
                const Component* component = object->rawComponent(componentType, index);
                const ClassInfo* info = component ? ClassDB::find(*component) : nullptr;
                if (!info)
                    continue;
                line(mUsed++)->setText("");
                line(mUsed++)->setText(info->name.c_str());
                for (std::size_t p = 0; p < info->properties.size(); ++p)
                {
                    const ClassProperty& property = info->properties[p];
                    const PropertyValue value = property.get(*component);
                    ct::String shown = value.toString();
                    if (property.info.hint == PropertyHint::Enum)
                        shown = property.info.enumName(static_cast<std::size_t>(value.asInt()));
                    std::snprintf(text, sizeof text, "  %s  %s", property.info.name.c_str(), shown.c_str());
                    line(mUsed++)->setText(text);
                }
            }
        }
    }
    for (std::size_t i = mUsed; i < mLines.size(); ++i)
        mLines[i]->setVisible(false);
    layout();
}

} // namespace k2d::editor
