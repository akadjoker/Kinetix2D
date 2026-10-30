#pragma once

#include "LayoutWidgets.hpp"
#include "core/AssetPaths.h"

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
    // A script written to its file (its project path).
    ig::retained::Signal<const ct::String&> saved;
    // Whether the widget with the focus is this panel's.
    bool owns(const ig::retained::Widget* focused) const;
    void showOpenDialog();
    void showNewDialog();
    // A new script at an absolute path, from the template, opened; an existing
    // one is only opened.
    bool create(const ct::String& path);

    bool acceptsDrop(const ig::retained::DragPayload& payload) override;
    void onDropReceive(const ig::retained::DragPayload& payload) override;
    bool hasUnsaved() const;

    // Breakpoints, toggled in the gutter, kept per open script; lines from 1,
    // as the VM counts them.
    ct::Vector<int> breakpointLines(const ct::String& path) const;
    ct::Vector<ct::String> breakpointFiles() const;
    // Opens the script and sets or clears the breakpoint on line (from 1).
    bool setBreakpoint(const ct::String& path, int line, bool on);
    ig::retained::Signal<const ct::String&> breakpointsChanged;
    // Where the debugger stopped: the script opened there, the line marked.
    void showExecution(const ct::String& path, int line);
    void clearExecution();
    // The file names of the unsaved scripts, comma separated.
    ct::String unsavedNames() const;
    bool saveAll();
    // Files moved or deleted in Assets: the open scripts follow them, or close.
    void assetMoved(const AssetMove& move);
    void assetDeleted(const AssetMove& asset);

    bool playing = false;

private:
    struct Document
    {
        ct::String path;   // project path
        ct::String text;   // as last shown, for the documents not on screen
        bool dirty = false;
        ct::Vector<int> breakpoints;   // lines from 0, as the editor counts them
    };

    void show(int index);
    void close(int index);
    void keepText();
    void syncTitle();

    ct::String mRoot;
    ct::Vector<Document> mDocuments;
    int mCurrent = -1;
    ct::String mExecutionPath;
    int mExecutionLine = -1;
    bool mLoading = false;

    ig::retained::ComboBox* mFiles = nullptr;
    ig::retained::Label* mTitle = nullptr;
    ig::retained::CodeEditor* mEditor = nullptr;
};

} // namespace k2d::editor
