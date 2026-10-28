#include "AtlasPanel.h"

#include "AssetsPanel.h"
#include "Icons.h"
#include "PropertyFields.h"
#include "core/EditorFileSystem.h"

#include "BasicWidgets.hpp"
#include "ComboBox.hpp"
#include "FileDialog.hpp"
#include "ScrollWidgets.hpp"
#include "SpinSlider.hpp"
#include "ToolBar.hpp"
#include "WidgetApp.hpp"

#define CT_RECTPACKER_IMPLEMENTATION
#include <ct/rectpacker.hpp>

#include <k2d/Assets.h>
#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/Pixmap.h>
#include <k2d/SpriteAtlas.h>
#include <k2d/Texture.h>

#include <ct/json.hpp>
#include <ct/ptr.hpp>

#include <platform.h>

#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

// The image list, which takes images and folders dropped from Assets.
class DropList : public rt::ListBox
{
public:
    IG_WIDGET_CLASS(DropList, rt::ListBox);

    ct::Function<void(const ct::String&)> dropped;

    bool acceptsDrop(const rt::DragPayload& payload) override { return payload.type == kAssetPayload; }

    void onDropReceive(const rt::DragPayload& payload) override
    {
        if (const ct::String* path = std::any_cast<ct::String>(&payload.data); path && dropped)
            dropped(*path);
    }
};

namespace
{

const int kSizes[] = {512, 1024, 2048, 4096};

bool isImage(const ct::String& path)
{
    const ct::String ext = EditorFileSystem::extension(path);
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tga";
}

} // namespace

AtlasPanel::AtlasPanel(Assets& assets) : rt::BoxLayout(rt::LayoutDir::Vertical), mAssets(assets)
{
    setPadding(4.0f);
    setSpacing(4.0f);

    auto* bar = createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    bar->setPadding(0.0f);
    bar->setSpacing(6.0f);
    auto* tools = bar->createChild<rt::ToolBar>();
    tools->setDraggable(false);
    const int openItem = tools->addButton(Icons::get("Open"), "Open an atlas (.atlas.json)");
    const int folderItem = tools->addButton(Icons::get("Folder"), "Add every image of a folder");
    const int removeItem = tools->addButton(Icons::get("Remove"), "Take the selected image out");
    const int clearItem = tools->addButton(rt::IconId::Trash, "Start a new atlas");
    tools->addSeparator();
    const int buildItem = tools->addButton(Icons::get("Save"), "Build the atlas: its image, regions and list");
    tools->buttonClicked.connect([this, openItem, folderItem, removeItem, clearItem, buildItem](int item)
    {
        rt::WidgetApp* app = rt::WidgetApp::existing();
        if (item == openItem && app)
        {
            auto* dialog = app->addFloat<rt::FileDialog>("Open atlas");
            dialog->setMode(rt::FileDialog::Mode::Open);
            dialog->setFilter("*.atlas.json");
            dialog->setPath(mRoot.empty() ? EditorFileSystem::currentDirectory() : mRoot);
            dialog->setRect({160.0f, 80.0f, 900.0f, 540.0f});
            dialog->accepted.connect([this](const rt::String& path) { open(path); });
        }
        else if (item == folderItem && app)
        {
            auto* dialog = app->addFloat<rt::FileDialog>("Add the images of a folder");
            dialog->setMode(rt::FileDialog::Mode::SelectFolder);
            dialog->setPath(mRoot.empty() ? EditorFileSystem::currentDirectory() : mRoot);
            dialog->setRect({160.0f, 80.0f, 900.0f, 540.0f});
            dialog->accepted.connect([this](const rt::String& path)
            {
                addFolder(EditorFileSystem::relativeTo(mRoot, path));
            });
        }
        else if (item == removeItem && mList->selectedIndex() >= 0)
        {
            mEntries.erase(mEntries.begin() + mList->selectedIndex());
            syncList();
        }
        else if (item == clearItem)
            clear();
        else if (item == buildItem)
            showBuildDialog();
    });

    mSize = bar->createChild<rt::ComboBox>();
    for (int size : kSizes)
    {
        char text[32];
        std::snprintf(text, sizeof text, "%d px", size);
        mSize->addItem(text);
    }
    mSize->setSelectedIndex(2);
    mSize->selectionChanged.connect([this](int index)
    {
        if (index >= 0)
            mMaxSize = kSizes[index];
    });
    NumberSpec paddingSpec;
    paddingSpec.integer = true;
    paddingSpec.step = 1.0;
    paddingSpec.minimum = 0.0;
    paddingSpec.maximum = 16.0;
    paddingSpec.width = 120.0f;
    mPaddingField = addNumberField(bar, "padding", mPadding, paddingSpec, [this](double value, bool)
    {
        mPadding = static_cast<int>(value);
    });
    mTrimBox = bar->createChild<rt::CheckBox>("Trim");
    mTrimBox->setTooltip("Cut the transparent border of each image (its frames lose their alignment)");
    mTrimBox->toggled.connect([this](bool on) { mTrim = on; });
    mInfo = bar->createChild<rt::Label>("");
    mInfo->setStretch(1.0f);
    mInfo->setLayoutAlign(rt::Align::Center);

    mList = createChild<DropList>();
    mList->setStretch(1.0f);
    mList->dropped = [this](const ct::String& path)
    {
        if (EditorFileSystem::isDirectory(absolute(path)))
            addFolder(path);
        else
            addImage(path);
    };
    syncList();
}

void AtlasPanel::setRoot(const ct::String& projectRoot)
{
    if (projectRoot != mRoot)
    {
        mEntries.clear();
        mOutput.clear();
    }
    mRoot = projectRoot;
    syncList();
}

void AtlasPanel::clear()
{
    mEntries.clear();
    mOutput.clear();
    syncList();
}

void AtlasPanel::setLayout(int maxSize, int padding, bool trim)
{
    mMaxSize = kSizes[0];
    for (int i = 0; i < 4; ++i)
        if (kSizes[i] <= maxSize)
        {
            mMaxSize = kSizes[i];
            mSize->setSelectedIndex(i);
        }
    mPadding = padding < 0 ? 0 : padding;
    mTrim = trim;
    mPaddingField->setValueNoSignal(mPadding);
    mTrimBox->setChecked(mTrim);
}

ct::String AtlasPanel::absolute(const ct::String& path) const
{
    return EditorFileSystem::resolve(mRoot, path);
}

void AtlasPanel::addImage(const ct::String& path)
{
    if (!isImage(path))
        return;
    for (const Entry& entry : mEntries)
        if (entry.path == path)
            return;
    Entry entry;
    entry.path = path;
    entry.name = EditorFileSystem::withoutExtension(EditorFileSystem::fileName(path));
    mEntries.push_back(entry);
    syncList();
}

void AtlasPanel::addFolder(const ct::String& folder)
{
    ct::Vector<EditorFileEntry> files;
    if (!EditorFileSystem::listDirectory(absolute(folder), files))
        return;
    for (const EditorFileEntry& file : files)
        if (!file.directory)
            addImage(EditorFileSystem::relativeTo(mRoot, file.path));
}

void AtlasPanel::syncList()
{
    mList->clearItems();
    for (const Entry& entry : mEntries)
        mList->addItem(entry.name + "    " + entry.path);
    char info[160];
    if (mEntries.empty())
        std::snprintf(info, sizeof info, "Drop images or folders from Assets here");
    else
        std::snprintf(info, sizeof info, "%zu images%s%s", mEntries.size(), mOutput.empty() ? "" : "  ->  ",
                      mOutput.c_str());
    mInfo->setText(info);
}

void AtlasPanel::showBuildDialog()
{
    rt::WidgetApp* app = rt::WidgetApp::existing();
    if (!app || mEntries.empty())
        return;
    auto* dialog = app->addFloat<rt::FileDialog>("Build the atlas image");
    dialog->setMode(rt::FileDialog::Mode::Save);
    dialog->setFilter("*.png");
    const ct::String textures = EditorFileSystem::join(mRoot, "assets/textures");
    dialog->setPath(!mOutput.empty() ? EditorFileSystem::parentPath(absolute(mOutput))
                    : EditorFileSystem::isDirectory(textures) ? textures
                                                              : mRoot);
    dialog->setFileName(mOutput.empty() ? ct::String("atlas.png") : EditorFileSystem::fileName(mOutput));
    dialog->setRect({160.0f, 80.0f, 900.0f, 540.0f});
    dialog->accepted.connect([this](const rt::String& chosen)
    {
        ct::String path = chosen;
        if (EditorFileSystem::extension(path) != "png")
            path += ".png";
        build(EditorFileSystem::relativeTo(mRoot, path));
    });
}

// One page per image file: atlas.png, or atlas_1.png, atlas_2.png... when the
// images need more than one.
bool AtlasPanel::build(const ct::String& imagePath)
{
    struct Source
    {
        ct::Unique<Pixmap> pixels;
        int x = 0, y = 0, w = 0, h = 0;
    };
    ct::Vector<Source> sources;
    ct::Vector<ct::RectPacker::Input> inputs;
    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        sources.push_back(Source{ct::make_unique<Pixmap>()});
        Source& source = sources[i];
        if (!source.pixels->Load(absolute(mEntries[i].path).c_str()))
        {
            log_error("Atlas: could not read %s", mEntries[i].path.c_str());
            return false;
        }
        source.w = source.pixels->Width();
        source.h = source.pixels->Height();
        if (mTrim && !source.pixels->ComputeTrimBounds(1, source.x, source.y, source.w, source.h))
            continue;   // nothing but transparency
        ct::RectPacker::Input input;
        input.id = static_cast<int32_t>(i);
        input.width = source.w;
        input.height = source.h;
        inputs.push_back(input);
    }
    const ct::Vector<ct::RectPacker::Page> pages = ct::RectPacker::pack_pages(inputs, mMaxSize, mMaxSize, mPadding);
    if (pages.empty())
    {
        log_error("Atlas: nothing to pack");
        return false;
    }

    const ct::String base = EditorFileSystem::withoutExtension(imagePath);
    std::size_t placed = 0;
    for (std::size_t p = 0; p < pages.size(); ++p)
    {
        const ct::RectPacker::Page& page = pages[p];
        char suffix[32];
        std::snprintf(suffix, sizeof suffix, "_%zu", p + 1);
        const ct::String pageBase = pages.size() == 1 ? base : base + suffix;
        const ct::String pagePath = pageBase + ".png";

        Pixmap canvas;
        canvas.Create(page.atlas_width, page.atlas_height);
        SpriteAtlas regions;
        regions.setTexturePath(pagePath.c_str());
        for (const ct::RectPacker::PlacedRect& rect : page.placed)
        {
            const Source& source = sources[static_cast<std::size_t>(rect.id)];
            Pixmap cut;
            if (source.pixels->CopyRect(source.x, source.y, source.w, source.h, cut))
                canvas.Blit(cut, rect.x, rect.y);
            regions.add(mEntries[static_cast<std::size_t>(rect.id)].name.c_str(),
                        Math::Vec4(static_cast<float>(rect.x), static_cast<float>(rect.y),
                                   static_cast<float>(rect.width), static_cast<float>(rect.height)));
            ++placed;
        }
        if (!canvas.Save(absolute(pagePath).c_str()) || !regions.save(absolute(pageBase + ".sprite.json").c_str()))
        {
            log_error("Atlas: could not write %s", pagePath.c_str());
            return false;
        }
        // A texture already loaded shows the new pixels at once.
        if (Texture* texture = mAssets.GetTexture(pagePath.c_str()))
            texture->Load(absolute(pagePath).c_str());
    }

    ct::Json recipe = ct::Json::object();
    recipe.set("maxSize", ct::Json(static_cast<int64_t>(mMaxSize)));
    recipe.set("padding", ct::Json(static_cast<int64_t>(mPadding)));
    recipe.set("trim", ct::Json(mTrim));
    ct::Json images = ct::Json::array();
    for (const Entry& entry : mEntries)
        images.push_back(ct::Json(entry.path.c_str()));
    recipe.set("images", images);
    FileSystem::Instance().SaveTextFile(absolute(base + ".atlas.json").c_str(), recipe.dump(2));

    mOutput = imagePath;
    syncList();
    if (placed < mEntries.size())
        log_warn("Atlas: %zu of %zu images did not fit or were empty", mEntries.size() - placed, mEntries.size());
    log_info("Atlas %s: %zu images on %zu page(s)", imagePath.c_str(), placed, pages.size());
    built.emit(pages.size() == 1 ? imagePath : base + "_1.png");
    return true;
}

bool AtlasPanel::open(const ct::String& recipePath)
{
    FileBuffer file;
    const ct::String path = absolute(recipePath);
    if (!FileSystem::Instance().LoadFile(path.c_str(), file, true))
        return false;
    const ct::Json recipe = ct::Json::parse(file.Text());
    if (!recipe.is_object())
        return false;
    mEntries.clear();
    const ct::Json& images = recipe["images"];
    for (std::size_t i = 0; images.is_array() && i < images.size(); ++i)
        addImage(images[i].as_cstr(""));
    setLayout(static_cast<int>(recipe["maxSize"].as_int(2048)), static_cast<int>(recipe["padding"].as_int(2)),
              recipe["trim"].as_bool(false));
    const ct::String relative = EditorFileSystem::relativeTo(mRoot, path);
    mOutput = relative.substr(0, relative.size() - 11) + ".png";   // without ".atlas.json"
    syncList();
    return true;
}

} // namespace k2d::editor
