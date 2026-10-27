#pragma once

#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"

#include <ct/vector.hpp>

namespace k2d
{
class GameObject;
}

namespace k2d::editor
{

class InspectorPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(InspectorPanel, ig::retained::BoxLayout);

    InspectorPanel();

    void show(GameObject* object);

private:
    ig::retained::Label* line(std::size_t index);

    ct::Vector<ig::retained::Label*> mLines;
    std::size_t mUsed = 0;
};

} // namespace k2d::editor
