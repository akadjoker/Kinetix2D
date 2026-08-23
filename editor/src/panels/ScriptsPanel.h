#pragma once

#include "core/EditorPanel.h"

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{
class GameObject;
}

namespace k2d::editor
{

class ScriptsPanel final : public EditorPanel
{
public:
    explicit ScriptsPanel(EditorApplication &application);

private:
    struct EventLogEntry
    {
        ct::String name;
        double value = 0.0;
        int count = 1;
    };

    void drawContents() override;
    void drawBlackboard();
    void drawEventLog();
    void drawScriptList(GameObject &object, int depth);

    static void onHostEvent(const char *name, double value, void *user);

    ct::Vector<EventLogEntry> mEvents;
    char mKeyBuffer[64] = "";
    char mValueBuffer[128] = "";
    int mValueKind = 0;
    bool mHandlerInstalled = false;
};

}
