#include "AssetsPanel.h"

#include "Icons.h"
#include "core/EditorFileSystem.h"

#include "DialogWidgets.hpp"
#include "MenuWidgets.hpp"
#include "WidgetApp.hpp"

#include <k2d/Assets.h>
#include <k2d/Texture.h>

#include <ct/sort.hpp>

#include <platform.h>

#include <cstring>

namespace rt = ig::retained;

namespace k2d::editor
{

const char* const kAssetPayload = "k2d-asset";
const char* const kAssetFolderPayload = "k2d-asset-folder";

namespace
{

bool hasExtension(const ct::String& path, const char* const* extensions)
{
    const ct::String ext = EditorFileSystem::extension(path);
    for (const char* const* e = extensions; *e; ++e)
        if (std::strcmp(ext.c_str(), *e) == 0)
            return true;
    return false;
}

// As EditorFileSystem::extension gives them: no dot, lower case.
const char* const kImages[] = {"png", "jpg", "jpeg", "bmp", "tga", nullptr};
const char* const kAudio[] = {"wav", "ogg", "mp3", "flac", nullptr};
const char* const kScripts[] = {"py", "zen", "zs", nullptr};
const char* const kScenes[] = {"k2dscene", nullptr};
const char* const kPrefabs[] = {"k2dprefab", "prefab", nullptr};

} // namespace

bool isImagePath(const ct::String& path)
{
    return hasExtension(path, kImages);
}

bool isScriptPath(const ct::String& path)
{
    return hasExtension(path, kScripts);
}

bool isPrefabPath(const ct::String& path)
{
    return hasExtension(path, kPrefabs);
}

const ct::String* assetPath(const rt::DragPayload& payload)
{
    return payload.type == kAssetPayload ? std::any_cast<ct::String>(&payload.data) : nullptr;
}

namespace
{

rt::IconId iconFor(const ct::String& path, bool directory)
{
    if (directory)
        return Icons::get("Folder");
    if (hasExtension(path, kAudio))
        return Icons::get("AudioFile");
    if (hasExtension(path, kScripts))
        return Icons::get("ScriptFile");
    if (hasExtension(path, kScenes))
        return Icons::get("SceneFile");
    if (hasExtension(path, kPrefabs))
        return Icons::get("Prefab");
    return rt::IconId::File;
}

rt::Menu* assetsMenu()
{
    static rt::Menu* menu = new rt::Menu();
    return menu;
}

bool startsWith(const ct::String& text, const ct::String& prefix)
{
    return text.size() >= prefix.size() && std::memcmp(text.c_str(), prefix.c_str(), prefix.size()) == 0;
}

} // namespace

AssetsPanel::AssetsPanel(Assets& assets) : rt::BoxLayout(rt::LayoutDir::Vertical), mAssets(assets)
{
    setPadding(0.0f);
    setSpacing(0.0f);

    auto* bar = createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    bar->setPadding(0.0f);
    bar->setSpacing(6.0f);
    mTools = bar->createChild<rt::ToolBar>();
    mTools->setDraggable(false);
    const int up = mTools->addButton(rt::IconId::ArrowUp, "Up one folder");
    const int reload = mTools->addButton(Icons::get("Refresh"), "Refresh");
    mTools->buttonClicked.connect([this, up, reload](int item)
    {
        if (item == up && mDirectory != mRoot)
            open(EditorFileSystem::parentPath(mDirectory));
        else if (item == reload)
            refresh();
    });
    mPath = bar->createChild<rt::Label>("");
    mPath->setStretch(1.0f);
    mPath->setLayoutAlign(rt::Align::Center);

    mGrid = createChild<rt::ThumbnailGrid>();
    mGrid->setStretch(1.0f);
    mGrid->setThumbSize(72.0f, 72.0f);
    mGrid->onItemDoubleClicked.connect([this](int index)
    {
        const Entry* found = entryAt(index);
        if (!found)
            return;
        // Copied: opening a folder replaces the entries.
        const Entry entry = *found;
        if (entry.directory)
            open(entry.path);
        else
            fileOpened.emit(relative(entry.path));
    });
    mGrid->setDragPayload([this](int index)
    {
        rt::DragPayload payload;
        const Entry* entry = entryAt(index);
        if (!entry || entry->up)
            return payload;
        payload.type = entry->directory ? kAssetFolderPayload : kAssetPayload;
        payload.data = relative(entry->path);
        return payload;
    });
    mGrid->setDropFilter([this](const rt::DragPayload& payload, int index)
    {
        const Entry* target = entryAt(index);
        const ct::String* from = payload.type == kAssetPayload || payload.type == kAssetFolderPayload
                                     ? std::any_cast<ct::String>(&payload.data)
                                     : nullptr;
        if (!target || !target->directory || !from)
            return false;
        const ct::String folder = relative(target->path);
        if (payload.type == kAssetFolderPayload && (folder == *from || startsWith(folder, *from + "/")))
            return false;
        return EditorFileSystem::parentPath(EditorFileSystem::resolve(mRoot, *from)) != target->path;
    });
    mGrid->itemDropped.connect([this](const rt::DragPayload& payload, int index)
    {
        const Entry* target = entryAt(index);
        if (const ct::String* from = std::any_cast<ct::String>(&payload.data))
            if (target)
                moveInto(*from, relative(target->path));
    });
    mGrid->onItemContextMenu.connect([this](int index, float x, float y) { showMenu(index, x, y); });
}

const AssetsPanel::Entry* AssetsPanel::entryAt(int index) const
{
    return index >= 0 && index < static_cast<int>(mEntries.size()) ? &mEntries[static_cast<std::size_t>(index)]
                                                                   : nullptr;
}

ct::String AssetsPanel::relative(const ct::String& path) const
{
    return path == mRoot ? ct::String() : EditorFileSystem::relativeTo(mRoot, path);
}

void AssetsPanel::onKeyPress(rt::KeyEvent& e)
{
    const Entry* entry = entryAt(mGrid->selected());
    if (!entry || entry->up || e.ctrl || e.alt || rt::WidgetApp::instance().focused() != mGrid)
        return rt::BoxLayout::onKeyPress(e);
    if (e.key == rt::Key::F1 + 1)
    {
        e.consumed = true;
        rename(*entry);
    }
    else if (e.key == rt::Key::Delete)
    {
        e.consumed = true;
        deleteRequested.emit(relative(entry->path));
    }
    else
        rt::BoxLayout::onKeyPress(e);
}

void AssetsPanel::showMenu(int index, float x, float y)
{
    rt::Menu* menu = assetsMenu();
    menu->clearActions();
    const Entry* found = entryAt(index);
    ct::String folder = mDirectory;
    if (found && !found->up)
    {
        const Entry entry = *found;
        if (entry.directory)
            folder = entry.path;
        menu->addAction("Open", [this, entry]()
        {
            if (entry.directory)
                open(entry.path);
            else
                fileOpened.emit(relative(entry.path));
        });
        menu->addAction("Rename...", [this, entry]() { rename(entry); })->setShortcut("F2");
        menu->addAction("Delete...", [this, entry]() { deleteRequested.emit(relative(entry.path)); })
            ->setShortcut("Delete");
        menu->addSeparator();
    }
    menu->addAction("New Folder...", [this]()
    {
        askName("New folder", "new_folder", [this](const ct::String& name)
        {
            if (!EditorFileSystem::makeDirectory(EditorFileSystem::join(mDirectory, name.c_str())))
                log_error("Could not make the folder %s", name.c_str());
            refresh();
        });
    });
    menu->addAction("New Script...", [this]()
    {
        askName("New script", "new_script.py", [this](const ct::String& name)
        {
            ct::String file = name;
            if (EditorFileSystem::extension(file) != "py")
                file += ".py";
            scriptRequested.emit(EditorFileSystem::join(mDirectory, file.c_str()));
            refresh();
        });
    });
    menu->addSeparator();
    menu->addAction("Show in Files", [folder]()
    {
        if (!open_url(folder.c_str()))
            log_error("Could not show %s", folder.c_str());
    });
    menu->addAction("Refresh", [this]() { refresh(); });
    menu->exec(x, y, this);
}

void AssetsPanel::askName(const char* title, const ct::String& name, ct::Function<void(const ct::String&)> done)
{
    auto* box = rt::WidgetApp::instance().addFloat<rt::InputBox>(title, "Name");
    box->setText(name);
    box->setValidator([this, name](const rt::String& typed) -> rt::String
    {
        const ct::String text = typed;
        if (text.empty())
            return "Type a name";
        if (text.find('/') != ct::String::npos || text.find('\\') != ct::String::npos)
            return "A name, not a path: drag the file onto a folder to move it";
        if (text[0] == '.')
            return "A name that starts with a dot is hidden";
        if (text != name && EditorFileSystem::exists(EditorFileSystem::join(mDirectory, text.c_str())))
            return "This folder has one with that name";
        return "";
    });
    box->accepted.connect([done](const rt::String& typed) { done(ct::String(typed)); });
}

void AssetsPanel::rename(const Entry& entry)
{
    const ct::String from = relative(entry.path);
    const ct::String folder = relative(EditorFileSystem::parentPath(entry.path));
    askName("Rename", entry.name, [this, from, folder, entry](const ct::String& name)
    {
        if (name != entry.name)
            moveRequested.emit(from, folder.empty() ? name : EditorFileSystem::join(folder, name.c_str()));
    });
}

void AssetsPanel::moveInto(const ct::String& from, const ct::String& folder)
{
    const ct::String name = EditorFileSystem::fileName(from);
    const ct::String to = folder.empty() ? name : EditorFileSystem::join(folder, name.c_str());
    if (to != from)
        moveRequested.emit(from, to);
}

void AssetsPanel::setRoot(const ct::String& root)
{
    mRoot = root;
    const ct::String assets = EditorFileSystem::join(root, "assets");
    open(EditorFileSystem::isDirectory(assets) ? assets : root);
}

void AssetsPanel::open(const ct::String& directory)
{
    mDirectory = directory;
    refresh();
}

void AssetsPanel::refresh()
{
    mGrid->clearItems();
    mEntries.clear();
    ct::Vector<EditorFileEntry> files;
    if (mDirectory.empty() || !EditorFileSystem::listDirectory(mDirectory, files))
    {
        mPath->setText(mDirectory.empty() ? "No project" : "Cannot read this folder");
        return;
    }
    // Folders first, then files, each by name; hidden files are left out.
    ct::sort(files.begin(), files.end(), [](const EditorFileEntry& a, const EditorFileEntry& b)
    {
        if (a.directory != b.directory)
            return a.directory;
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    if (!mRoot.empty() && mDirectory != mRoot)
    {
        Entry up;
        up.name = "..";
        up.path = EditorFileSystem::parentPath(mDirectory);
        up.directory = true;
        up.up = true;
        const int item = mGrid->addItem(up.name, rt::Color(40, 43, 50, 255));
        mGrid->setItemIcon(item, rt::IconId::ArrowUp);
        mEntries.push_back(up);
    }
    for (const EditorFileEntry& file : files)
    {
        if (file.name.empty() || file.name[0] == '.')
            continue;
        Entry entry;
        entry.name = file.name;
        entry.path = file.path;
        entry.directory = file.directory;
        const int item = mGrid->addItem(file.name, rt::Color(40, 43, 50, 255));
        mGrid->setItemIcon(item, iconFor(file.path, file.directory));
        if (!file.directory && hasExtension(file.path, kImages))
        {
            // Loaded under its project path, the name a scene stores it by.
            const ct::String name = EditorFileSystem::relativeTo(mRoot, file.path);
            Texture* texture = mAssets.GetTexture(name.c_str());
            if (!texture)
                texture = mAssets.LoadTexture(name.c_str(), file.path.c_str());
            if (texture)
            {
                rt::TextureHandle handle;
                handle.value = texture->Id();
                mGrid->setItemTexture(item, handle, static_cast<float>(texture->Width()),
                                      static_cast<float>(texture->Height()));
            }
        }
        mEntries.push_back(entry);
    }
    const ct::String shown = EditorFileSystem::relativeTo(mRoot, mDirectory);
    mPath->setText(shown == mDirectory ? mDirectory : (shown.empty() ? ct::String("/") : shown));
    rt::WidgetApp::instance().requestLayout();
}

} // namespace k2d::editor
