#pragma once

#include "LayoutWidgets.hpp"

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace ig::retained
{
class CodeEditor;
class ComboBox;
class Label;
class Widget;
}

namespace k2d::editor
{

// Script files open side by side in one code editor: picked from a list, saved
// with Ctrl+S while the editor has the focus. Saving while the game plays
// reloads the scripts that changed.
class ScriptEditorPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(ScriptEditorPanel, ig::retained::BoxLayout);

    ScriptEditorPanel();

    void setRoot(const ct::String& root) { mRoot = root; }
    // Opens a script by its project path, or shows it when it is open already.
    bool open(const ct::String& path);
    bool save();
    bool reload();
    // Whether the widget with the focus is this panel's.
    bool owns(const ig::retained::Widget* focused) const;
    bool hasUnsaved() const;

    bool playing = false;

private:
    struct Document
    {
        ct::String path;   // project path
        ct::String text;   // as last shown, for the documents not on screen
        bool dirty = false;
    };

    void show(int index);
    void keepText();
    void syncTitle();

    ct::String mRoot;
    ct::Vector<Document> mDocuments;
    int mCurrent = -1;
    bool mLoading = false;

    ig::retained::ComboBox* mFiles = nullptr;
    ig::retained::Label* mTitle = nullptr;
    ig::retained::CodeEditor* mEditor = nullptr;
};

} // namespace k2d::editor
