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
        mDocuments[mCurrent].text = mEditor->text();
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
    mFiles->setSelectedIndex(index);
    mLoading = false;
    syncTitle();
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
                return;
            }
            log_info("Created %s", path.c_str());
        }
        open(EditorFileSystem::relativeTo(mRoot, path));
    });
}

} // namespace k2d::editor
