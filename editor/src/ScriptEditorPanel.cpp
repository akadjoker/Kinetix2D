#include "ScriptEditorPanel.h"

#include "AssetsPanel.h"
#include "Icons.h"
#include "core/EditorFileSystem.h"

#include "BasicWidgets.hpp"
#include "CodeEditor.hpp"
#include "ComboBox.hpp"
#include "FileDialog.hpp"
#include "WidgetApp.hpp"
#include "ToolBar.hpp"

#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/ZenScriptComponent.h>

#include <platform.h>

#include <cctype>

namespace rt = ig::retained;

namespace k2d::editor
{

ScriptEditorPanel::ScriptEditorPanel() : rt::BoxLayout(rt::LayoutDir::Vertical)
{
    setPadding(0.0f);
    setSpacing(0.0f);

    auto* bar = createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    bar->setPadding(2.0f, 4.0f);
    bar->setSpacing(6.0f);
    auto* tools = bar->createChild<rt::ToolBar>();
    tools->setDraggable(false);
    const int newItem = tools->addButton(Icons::get("New"), "New script");
    const int openItem = tools->addButton(Icons::get("Open"), "Open a script");
    const int saveItem = tools->addButton(Icons::get("Save"), "Save the script (Ctrl+S)");
    const int reloadItem = tools->addButton(Icons::get("Refresh"), "Reload the script from disk");
    const int findItem = tools->addButton(rt::IconId::Search, "Find and replace");
    tools->buttonClicked.connect([this, newItem, openItem, saveItem, reloadItem, findItem](int item)
    {
        if (item == newItem)
            showNewDialog();
        else if (item == openItem)
            showOpenDialog();
        else if (item == saveItem)
            save();
        else if (item == reloadItem)
            reload();
        else if (item == findItem && mCurrent >= 0)
            mEditor->showReplaceBar();
    });
    mFiles = bar->createChild<rt::ComboBox>();
    mFiles->setMaxVisible(20);
    mFiles->selectionChanged.connect([this](int index)
    {
        if (!mLoading && index != mCurrent)
            show(index);
    });
    mTitle = bar->createChild<rt::Label>("Double-click a script in Assets to edit it here");
    mTitle->setStretch(1.0f);
    mTitle->setLayoutAlign(rt::Align::Center);

    mEditor = createChild<rt::CodeEditor>();
    mEditor->setStretch(1.0f);
    mEditor->setShowFolding(true);
    mEditor->setShowIndentGuides(true);
    mEditor->setShowBracketMatch(true);
    mEditor->setHighlightCurrentLine(true);
    mEditor->setShowMinimap(true);
    mEditor->setShowBreakpoints(true);
    mEditor->breakpointToggled.connect([this](int, bool)
    {
        if (mCurrent < 0)
            return;
        mDocuments[mCurrent].breakpoints = mEditor->breakpoints();
        breakpointsChanged.emit(mDocuments[mCurrent].path);
    });
    mEditor->textChanged.connect([this]()
    {
        if (mLoading || mCurrent < 0 || mDocuments[mCurrent].dirty)
            return;
        mDocuments[mCurrent].dirty = true;
        syncTitle();
    });
    mEditor->setEnabled(false);
}

bool ScriptEditorPanel::owns(const rt::Widget* focused) const
{
    for (const rt::Widget* w = focused; w; w = w->parent())
        if (w == this)
            return true;
    return false;
}

bool ScriptEditorPanel::hasUnsaved() const
{
    for (const Document& document : mDocuments)
        if (document.dirty)
            return true;
    return false;
}

ct::String ScriptEditorPanel::unsavedNames() const
{
    ct::String names;
    for (const Document& document : mDocuments)
        if (document.dirty)
        {
            if (!names.empty())
                names += ", ";
            names += EditorFileSystem::fileName(document.path);
        }
    return names;
}

bool ScriptEditorPanel::saveAll()
{
    keepText();
    bool ok = true;
    for (std::size_t i = 0; i < mDocuments.size(); ++i)
    {
        Document& document = mDocuments[i];
        if (!document.dirty)
            continue;
        if (!FileSystem::Instance().SaveTextFile(EditorFileSystem::resolve(mRoot, document.path).c_str(),
                                                 document.text))
        {
            log_error("Could not save %s", document.path.c_str());
            ok = false;
            continue;
        }
        document.dirty = false;
        mFiles->setItemText(static_cast<int>(i), EditorFileSystem::fileName(document.path));
        log_info("Saved %s", document.path.c_str());
        saved.emit(document.path);
    }
    syncTitle();
    return ok;
}

bool ScriptEditorPanel::open(const ct::String& path)
{
    for (std::size_t i = 0; i < mDocuments.size(); ++i)
        if (mDocuments[i].path == path)
        {
            show(static_cast<int>(i));
            return true;
        }
    FileBuffer file;
    if (!FileSystem::Instance().LoadFile(EditorFileSystem::resolve(mRoot, path).c_str(), file, true))
    {
        log_error("Could not open %s", path.c_str());
        return false;
    }
    Document document;
    document.path = path;
    document.text = ct::String(file.Text(), file.Size());
    mDocuments.push_back(document);
    mLoading = true;
    mFiles->addItem(EditorFileSystem::fileName(path));
    mLoading = false;
    show(static_cast<int>(mDocuments.size()) - 1);
    return true;
}

void ScriptEditorPanel::keepText()
{
    if (mCurrent >= 0)
    {
        mDocuments[mCurrent].text = mEditor->text();
        mDocuments[mCurrent].breakpoints = mEditor->breakpoints();
    }
}

ct::Vector<int> ScriptEditorPanel::breakpointLines(const ct::String& path) const
{
    ct::Vector<int> lines;
    for (std::size_t i = 0; i < mDocuments.size(); ++i)
        if (mDocuments[i].path == path)
            for (int line : static_cast<int>(i) == mCurrent ? mEditor->breakpoints() : mDocuments[i].breakpoints)
                lines.push_back(line + 1);
    return lines;
}

ct::Vector<ct::String> ScriptEditorPanel::breakpointFiles() const
{
    ct::Vector<ct::String> files;
    for (std::size_t i = 0; i < mDocuments.size(); ++i)
        if (!(static_cast<int>(i) == mCurrent ? mEditor->breakpoints() : mDocuments[i].breakpoints).empty())
            files.push_back(mDocuments[i].path);
    return files;
}

bool ScriptEditorPanel::setBreakpoint(const ct::String& path, int line, bool on)
{
    if (line < 1 || !open(path) || mCurrent < 0)
        return false;
    mEditor->setBreakpoint(line - 1, on);
    mDocuments[mCurrent].breakpoints = mEditor->breakpoints();
    breakpointsChanged.emit(mDocuments[mCurrent].path);
    return true;
}

void ScriptEditorPanel::showExecution(const ct::String& path, int line)
{
    ct::String project = path;
    const ct::String root = mRoot + "/";
    if (!mRoot.empty() && project.size() > root.size() && project.substr(0, root.size()) == root)
        project = project.substr(root.size());
    mExecutionPath = project;
    mExecutionLine = line - 1;
    if (!open(project))
        return;
    mEditor->setExecutionLine(mExecutionLine);
    mEditor->setCursorPos(mExecutionLine > 0 ? mExecutionLine : 0, 0);
    mEditor->scrollToLine(mExecutionLine > 3 ? mExecutionLine - 3 : 0);
}

void ScriptEditorPanel::clearExecution()
{
    mExecutionPath.clear();
    mExecutionLine = -1;
    mEditor->setExecutionLine(-1);
}

void ScriptEditorPanel::show(int index)
{
    if (index < 0 || index >= static_cast<int>(mDocuments.size()))
        return;
    keepText();
    mCurrent = index;
    mLoading = true;
    mEditor->setEnabled(true);
    // zenpy scripts read as Python.
    mEditor->setHighlighterForFile(EditorFileSystem::withoutExtension(mDocuments[index].path) + ".py");
    mEditor->setText(mDocuments[index].text);
    mEditor->clearUndoHistory();
    mEditor->clearBreakpoints();
    for (int line : mDocuments[index].breakpoints)
        mEditor->setBreakpoint(line, true);
    mEditor->setExecutionLine(mDocuments[index].path == mExecutionPath ? mExecutionLine : -1);
    mFiles->setSelectedIndex(index);
    mLoading = false;
    syncTitle();
}

void ScriptEditorPanel::close(int index)
{
    const bool shown = index == mCurrent;
    mDocuments.erase(mDocuments.begin() + index);
    mLoading = true;
    mFiles->removeItem(index);
    if (!shown)
    {
        if (index < mCurrent)
            --mCurrent;
        mFiles->setSelectedIndex(mCurrent);
        mLoading = false;
        return;
    }
    mLoading = false;
    mCurrent = -1;
    if (!mDocuments.empty())
        return show(index < static_cast<int>(mDocuments.size()) ? index : index - 1);
    mLoading = true;
    mEditor->setText("");
    mEditor->clearBreakpoints();
    mEditor->setExecutionLine(-1);
    mLoading = false;
    mEditor->setEnabled(false);
    mTitle->setText("Double-click a script in Assets to edit it here");
}

void ScriptEditorPanel::assetMoved(const AssetMove& move)
{
    ct::String path;
    for (std::size_t i = 0; i < mDocuments.size(); ++i)
        if (move.apply(mRoot, mDocuments[i].path, path))
        {
            mDocuments[i].path = path;
            mFiles->setItemText(static_cast<int>(i), EditorFileSystem::fileName(path) +
                                                         (mDocuments[i].dirty ? " *" : ""));
        }
    if (move.apply(mRoot, mExecutionPath, path))
        mExecutionPath = path;
    syncTitle();
}

void ScriptEditorPanel::assetDeleted(const AssetMove& asset)
{
    ct::String unused;
    for (int i = static_cast<int>(mDocuments.size()) - 1; i >= 0; --i)
        if (asset.apply(mRoot, mDocuments[static_cast<std::size_t>(i)].path, unused))
            close(i);
}

void ScriptEditorPanel::syncTitle()
{
    if (mCurrent < 0)
        return;
    const Document& document = mDocuments[mCurrent];
    mTitle->setText(document.dirty ? document.path + " *" : document.path);
    mFiles->setItemText(mCurrent, document.dirty ? EditorFileSystem::fileName(document.path) + " *"
                                                 : EditorFileSystem::fileName(document.path));
}

bool ScriptEditorPanel::save()
{
    if (mCurrent < 0)
        return false;
    Document& document = mDocuments[mCurrent];
    document.text = mEditor->text();
    const ct::String absolute = EditorFileSystem::resolve(mRoot, document.path);
    if (!FileSystem::Instance().SaveTextFile(absolute.c_str(), document.text))
    {
        log_error("Could not save %s", document.path.c_str());
        return false;
    }
    document.dirty = false;
    syncTitle();
    log_info("Saved %s", document.path.c_str());
    if (playing)
    {
        const std::size_t reloaded = ReloadChangedZenScripts();
        if (reloaded > 0)
            log_info("Reloaded %d changed script(s)", static_cast<int>(reloaded));
    }
    saved.emit(document.path);
    return true;
}

bool ScriptEditorPanel::reload()
{
    if (mCurrent < 0)
        return false;
    FileBuffer file;
    Document& document = mDocuments[mCurrent];
    if (!FileSystem::Instance().LoadFile(EditorFileSystem::resolve(mRoot, document.path).c_str(), file, true))
        return false;
    document.text = ct::String(file.Text(), file.Size());
    document.dirty = false;
    const int index = mCurrent;
    mCurrent = -1;   // nothing to keep: the disk wins
    show(index);
    return true;
}

bool ScriptEditorPanel::acceptsDrop(const rt::DragPayload& payload)
{
    const ct::String* path = assetPath(payload);
    return path && isScriptPath(*path);
}

void ScriptEditorPanel::onDropReceive(const rt::DragPayload& payload)
{
    if (const ct::String* path = assetPath(payload))
        open(*path);
}

void ScriptEditorPanel::showOpenDialog()
{
    auto* dialog = rt::WidgetApp::instance().addFloat<rt::FileDialog>("Open script");
    dialog->setMode(rt::FileDialog::Mode::Open);
    dialog->setFilter("*.py;*.zen");
    const ct::String scripts = EditorFileSystem::join(mRoot, "assets/scripts");
    dialog->setPath(EditorFileSystem::isDirectory(scripts) ? scripts : mRoot);
    dialog->setRect({160.0f, 80.0f, 900.0f, 540.0f});
    dialog->accepted.connect([this](const rt::String& path) { open(EditorFileSystem::relativeTo(mRoot, path)); });
}

// A script with a class named after the file, ready for on_start and on_update.
void ScriptEditorPanel::showNewDialog()
{
    auto* dialog = rt::WidgetApp::instance().addFloat<rt::FileDialog>("New script");
    dialog->setMode(rt::FileDialog::Mode::Save);
    dialog->setFilter("*.py");
    const ct::String scripts = EditorFileSystem::join(mRoot, "assets/scripts");
    dialog->setPath(EditorFileSystem::isDirectory(scripts) ? scripts : mRoot);
    dialog->setFileName("new_script.py");
    dialog->setRect({160.0f, 80.0f, 900.0f, 540.0f});
    dialog->accepted.connect([this](const rt::String& chosen)
    {
        ct::String path = chosen;
        if (EditorFileSystem::extension(path) != "py")
            path += ".py";
        create(path);
    });
}

bool ScriptEditorPanel::create(const ct::String& path)
{
    if (!EditorFileSystem::exists(path))
    {
        // new_script -> NewScript
        ct::String name;
        bool upper = true;
        for (char c : EditorFileSystem::withoutExtension(EditorFileSystem::fileName(path)))
        {
            if (!std::isalnum(static_cast<unsigned char>(c)))
            {
                upper = true;
                continue;
            }
            name += upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
            upper = false;
        }
        if (name.empty() || std::isdigit(static_cast<unsigned char>(name[0])))
            name = ct::String("Script") + name;
        const ct::String source = ct::String("class ") + name +
                                  "(ScriptComponent):\n"
                                  "    speed = 100\n"
                                  "\n"
                                  "    def on_start(self):\n"
                                  "        pass\n"
                                  "\n"
                                  "    def on_update(self, dt):\n"
                                  "        pass\n";
        if (!FileSystem::Instance().SaveTextFile(path.c_str(), source))
        {
            log_error("Could not create %s", path.c_str());
            return false;
        }
        log_info("Created %s", path.c_str());
    }
    return open(EditorFileSystem::relativeTo(mRoot, path));
}

} // namespace k2d::editor
