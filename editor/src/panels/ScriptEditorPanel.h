#pragma once

#include "core/EditorPanel.h"

#include <ct/string.hpp>

#include <TextEditor.h>

namespace k2d::editor
{

class ScriptEditorPanel final : public EditorPanel
{
public:
    explicit ScriptEditorPanel(EditorApplication &application);

    void openFile(const char *path);

private:
    bool loadFile(const char *path);
    bool saveFile();
    bool dirty() const;
    void drawContents() override;

    ct::String mPath;
    ct::String mPendingPath;
    TextEditor mEditor;
    int mSavedUndoIndex = 0;
    bool mConfirmOpen = false;
    bool mShowMinimap = true;
};

}
