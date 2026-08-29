#include "AssetsPanel.h"

#include "core/EditorApplication.h"

#include <k2d/ChainCollider2D.h>
#include <k2d/FileSystem.h>
#include <k2d/GameObject.h>
#include <k2d/MaskContour2D.h>
#include <k2d/NavigationRegion2D.h>
#include <k2d/Pixmap.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Scene.h>
#include <k2d/Texture.h>
#include <IconsMaterialDesignIcons.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace k2d::editor
{

namespace
{
bool isImage(const ct::String& ext)
{
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tga" || ext == "gif" ||
           ext == "webp";
}

bool isPrefab(const ct::String& ext)
{
    return ext == "k2dprefab";
}
bool isScene(const ct::String& ext)
{
    return ext == "k2dscene";
}
bool isScript(const ct::String& ext)
{
    return ext == "py";
}

const char* dragPayloadFor(const ct::String& ext)
{
    if (isPrefab(ext))
        return kPrefabDragDropPayload;
    if (isScript(ext))
        return kScriptDragDropPayload;
    return kTextureDragDropPayload;
}

bool isCode(const ct::String& ext)
{
    return ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" || ext == "glsl" || ext == "vert" ||
           ext == "frag" || ext == "lua" || ext == "py";
}

bool isDocument(const ct::String& ext)
{
    return ext == "txt" || ext == "md" || ext == "json" || ext == "xml" || ext == "yaml" || ext == "yml" ||
           ext == "ini" || ext == "cfg";
}

bool isAudio(const ct::String& ext)
{
    return ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac";
}

bool isAssetName(const char* name)
{
    if (!name || !name[0] || std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0)
        return false;
    return std::strchr(name, '/') == nullptr && std::strchr(name, '\\') == nullptr;
}

const char* iconFor(bool directory, const ct::String& ext)
{
    if (directory)
        return ICON_MDI_FOLDER;
    if (isPrefab(ext))
        return ICON_MDI_CUBE_OUTLINE;
    if (isScene(ext))
        return ICON_MDI_SHAPE;
    if (isImage(ext))
        return ICON_MDI_FILE_IMAGE;
    if (isCode(ext))
        return ICON_MDI_CODE_BRACES;
    if (isAudio(ext))
        return ICON_MDI_FILE_MUSIC;
    if (isDocument(ext))
        return ICON_MDI_FILE_DOCUMENT;
    return ICON_MDI_FILE;
}

ImVec4 colorFor(bool directory, const ct::String& ext)
{
    if (directory)
        return ImVec4(0.95f, 0.78f, 0.35f, 1.0f);
    if (isPrefab(ext))
        return ImVec4(0.72f, 0.55f, 0.95f, 1.0f);
    if (isScene(ext))
        return ImVec4(0.55f, 0.70f, 0.95f, 1.0f);
    if (isImage(ext))
        return ImVec4(0.55f, 0.85f, 0.55f, 1.0f);
    if (isCode(ext))
        return ImVec4(0.6f, 0.65f, 0.95f, 1.0f);
    if (isAudio(ext))
        return ImVec4(0.9f, 0.55f, 0.65f, 1.0f);
    if (isDocument(ext))
        return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

ct::String scriptClassName(const char* fileName)
{
    ct::String result;
    bool uppercaseNext = true;
    for (const char* c = fileName; c && *c; ++c)
    {
        const bool lower = *c >= 'a' && *c <= 'z';
        const bool upper = *c >= 'A' && *c <= 'Z';
        const bool digit = *c >= '0' && *c <= '9';
        if (!lower && !upper && !digit)
        {
            uppercaseNext = true;
            continue;
        }
        if (result.empty() && digit)
            result += "Script";
        if (uppercaseNext && lower)
            result.push_back(static_cast<char>(*c - 'a' + 'A'));
        else
            result.push_back(*c);
        uppercaseNext = false;
    }
    return result.empty() ? ct::String("NewScript") : result;
}

ct::String makeScriptTemplate(const char* fileName, int kind)
{
    ct::String source;
    if (kind == 4 || kind == 5)
        source += "import math\n\n";
    source += "class ";
    source += scriptClassName(fileName);
    source += "(ScriptComponent):\n";

    switch (kind)
    {
    case 1:
        source += "    speed = 200\n\n"
                  "    def on_update(self, dt):\n"
                  "        dx = 0\n"
                  "        dy = 0\n"
                  "        if key_down(KEY_LEFT):\n"
                  "            dx = dx - 1\n"
                  "        if key_down(KEY_RIGHT):\n"
                  "            dx = dx + 1\n"
                  "        if key_down(KEY_UP):\n"
                  "            dy = dy - 1\n"
                  "        if key_down(KEY_DOWN):\n"
                  "            dy = dy + 1\n"
                  "        self.node.translate(dx * self.speed * dt, dy * self.speed * dt)\n";
        break;
    case 2:
        source += "    jump_impulse = 160000\n\n"
                  "    def on_start(self):\n"
                  "        self.body = self.node.get_body()\n\n"
                  "    def on_update(self, dt):\n"
                  "        if self.body != None and key_pressed(KEY_SPACE):\n"
                  "            self.body.apply_impulse(0, -self.jump_impulse)\n";
        break;
    case 3:
        source += "    def on_start(self):\n"
                  "        print(\"Script started on\", self.node.get_name())\n\n"
                  "    def on_event(self, name, value):\n"
                  "        print(\"Event:\", name, value)\n\n"
                  "    def on_collision(self, other, began):\n"
                  "        if began:\n"
                  "            print(\"Touched\", other.get_name())\n";
        break;
    case 4:
        source += "    turn_speed = 180\n"
                  "    thrust = 260\n"
                  "    aim_at_mouse = True\n\n"
                  "    def on_update(self, dt):\n"
                  "        if self.aim_at_mouse:\n"
                  "            mx, my = mouse_world_position()\n"
                  "            x, y = self.node.get_position()\n"
                  "            self.node.set_rotation(math.degrees(math.atan2(my - y, mx - x)))\n"
                  "        else:\n"
                  "            if key_down(KEY_A):\n"
                  "                self.node.set_rotation(self.node.get_rotation() - self.turn_speed * dt)\n"
                  "            if key_down(KEY_D):\n"
                  "                self.node.set_rotation(self.node.get_rotation() + self.turn_speed * dt)\n"
                  "        heading = math.radians(self.node.get_rotation())\n"
                  "        forward = 0\n"
                  "        if key_down(KEY_W):\n"
                  "            forward = forward + 1\n"
                  "        if key_down(KEY_S):\n"
                  "            forward = forward - 1\n"
                  "        if forward != 0:\n"
                  "            step = forward * self.thrust * dt\n"
                  "            self.node.translate(math.cos(heading) * step, math.sin(heading) * step)\n";
        break;
    case 5:
        source += "    speed = 220\n\n"
                  "    def on_update(self, dt):\n"
                  "        mx, my = mouse_world_position()\n"
                  "        x, y = self.node.get_position()\n"
                  "        heading = math.atan2(my - y, mx - x)\n"
                  "        self.node.set_rotation(math.degrees(heading))\n"
                  "        forward = 0\n"
                  "        strafe = 0\n"
                  "        if key_down(KEY_W):\n"
                  "            forward = forward + 1\n"
                  "        if key_down(KEY_S):\n"
                  "            forward = forward - 1\n"
                  "        if key_down(KEY_D):\n"
                  "            strafe = strafe + 1\n"
                  "        if key_down(KEY_A):\n"
                  "            strafe = strafe - 1\n"
                  "        if forward == 0 and strafe == 0:\n"
                  "            return\n"
                  "        length = math.sqrt(forward * forward + strafe * strafe)\n"
                  "        forward = forward / length\n"
                  "        strafe = strafe / length\n"
                  "        fx = math.cos(heading)\n"
                  "        fy = math.sin(heading)\n"
                  "        step = self.speed * dt\n"
                  "        self.node.translate((fx * forward - fy * strafe) * step,\n"
                  "                            (fy * forward + fx * strafe) * step)\n";
        break;
    default:
        source += "    def on_start(self):\n"
                  "        pass\n\n"
                  "    def on_update(self, dt):\n"
                  "        pass\n";
        break;
    }
    return source;
}
} // namespace

AssetsPanel::AssetsPanel(EditorApplication& application) : EditorPanel("Assets", application)
{
    const char* basePath = FileSystem::Instance().BasePath();
    mRoot = (basePath && basePath[0]) ? ct::String(basePath) : EditorFileSystem::currentDirectory();

    const ct::String& saved = application.settings().assetsDirectory;
    if (!saved.empty() && EditorFileSystem::isDirectory(saved))
        mRoot = saved;
    mHistory.push_back(mRoot);
    mHistoryPosition = 0;
}

AssetsPanel::~AssetsPanel()
{
    for (auto& entry : mThumbnailCache)
        delete entry.value;
    mThumbnailCache.clear();
    delete mMaskPixmap;
}

void AssetsPanel::navigateTo(const ct::String& directory)
{
    if (directory == mRoot || !EditorFileSystem::isDirectory(directory))
        return;

    while (mHistory.size() > mHistoryPosition + 1)
        mHistory.pop_back();
    mHistory.push_back(directory);
    mHistoryPosition = mHistory.size() - 1;
    mRoot = directory;
    mEntriesDirty = true;
}

void AssetsPanel::refreshEntries()
{
    if (!mEntriesDirty && mCachedDirectory == mRoot)
        return;

    mEntriesValid = EditorFileSystem::listDirectory(mRoot, mEntries);
    if (!mEntriesValid)
        mEntries.clear();
    mCachedDirectory = mRoot;
    mEntriesDirty = false;
}

Texture* AssetsPanel::thumbnailFor(const EditorFileEntry& entry)
{
    Texture** cached = mThumbnailCache.find(entry.path);
    if (cached)
        return *cached;

    Texture* texture = new Texture();
    if (!texture->Load(entry.path.c_str(), true, false))
    {
        delete texture;
        texture = nullptr;
    }
    mThumbnailCache[entry.path] = texture;
    return texture;
}

void AssetsPanel::drawToolbar()
{
    ImGui::BeginDisabled(mHistoryPosition == 0);
    if (ImGui::Button(ICON_MDI_ARROW_LEFT))
    {
        --mHistoryPosition;
        mRoot = mHistory[mHistoryPosition];
        mEntriesDirty = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(mHistoryPosition + 1 >= mHistory.size());
    if (ImGui::Button(ICON_MDI_ARROW_RIGHT))
    {
        ++mHistoryPosition;
        mRoot = mHistory[mHistoryPosition];
        mEntriesDirty = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_ARROW_UP))
        navigateTo(EditorFileSystem::parentPath(mRoot));
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_REFRESH))
        mEntriesDirty = true;
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_FILE_PLUS))
        requestNewScript(mRoot);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("New Zen script (.py) in this folder");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_FOLDER_PLUS))
        requestNewFolder(mRoot);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("New folder in this directory");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_VIEW_GRID))
        mViewMode = ViewMode::Grid;
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_VIEW_LIST))
        mViewMode = ViewMode::List;
    if (mViewMode == ViewMode::Grid)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("##assets_zoom", &mThumbnailSize, 48.0f, 220.0f, ICON_MDI_MAGNIFY " %.0f");
    }
}

void AssetsPanel::drawBreadcrumb()
{
    ct::Vector<ct::String> segments;
    ct::String current;
    for (size_t i = 0; i < mRoot.size(); ++i)
    {
        if (mRoot[i] == '/' || mRoot[i] == '\\')
        {
            if (!current.empty())
            {
                segments.push_back(current);
                current.clear();
            }
        }
        else
        {
            current.push_back(mRoot[i]);
        }
    }
    if (!current.empty())
        segments.push_back(current);

    ct::String accumulated = "/";
    ImGui::PushID("assets_crumb_root");
    if (ImGui::Selectable("/", false, ImGuiSelectableFlags_None, ImVec2(ImGui::CalcTextSize("/").x, 0.0f)))
        navigateTo(accumulated);
    ImGui::PopID();

    for (size_t i = 0; i < segments.size(); ++i)
    {
        accumulated = EditorFileSystem::join(accumulated, segments[i].c_str());
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextUnformatted("/");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(segments[i].c_str(), false, ImGuiSelectableFlags_None,
                              ImVec2(ImGui::CalcTextSize(segments[i].c_str()).x, 0.0f)))
            navigateTo(accumulated);
        ImGui::PopID();
    }
}

void AssetsPanel::drawDirectoryTree(const ct::String& directory)
{
    ct::Vector<EditorFileEntry> children;
    EditorFileSystem::listDirectory(directory, children);
    for (size_t i = 0; i < children.size(); ++i)
    {
        const EditorFileEntry& child = children[i];
        if (!child.directory)
            continue;

        ImGui::PushID(child.path.c_str());
        ImGuiStorage* storage = ImGui::GetStateStorage();
        const ImGuiID openId = ImGui::GetID("##open");
        bool open = storage->GetBool(openId, false);
        if (ImGui::ArrowButton("##expand", open ? ImGuiDir_Down : ImGuiDir_Right))
        {
            open = !open;
            storage->SetBool(openId, open);
        }
        ImGui::SameLine(0.0f, 4.0f);
        ct::String rowLabel(ICON_MDI_FOLDER);
        rowLabel += " ";
        rowLabel += child.name;
        if (ImGui::Selectable(rowLabel.c_str(), child.path == mRoot))
            navigateTo(child.path);
        if (open)
        {
            ImGui::Indent();
            drawDirectoryTree(child.path);
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
}

void AssetsPanel::drawBookmark(const char* label, const ct::String& root)
{
    if (root.empty() || !EditorFileSystem::isDirectory(root))
        return;

    ImGui::PushID(root.c_str());
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID openId = ImGui::GetID("##open");
    bool open = storage->GetBool(openId, false);
    if (ImGui::ArrowButton("##expand", open ? ImGuiDir_Down : ImGuiDir_Right))
    {
        open = !open;
        storage->SetBool(openId, open);
    }
    ImGui::SameLine(0.0f, 4.0f);
    ct::String rowLabel(ICON_MDI_FOLDER);
    rowLabel += " ";
    rowLabel += label;
    if (ImGui::Selectable(rowLabel.c_str(), root == mRoot))
        navigateTo(root);
    if (open)
    {
        ImGui::Indent();
        drawDirectoryTree(root);
        ImGui::Unindent();
    }
    ImGui::PopID();
}

void AssetsPanel::drawSidebar()
{
    ImGui::BeginChild("##assets_tree", ImVec2(mSidebarWidth, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (app().project().valid())
        drawBookmark("Project", app().project().assetsDirectory());
    drawBookmark("Filesystem", "/");
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::InvisibleButton("##assets_tree_splitter", ImVec2(4.0f, ImGui::GetContentRegionAvail().y));
    if (ImGui::IsItemActive())
    {
        mSidebarWidth += ImGui::GetIO().MouseDelta.x;
        if (mSidebarWidth < 120.0f)
            mSidebarWidth = 120.0f;
        if (mSidebarWidth > 420.0f)
            mSidebarWidth = 420.0f;
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    ImGui::SameLine();
}

void AssetsPanel::handleEntryInteraction(const EditorFileEntry& entry, bool clicked, bool doubleClicked)
{
    if (!clicked)
        return;

    if (entry.directory)
    {
        if (doubleClicked)
        {
            navigateTo(entry.path);
            ct::String message("Assets folder: ");
            message += entry.path;
            app().log(message);
        }
        return;
    }

    const ct::String ext = EditorFileSystem::extension(entry.path);
    if (isScript(ext) && doubleClicked)
    {
        app().openScriptEditor(entry.path.c_str());
        return;
    }
    if (isImage(ext) && doubleClicked)
    {
        app().openImageEditor(entry.path.c_str());
        return;
    }
    if (isPrefab(ext) && !doubleClicked)
    {
        app().previewPrefab(entry.path.c_str());
        ct::String message("Prefab preview: ");
        message += entry.name;
        app().log(message);
    }
}

void AssetsPanel::generateBumpMap(const EditorFileEntry& entry)
{
    Pixmap source;
    if (!source.Load(entry.path.c_str()))
    {
        app().log("Generate Bump Map failed: could not read image");
        app().toasts().error("Could not read image");
        return;
    }

    Pixmap* normalMap = source.GenerateNormalMap();
    if (!normalMap)
    {
        app().log("Generate Bump Map failed: could not generate");
        app().toasts().error("Could not generate bump map");
        return;
    }

    ct::String outputPath = EditorFileSystem::withoutExtension(entry.path);
    outputPath += "_normal.png";
    const bool ok = normalMap->Save(outputPath.c_str());
    delete normalMap;

    if (ok)
    {
        mEntriesDirty = true;
        ct::String message("Generated bump map: ");
        message += EditorFileSystem::fileName(outputPath);
        app().log(message);
        app().toasts().success(message);
    }
    else
    {
        app().log("Generate Bump Map failed: could not write file");
        app().toasts().error("Could not write bump map");
    }
}

void AssetsPanel::requestGenerateCollisionShape(const EditorFileEntry& entry)
{
    delete mMaskPixmap;
    mMaskPixmap = new Pixmap();
    if (!mMaskPixmap->Load(entry.path.c_str()))
    {
        delete mMaskPixmap;
        mMaskPixmap = nullptr;
        app().log("Generate Collision Shape failed: could not read image");
        app().toasts().error("Could not read image");
        return;
    }

    mMaskImagePath = entry.path;
    mMaskThreshold = 127;
    mMaskScale = 1.0f;
    mMaskSimplifyTolerance = 1.0f;
    mMaskMinArea = 16.0f;
    mMaskGenerateMode = 0;
    recomputeMaskContours();
    mMaskOpenPending = true;
}

void AssetsPanel::recomputeMaskContours()
{
    mMaskLoops.clear();
    mMaskPointCount = 0;
    if (!mMaskPixmap)
        return;

    MaskContourOptions options;
    options.threshold = (unsigned char)mMaskThreshold;
    options.simplifyTolerance = mMaskSimplifyTolerance;
    options.scale = mMaskScale;
    options.minArea = mMaskMinArea;

    TraceMaskContours(mMaskPixmap->Pixels(), mMaskPixmap->Width(), mMaskPixmap->Height(), 4, options, mMaskLoops);
    for (size_t i = 0; i < mMaskLoops.size(); ++i)
        mMaskPointCount += (int)mMaskLoops[i].size();
}

namespace
{
double loopSignedArea(const ct::Vector<Math::Vec2>& loop)
{
    double area2 = 0.0;
    const size_t count = loop.size();
    for (size_t i = 0; i < count; ++i)
    {
        const size_t j = (i + 1) % count;
        area2 += (double)loop[i].x * (double)loop[j].y - (double)loop[j].x * (double)loop[i].y;
    }
    return area2 * 0.5;
}
} // namespace

void AssetsPanel::createCollisionShapeFromMask()
{
    if (!mMaskPixmap || mMaskLoops.empty())
        return;

    const EditorApplication::SceneChange before = app().beginChange();

    GameObject* parent = app().selection().resolve(app().scene());
    if (!parent)
        parent = &app().scene().root();

    const ct::String name = EditorFileSystem::withoutExtension(EditorFileSystem::fileName(mMaskImagePath));
    GameObject* created = app().scene().createObject(name.c_str(), parent);
    if (!created)
        return;

    created->setPosition(Math::Vec2(0.0f, 0.0f));

    if (mMaskGenerateMode == 0 || mMaskGenerateMode == 2)
    {
        created->addComponent<RigidBody2D>()->setBodyType(BodyType::Static);
        for (size_t i = 0; i < mMaskLoops.size(); ++i)
        {
            ChainCollider2D* chain = created->addComponent<ChainCollider2D>();
            chain->setPoints(mMaskLoops[i].data(), (int)mMaskLoops[i].size());
            chain->setLoop(true);
        }
    }

    if (mMaskGenerateMode == 1 || mMaskGenerateMode == 2)
    {
        size_t outlineIndex = 0;
        double outlineArea = 0.0;
        for (size_t i = 0; i < mMaskLoops.size(); ++i)
        {
            const double area = std::fabs(loopSignedArea(mMaskLoops[i]));
            if (area > outlineArea)
            {
                outlineArea = area;
                outlineIndex = i;
            }
        }

        ct::Vector<const Math::Vec2*> holePtrs;
        ct::Vector<int> holeCounts;
        for (size_t i = 0; i < mMaskLoops.size(); ++i)
        {
            if (i == outlineIndex)
                continue;
            holePtrs.push_back(mMaskLoops[i].data());
            holeCounts.push_back((int)mMaskLoops[i].size());
        }

        NavigationRegion2D* region = created->addComponent<NavigationRegion2D>();
        region->setPolygonWithHoles(mMaskLoops[outlineIndex].data(), (int)mMaskLoops[outlineIndex].size(),
                                     holePtrs.data(), holeCounts.data(), (int)holePtrs.size());
    }

    app().selection().select(created);
    app().commitChange("Generate Collision Shape", before);

    ct::String message("Generated shape: ");
    message += name;
    app().log(message);
    app().toasts().success(message);
}

void AssetsPanel::drawGenerateCollisionShapePopup()
{
    // Opening from inside the context menu nests the modal in that popup and the
    // menu item's own close takes it down with it, so the open is deferred here.
    if (mMaskOpenPending)
    {
        ImGui::OpenPopup("Generate Collision Shape");
        mMaskOpenPending = false;
    }
    if (!ImGui::BeginPopupModal("Generate Collision Shape", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    if (!mMaskPixmap)
    {
        ImGui::TextDisabled("No image loaded.");
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    ImGui::TextDisabled("Mask: %s (%dx%d)", EditorFileSystem::fileName(mMaskImagePath).c_str(),
                        mMaskPixmap->Width(), mMaskPixmap->Height());

    ImGui::RadioButton("Collision", &mMaskGenerateMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Navigation", &mMaskGenerateMode, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Both", &mMaskGenerateMode, 2);

    bool changed = false;
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::SliderInt("Threshold", &mMaskThreshold, 0, 255))
        changed = true;
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::DragFloat("Scale (units/pixel)", &mMaskScale, 0.05f, 0.01f, 64.0f, "%.2f"))
        changed = true;
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::DragFloat("Simplify Tolerance", &mMaskSimplifyTolerance, 0.05f, 0.0f, 64.0f, "%.2f"))
        changed = true;
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::DragFloat("Min Area", &mMaskMinArea, 0.5f, 0.0f, 100000.0f, "%.1f"))
        changed = true;

    if (changed)
        recomputeMaskContours();

    ImGui::Separator();
    ImGui::Text("%d loops, %d points", (int)mMaskLoops.size(), mMaskPointCount);
    if (mMaskLoops.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.3f, 1.0f), "No shape found at this threshold.");

    ImGui::BeginDisabled(mMaskLoops.empty());
    if (ImGui::Button("Create"))
    {
        createCollisionShapeFromMask();
        delete mMaskPixmap;
        mMaskPixmap = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        delete mMaskPixmap;
        mMaskPixmap = nullptr;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void AssetsPanel::drawEntryContextMenu(const EditorFileEntry& entry)
{
    if (ImGui::BeginPopupContextItem())
    {
        if (entry.directory)
        {
            if (ImGui::MenuItem(ICON_MDI_FOLDER_OPEN " Open"))
                navigateTo(entry.path);
            drawCreationMenu(entry.path);
        }
        else
        {
            const ct::String ext = EditorFileSystem::extension(entry.path);
            if (isScript(ext) && ImGui::MenuItem(ICON_MDI_CODE_BRACES " Open Script"))
                app().openScriptEditor(entry.path.c_str());
            if (isImage(ext))
            {
                if (ImGui::MenuItem(ICON_MDI_CONTENT_CUT " Open in Image Editor"))
                    app().openImageEditor(entry.path.c_str());
                if (ImGui::MenuItem(ICON_MDI_TEXTURE " Generate Bump Map"))
                    generateBumpMap(entry);
                if (ImGui::MenuItem(ICON_MDI_VECTOR_POLYGON " Generate Collision Shape"))
                    requestGenerateCollisionShape(entry);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Rename"))
            requestRename(entry);
        ImGui::EndPopup();
    }
}

void AssetsPanel::drawCreationMenu(const ct::String& directory)
{
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_MDI_FOLDER_PLUS " Create Folder"))
        requestNewFolder(directory);
    if (ImGui::MenuItem(ICON_MDI_FILE_PLUS " Create Zen Script"))
        requestNewScript(directory);
}

void AssetsPanel::requestNewScript(const ct::String& directory)
{
    mCreateDirectory = directory;
    mNewScriptName[0] = '\0';
    mNewScriptOpenPending = true;
}

void AssetsPanel::requestNewFolder(const ct::String& directory)
{
    mCreateDirectory = directory;
    mNewFolderName[0] = '\0';
    mNewFolderOpenPending = true;
}

void AssetsPanel::requestRename(const EditorFileEntry& entry)
{
    mRenamePath = entry.path;
    std::snprintf(mRenameName, sizeof(mRenameName), "%s", entry.name.c_str());
    mRenameOpenPending = true;
}

void AssetsPanel::drawNewScriptPopup()
{
    if (mNewScriptOpenPending)
    {
        ImGui::OpenPopup("New Script");
        mNewScriptOpenPending = false;
    }
    if (!ImGui::BeginPopupModal("New Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    const ct::String& directory = mCreateDirectory.empty() ? mRoot : mCreateDirectory;
    ImGui::TextDisabled("Creating in %s", directory.c_str());
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("Name", "player", mNewScriptName, sizeof(mNewScriptName));
    static const char* templates[] = {"Empty",   "Movement",  "Physics body",
                                      "Events & collisions", "Ship (turn + thrust)", "Twin-stick (aim + strafe)"};
    ImGui::SetNextItemWidth(220.0f);
    ImGui::Combo("Template", &mNewScriptTemplate, templates,
                 static_cast<int>(sizeof(templates) / sizeof(templates[0])));
    ImGui::TextDisabled("Scripts inherit ScriptComponent and receive self.node automatically.");

    ct::String fileName(mNewScriptName);
    fileName += ".py";
    const ct::String target = EditorFileSystem::join(directory, fileName.c_str());
    const bool exists = EditorFileSystem::exists(target);
    if (exists)
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.3f, 1.0f), "%s already exists", fileName.c_str());

    ImGui::BeginDisabled(!isAssetName(mNewScriptName) || exists);
    if (ImGui::Button("Create"))
    {
        const ct::String templateSource = makeScriptTemplate(mNewScriptName, mNewScriptTemplate);
        if (FileSystem::Instance().SaveTextFile(target.c_str(), templateSource))
        {
            mEntriesDirty = true;
            app().toasts().info("Script created");
            app().log(ct::String("Created script: ") + target);
            app().openScriptEditor(target.c_str());
        }
        else
        {
            app().toasts().error("Could not create script");
        }
        mCreateDirectory.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        mCreateDirectory.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void AssetsPanel::drawNewFolderPopup()
{
    if (mNewFolderOpenPending)
    {
        ImGui::OpenPopup("New Folder");
        mNewFolderOpenPending = false;
    }
    if (!ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    const ct::String& directory = mCreateDirectory.empty() ? mRoot : mCreateDirectory;
    ImGui::TextDisabled("Creating in %s", directory.c_str());
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("Name", "textures", mNewFolderName, sizeof(mNewFolderName));

    const ct::String target = EditorFileSystem::join(directory, mNewFolderName);
    const bool exists = EditorFileSystem::exists(target);
    if (exists)
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.3f, 1.0f), "%s already exists", mNewFolderName);

    ImGui::BeginDisabled(!isAssetName(mNewFolderName) || exists);
    if (ImGui::Button("Create"))
    {
        if (EditorFileSystem::makeDirectory(target))
        {
            mEntriesDirty = true;
            app().toasts().info("Folder created");
            app().log(ct::String("Created folder: ") + target);
        }
        else
        {
            app().toasts().error("Could not create folder");
        }
        mCreateDirectory.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        mCreateDirectory.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void AssetsPanel::drawRenamePopup()
{
    if (mRenameOpenPending)
    {
        ImGui::OpenPopup("Rename Asset");
        mRenameOpenPending = false;
    }
    if (!ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextDisabled("Renaming %s", EditorFileSystem::fileName(mRenamePath).c_str());
    ImGui::SetNextItemWidth(280.0f);
    ImGui::InputText("Name", mRenameName, sizeof(mRenameName));

    const bool validName = isAssetName(mRenameName);
    const ct::String target = EditorFileSystem::join(EditorFileSystem::parentPath(mRenamePath), mRenameName);
    const bool unchanged = target == mRenamePath;
    const bool exists = !unchanged && EditorFileSystem::exists(target);
    if (!validName)
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.3f, 1.0f), "The name cannot contain a path separator.");
    else if (exists)
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.3f, 1.0f), "An asset with this name already exists.");

    ImGui::BeginDisabled(!validName || unchanged || exists);
    if (ImGui::Button("Rename"))
    {
        if (std::rename(mRenamePath.c_str(), target.c_str()) == 0)
        {
            mEntriesDirty = true;
            app().log(ct::String("Renamed asset: ") + target);
            app().toasts().success("Asset renamed");
            mRenamePath.clear();
            mRenameName[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        else
        {
            app().toasts().error("Could not rename asset");
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        mRenamePath.clear();
        mRenameName[0] = '\0';
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void AssetsPanel::drawGrid()
{
    const float cell = mThumbnailSize;
    const float panelWidth = ImGui::GetContentRegionAvail().x;
    int columns = static_cast<int>(panelWidth / (cell + 16.0f));
    if (columns < 1)
        columns = 1;

    if (!ImGui::BeginTable("##assets_grid", columns, ImGuiTableFlags_None))
        return;

    for (size_t i = 0; i < mEntries.size(); ++i)
    {
        ImGui::TableNextColumn();
        const EditorFileEntry& entry = mEntries[i];
        const ct::String ext = entry.directory ? ct::String() : EditorFileSystem::extension(entry.path);

        ImGui::PushID(static_cast<int>(i));
        ImGui::BeginGroup();

        ImGui::InvisibleButton("##cell", ImVec2(cell, cell));
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool doubleClicked = clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 cellMin = ImGui::GetItemRectMin();
        const ImVec2 cellMax = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        if (hovered)
            drawList->AddRectFilled(cellMin, cellMax, IM_COL32(60, 66, 78, 120), 4.0f);

        const bool selected = isPrefab(ext) && app().previewPrefabPath() == entry.path;
        if (selected)
            drawList->AddRect(cellMin, cellMax, IM_COL32(255, 205, 65, 220), 4.0f, 0, 2.0f);

        Texture* texture = !entry.directory && isImage(ext) ? thumbnailFor(entry) : nullptr;
        if (texture)
        {
            drawList->AddImage((ImTextureID)(intptr_t)texture->Id(), cellMin, cellMax);
        }
        else
        {
            const char* icon = iconFor(entry.directory, ext);
            const ImVec4 color = colorFor(entry.directory, ext);
            const ImVec2 textSize = ImGui::CalcTextSize(icon);
            const ImVec2 iconPos((cellMin.x + cellMax.x - textSize.x) * 0.5f,
                                 (cellMin.y + cellMax.y - textSize.y) * 0.5f);
            drawList->AddText(iconPos, ImGui::ColorConvertFloat4ToU32(color), icon);
        }

        if (!entry.directory && (isPrefab(ext) || isImage(ext) || isScript(ext)) && ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(dragPayloadFor(ext), entry.path.c_str(), entry.path.size() + 1);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::EndDragDropSource();
        }

        drawEntryContextMenu(entry);
        handleEntryInteraction(entry, clicked, doubleClicked);

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cell);
        ImGui::TextWrapped("%s", entry.name.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
        ImGui::PopID();
    }

    ImGui::EndTable();
}

void AssetsPanel::drawList()
{
    for (size_t i = 0; i < mEntries.size(); ++i)
    {
        const EditorFileEntry& entry = mEntries[i];
        const ct::String ext = entry.directory ? ct::String() : EditorFileSystem::extension(entry.path);
        const bool selected = isPrefab(ext) && app().previewPrefabPath() == entry.path;

        ImGui::PushID(static_cast<int>(i));
        const char* icon = iconFor(entry.directory, ext);
        const ImVec4 color = colorFor(entry.directory, ext);
        ImGui::TextColored(color, "%s", icon);
        ImGui::SameLine();

        const bool clicked = ImGui::Selectable(entry.name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick);
        const bool doubleClicked = clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        if (!entry.directory && (isPrefab(ext) || isImage(ext) || isScript(ext)) && ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(dragPayloadFor(ext), entry.path.c_str(), entry.path.size() + 1);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::EndDragDropSource();
        }

        drawEntryContextMenu(entry);
        handleEntryInteraction(entry, clicked, doubleClicked);
        ImGui::PopID();
    }
}

void AssetsPanel::drawContents()
{
    app().settings().assetsDirectory = mRoot;

    drawToolbar();
    ImGui::Separator();
    drawBreadcrumb();
    ImGui::Separator();

    refreshEntries();
    drawSidebar();

    ImGui::BeginChild("##assets_content", ImVec2(0.0f, 0.0f), false);
    if (!mEntriesValid)
        ImGui::TextDisabled("Unable to read this folder.");
    else if (mEntries.empty())
        ImGui::TextDisabled("Empty folder.");
    else if (mViewMode == ViewMode::Grid)
        drawGrid();
    else
        drawList();
    if (ImGui::BeginPopupContextWindow("Assets Context", ImGuiPopupFlags_MouseButtonRight |
                                                            ImGuiPopupFlags_NoOpenOverItems))
    {
        drawCreationMenu(mRoot);
        ImGui::EndPopup();
    }
    ImGui::EndChild();

    drawNewScriptPopup();
    drawNewFolderPopup();
    drawRenamePopup();
    drawGenerateCollisionShapePopup();
}

} // namespace k2d::editor
