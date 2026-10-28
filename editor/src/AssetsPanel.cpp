#include "AssetsPanel.h"

#include "Icons.h"
#include "core/EditorFileSystem.h"

#include "WidgetApp.hpp"

#include <k2d/Assets.h>
#include <k2d/Texture.h>

#include <ct/sort.hpp>

#include <cstring>

namespace rt = ig::retained;

namespace k2d::editor
{

const char* const kAssetPayload = "k2d-asset";

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
const char* const kScripts[] = {"zen", "zs", nullptr};
const char* const kScenes[] = {"k2dscene", nullptr};
const char* const kPrefabs[] = {"k2dprefab", "prefab", nullptr};

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
        if (index < 0 || index >= static_cast<int>(mEntries.size()))
            return;
        // Copied: opening a folder replaces the entries.
        const Entry entry = mEntries[static_cast<std::size_t>(index)];
        if (entry.directory)
            open(entry.path);
        else
            fileOpened.emit(EditorFileSystem::relativeTo(mRoot, entry.path));
    });
    mGrid->setDragPayload([this](int index)
    {
        rt::DragPayload payload;
        if (index < 0 || index >= static_cast<int>(mEntries.size()) || mEntries[index].directory)
            return payload;
        payload.type = kAssetPayload;
        payload.data = EditorFileSystem::relativeTo(mRoot, mEntries[index].path);
        return payload;
    });
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
