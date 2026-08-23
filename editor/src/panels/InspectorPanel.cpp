#include "InspectorPanel.h"

#include "core/EditorApplication.h"
#include "panels/AssetsPanel.h"
#include "panels/HierarchyPanel.h"

#include <k2d/Animation2D.h>
#include <k2d/CameraComponent.h>
#include <k2d/Component.h>
#include <k2d/DirectionalLight2D.h>
#include <k2d/GameObject.h>
#include <k2d/Light2D.h>
#include <k2d/LightOccluder2D.h>
#include <k2d/Line2D.h>
#include <k2d/NinePatchComponent.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Polygon2D.h>
#include <k2d/Scene.h>
#include <k2d/SpriteBatch.h>
#include <k2d/SpriteComponent.h>
#include <k2d/Texture.h>
#include <k2d/TileMapComponent.h>
#include <IconsMaterialDesignIcons.h>

#include <cmath>
#include <cstdint>

namespace k2d::editor
{

namespace
{
GameObject *findById(GameObject &object, uint64_t id)
{
    if (object.id() == id)
        return &object;
    for (size_t i = 0; i < object.childCount(); ++i)
        if (GameObject *found = findById(*object.child(i), id))
            return found;
    return nullptr;
}

const char *componentName(ComponentType type)
{
    static const char *names[] = {
        "Sprite", "Script", "Camera", "TileMap", "SpriteBatch", "Polygon2D",
        "Animation2D", "Light2D", "LightOccluder2D", "Line2D", "NinePatch",
        "Particle"
    };
    const unsigned int index = static_cast<unsigned int>(type);
    return index < static_cast<unsigned int>(ComponentType::Count) ? names[index] : "Unknown";
}

constexpr int kPlaceholderSize = 32;

Texture *placeholderSpriteTexture(EditorApplication &application)
{
    constexpr const char *kName = "__editor_sprite_placeholder";
    Texture *texture = application.assets().GetTexture(kName);
    if (texture)
        return texture;

    unsigned char pixels[kPlaceholderSize * kPlaceholderSize * 4];
    for (int i = 0; i < kPlaceholderSize * kPlaceholderSize; ++i)
    {
        pixels[i * 4 + 0] = 255;
        pixels[i * 4 + 1] = 255;
        pixels[i * 4 + 2] = 255;
        pixels[i * 4 + 3] = 255;
    }
    return application.assets().CreateTexture(kName, kPlaceholderSize, kPlaceholderSize, pixels, true, false);
}

Texture *acceptTextureDrop(EditorApplication &app)
{
    Texture *result = nullptr;
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kTextureDragDropPayload))
        {
            const char *path = static_cast<const char *>(payload->Data);
            Texture *loaded = app.assets().GetTexture(path);
            if (!loaded)
                loaded = app.assets().LoadTexture(path, path, true, false);
            result = loaded;
        }
        ImGui::EndDragDropTarget();
    }
    return result;
}

bool textureField(EditorApplication &app, const char *label, Texture *current, Texture *&outTexture)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(130.0f);
    const ImVec2 box(28.0f, 28.0f);
    if (current)
        ImGui::Image((ImTextureID)(intptr_t)current->Id(), box);
    else
        ImGui::Dummy(box);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Drag an image here from Assets");
    Texture *dropped = acceptTextureDrop(app);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", current ? "(assigned)" : "None");
    ImGui::PopID();
    if (dropped)
    {
        outTexture = dropped;
        return true;
    }
    return false;
}

bool dragVec2(EditorApplication &app, const char *label, Math::Vec2 &value, float speed, const char *undoLabel)
{
    float values[2] = {value.x, value.y};
    const bool changed = ImGui::DragFloat2(label, values, speed);
    if (ImGui::IsItemActivated())
        app.beginTransaction(undoLabel, app.beginChange());
    if (changed)
        value = Math::Vec2(values[0], values[1]);
    if (ImGui::IsItemDeactivatedAfterEdit())
        app.commitTransaction();
    return changed;
}

bool dragVec4(EditorApplication &app, const char *label, Math::Vec4 &value, float speed, const char *undoLabel)
{
    float values[4] = {value.x, value.y, value.z, value.w};
    const bool changed = ImGui::DragFloat4(label, values, speed);
    if (ImGui::IsItemActivated())
        app.beginTransaction(undoLabel, app.beginChange());
    if (changed)
        value = Math::Vec4(values[0], values[1], values[2], values[3]);
    if (ImGui::IsItemDeactivatedAfterEdit())
        app.commitTransaction();
    return changed;
}

bool dragFloatProperty(EditorApplication &app, const char *label, float &value, float speed,
                       const char *undoLabel, float minValue = 0.0f, float maxValue = 0.0f)
{
    const bool changed = ImGui::DragFloat(label, &value, speed, minValue, maxValue);
    if (ImGui::IsItemActivated())
        app.beginTransaction(undoLabel, app.beginChange());
    if (ImGui::IsItemDeactivatedAfterEdit())
        app.commitTransaction();
    return changed;
}

bool dragIntProperty(EditorApplication &app, const char *label, int &value, float speed,
                     const char *undoLabel, int minValue = 0, int maxValue = 0)
{
    const bool changed = ImGui::DragInt(label, &value, speed, minValue, maxValue);
    if (ImGui::IsItemActivated())
        app.beginTransaction(undoLabel, app.beginChange());
    if (ImGui::IsItemDeactivatedAfterEdit())
        app.commitTransaction();
    return changed;
}

bool colorEdit(EditorApplication &app, const char *label, Color &value, const char *undoLabel)
{
    const bool changed = ImGui::ColorEdit4(label, &value.r);
    if (ImGui::IsItemActivated())
        app.beginTransaction(undoLabel, app.beginChange());
    if (ImGui::IsItemDeactivatedAfterEdit())
        app.commitTransaction();
    return changed;
}

void colorToBytes(const Color &c, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a)
{
    r = static_cast<unsigned char>(std::lround(c.r * 255.0f));
    g = static_cast<unsigned char>(std::lround(c.g * 255.0f));
    b = static_cast<unsigned char>(std::lround(c.b * 255.0f));
    a = static_cast<unsigned char>(std::lround(c.a * 255.0f));
}

bool blendModeCombo(BlendMode &mode)
{
    static const char *names[] = {"Mix", "Add", "Sub", "Mul"};
    int index = static_cast<int>(mode);
    const bool changed = ImGui::Combo("Blend Mode", &index, names, 4);
    if (changed)
        mode = static_cast<BlendMode>(index);
    return changed;
}

template <class Setter>
void applyInstant(EditorApplication &app, const char *undoLabel, Setter &&setter)
{
    const EditorApplication::SceneChange before = app.beginChange();
    setter();
    app.commitChange(undoLabel, before);
}

template <class Setter>
void pivotPresetPicker(EditorApplication &app, const char *undoLabel, const Math::Vec2 &current, Setter &&setter)
{
    static const Math::Vec2 presets[9] = {
        Math::Vec2(0.0f, 0.0f), Math::Vec2(0.5f, 0.0f), Math::Vec2(1.0f, 0.0f),
        Math::Vec2(0.0f, 0.5f), Math::Vec2(0.5f, 0.5f), Math::Vec2(1.0f, 0.5f),
        Math::Vec2(0.0f, 1.0f), Math::Vec2(0.5f, 1.0f), Math::Vec2(1.0f, 1.0f)
    };
    static const char *names[9] = {
        "Top Left", "Top Center", "Top Right",
        "Center Left", "Center", "Center Right",
        "Bottom Left", "Bottom Center", "Bottom Right"
    };

    ImGui::TextUnformatted("Pivot Preset");
    for (int i = 0; i < 9; ++i)
    {
        if (i % 3 != 0)
            ImGui::SameLine();
        ImGui::PushID(i);
        const bool isCurrent =
            fabsf(current.x - presets[i].x) < 0.001f && fabsf(current.y - presets[i].y) < 0.001f;
        if (isCurrent)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.55f, 0.85f, 1.0f));
        if (ImGui::Button("##preset", ImVec2(22.0f, 22.0f)))
        {
            const Math::Vec2 target = presets[i];
            applyInstant(app, undoLabel, [&] { setter(target); });
        }
        if (isCurrent)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", names[i]);
        ImGui::PopID();
    }
}

void drawSpriteProperties(EditorApplication &app, SpriteComponent &sprite)
{
    Texture *texture = sprite.texture();
    Texture *newTexture = nullptr;
    if (textureField(app, "Texture", texture, newTexture))
        applyInstant(app, "Set Sprite Texture", [&] { sprite.setTexture(newTexture); });

    Texture *normalMap = sprite.normalMap();
    Texture *newNormalMap = nullptr;
    if (textureField(app, "Normal Map", normalMap, newNormalMap))
        applyInstant(app, "Set Sprite Normal Map", [&] { sprite.setNormalMap(newNormalMap); });

    Math::Vec2 size = sprite.size();
    if (dragVec2(app, "Size", size, 0.5f, "Resize Sprite"))
        sprite.setSize(size);

    Math::Vec2 pivot = sprite.pivot();
    if (dragVec2(app, "Pivot", pivot, 0.05f, "Adjust Sprite Pivot"))
        sprite.setPivot(pivot);
    pivotPresetPicker(app, "Set Sprite Pivot", sprite.pivot(), [&](const Math::Vec2 &p) { sprite.setPivot(p); });

    Math::Vec2 tiling = sprite.tiling();
    if (dragVec2(app, "Tiling", tiling, 0.05f, "Adjust Sprite Tiling"))
        sprite.setTiling(tiling.x, tiling.y);

    Color color = sprite.material().color();
    if (colorEdit(app, "Color", color, "Recolor Sprite"))
    {
        unsigned char r, g, b, a;
        colorToBytes(color, r, g, b, a);
        sprite.setColor(r, g, b, a);
    }

    bool flipX = sprite.flipX();
    bool flipY = sprite.flipY();
    if (ImGui::Checkbox("Flip X", &flipX))
        applyInstant(app, "Flip Sprite X", [&] { sprite.setFlip(flipX, sprite.flipY()); });
    ImGui::SameLine();
    if (ImGui::Checkbox("Flip Y", &flipY))
        applyInstant(app, "Flip Sprite Y", [&] { sprite.setFlip(sprite.flipX(), flipY); });

    bool ySort = sprite.ySort();
    if (ImGui::Checkbox("Y Sort", &ySort))
        applyInstant(app, "Toggle Sprite Y Sort", [&] { sprite.setYSort(ySort); });

    int lightMask = static_cast<int>(sprite.lightMask());
    if (dragIntProperty(app, "Light Mask", lightMask, 1.0f, "Change Sprite Light Mask", 0, 0x7fffffff))
        sprite.setLightMask(static_cast<unsigned int>(lightMask));

    BlendMode blendMode = sprite.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set Sprite Blend Mode", [&] { sprite.setBlendMode(blendMode); });

    bool hasRect = sprite.material().hasSourceRect();
    if (ImGui::Checkbox("Source Rect", &hasRect))
    {
        applyInstant(app, "Toggle Sprite Source Rect", [&]
        {
            if (hasRect)
            {
                const Math::Vec2 s = sprite.size();
                sprite.setSourceRect(0.0f, 0.0f, s.x, s.y);
            }
            else
            {
                sprite.clearSourceRect();
            }
        });
    }
    if (hasRect)
    {
        Math::Vec4 rect = sprite.material().sourceRect();
        if (dragVec4(app, "Rect", rect, 0.5f, "Adjust Sprite Source Rect"))
            sprite.setSourceRect(rect.x, rect.y, rect.z, rect.w);
    }
}

void drawTileMapProperties(EditorApplication &app, TileMapComponent &tileMap)
{
    Texture *texture = tileMap.texture();
    Texture *newTexture = nullptr;
    if (textureField(app, "Texture", texture, newTexture))
        applyInstant(app, "Set TileMap Texture", [&] { tileMap.setTexture(newTexture); });

    float cellW = tileMap.cellWidth();
    float cellH = tileMap.cellHeight();
    if (dragFloatProperty(app, "Cell Width", cellW, 0.5f, "Resize TileMap Cell", 1.0f, 4096.0f))
        tileMap.setCellSize(cellW, tileMap.cellHeight());
    if (dragFloatProperty(app, "Cell Height", cellH, 0.5f, "Resize TileMap Cell", 1.0f, 4096.0f))
        tileMap.setCellSize(tileMap.cellWidth(), cellH);

    int columns = tileMap.columns();
    int rows = tileMap.rows();
    if (dragIntProperty(app, "Columns", columns, 1.0f, "Resize TileMap", 1, 4096))
        tileMap.setMapSize(columns, tileMap.rows());
    if (dragIntProperty(app, "Rows", rows, 1.0f, "Resize TileMap", 1, 4096))
        tileMap.setMapSize(tileMap.columns(), rows);

    int atlasTilesX = tileMap.atlasTilesX();
    if (dragIntProperty(app, "Atlas Tiles X", atlasTilesX, 1.0f, "Change TileMap Atlas Width", 1, 4096))
        tileMap.setAtlasTilesX(atlasTilesX);

    bool hasCull = tileMap.hasCullRect();
    if (ImGui::Checkbox("Cull Rect", &hasCull))
    {
        applyInstant(app, "Toggle TileMap Cull Rect", [&]
        {
            if (hasCull)
                tileMap.setCullRect(0.0f, 0.0f, tileMap.columns() * tileMap.cellWidth(),
                                    tileMap.rows() * tileMap.cellHeight());
            else
                tileMap.clearCullRect();
        });
    }
    if (hasCull)
    {
        Math::Vec4 rect = tileMap.cullRect();
        if (dragVec4(app, "Rect", rect, 0.5f, "Adjust TileMap Cull Rect"))
            tileMap.setCullRect(rect.x, rect.y, rect.z, rect.w);
    }

    BlendMode blendMode = tileMap.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set TileMap Blend Mode", [&] { tileMap.setBlendMode(blendMode); });

    ImGui::TextDisabled("Painting individual tiles is not available in the Inspector yet.");
}

void drawPolygonProperties(EditorApplication &app, Polygon2D &polygon)
{
    Texture *texture = polygon.texture();
    Texture *newTexture = nullptr;
    if (textureField(app, "Texture", texture, newTexture))
        applyInstant(app, "Set Polygon Texture", [&] { polygon.setTexture(newTexture); });

    Color color = polygon.color();
    if (colorEdit(app, "Color", color, "Recolor Polygon"))
    {
        unsigned char r, g, b, a;
        colorToBytes(color, r, g, b, a);
        polygon.setColor(r, g, b, a);
    }

    BlendMode blendMode = polygon.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set Polygon Blend Mode", [&] { polygon.setBlendMode(blendMode); });

    ImGui::Text("%d point(s)", static_cast<int>(polygon.polygon().size()));
    ImGui::TextDisabled("Editing individual points is not available in the Inspector yet.");
}

void drawLineProperties(EditorApplication &app, Line2D &line)
{
    Texture *texture = line.texture();
    Texture *newTexture = nullptr;
    if (textureField(app, "Texture", texture, newTexture))
        applyInstant(app, "Set Line Texture", [&] { line.setTexture(newTexture); });

    float width = line.width();
    if (dragFloatProperty(app, "Width", width, 0.25f, "Resize Line Width", 0.01f, 4096.0f))
        line.setWidth(width);

    bool closed = line.closed();
    if (ImGui::Checkbox("Closed", &closed))
        applyInstant(app, "Toggle Line Closed", [&] { line.setClosed(closed); });

    Color color = line.color();
    if (colorEdit(app, "Color", color, "Recolor Line"))
    {
        unsigned char r, g, b, a;
        colorToBytes(color, r, g, b, a);
        line.setColor(r, g, b, a);
    }

    BlendMode blendMode = line.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set Line Blend Mode", [&] { line.setBlendMode(blendMode); });

    ImGui::Text("%d point(s)", static_cast<int>(line.points().size()));
    ImGui::TextDisabled("Editing individual points is not available in the Inspector yet.");
}

void drawNinePatchProperties(EditorApplication &app, NinePatchComponent &ninePatch)
{
    Texture *texture = ninePatch.texture();
    Texture *newTexture = nullptr;
    if (textureField(app, "Texture", texture, newTexture))
        applyInstant(app, "Set NinePatch Texture", [&] { ninePatch.setTexture(newTexture); });

    Math::Vec2 size = ninePatch.size();
    if (dragVec2(app, "Size", size, 0.5f, "Resize NinePatch"))
        ninePatch.setSize(size);

    Math::Vec4 margins = ninePatch.margins();
    if (dragVec4(app, "Margins (L,T,R,B)", margins, 0.25f, "Adjust NinePatch Margins"))
        ninePatch.setMargins(margins.x, margins.y, margins.z, margins.w);

    Math::Vec2 pivot = ninePatch.pivot();
    if (dragVec2(app, "Pivot", pivot, 0.05f, "Adjust NinePatch Pivot"))
        ninePatch.setPivot(pivot);
    pivotPresetPicker(app, "Set NinePatch Pivot", ninePatch.pivot(),
                      [&](const Math::Vec2 &p) { ninePatch.setPivot(p); });

    Color color = ninePatch.color();
    if (colorEdit(app, "Color", color, "Recolor NinePatch"))
    {
        unsigned char r, g, b, a;
        colorToBytes(color, r, g, b, a);
        ninePatch.setColor(r, g, b, a);
    }

    BlendMode blendMode = ninePatch.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set NinePatch Blend Mode", [&] { ninePatch.setBlendMode(blendMode); });
}

void drawOccluderProperties(EditorApplication &, LightOccluder2D &occluder)
{
    ImGui::Text("%d point(s)", static_cast<int>(occluder.points().size()));
    ImGui::TextDisabled("Editing individual points is not available in the Inspector yet.");
}

void drawPointLightProperties(EditorApplication &app, Light2D &light)
{
    Color color = light.color();
    if (colorEdit(app, "Color", color, "Recolor Light"))
        light.setColor(color.r, color.g, color.b, color.a);

    float energy = light.energy();
    if (dragFloatProperty(app, "Energy", energy, 0.05f, "Adjust Light Energy", 0.0f, 100.0f))
        light.setEnergy(energy);

    float radius = light.radius();
    if (dragFloatProperty(app, "Radius", radius, 1.0f, "Resize Light Radius", 0.0f, 100000.0f))
        light.setRadius(radius);

    float height = light.height();
    if (dragFloatProperty(app, "Height", height, 0.5f, "Adjust Light Height"))
        light.setHeight(height);

    bool castShadow = light.castShadow();
    if (ImGui::Checkbox("Cast Shadow", &castShadow))
        applyInstant(app, "Toggle Light Shadow", [&] { light.setCastShadow(castShadow); });

    if (castShadow)
    {
        Color shadowColor = light.shadowColor();
        if (colorEdit(app, "Shadow Color", shadowColor, "Recolor Light Shadow"))
            light.setShadowColor(shadowColor.r, shadowColor.g, shadowColor.b, shadowColor.a);

        static const char *filterNames[] = {"Nearest", "PCF 5", "PCF 13"};
        int filter = static_cast<int>(light.shadowFilter());
        if (ImGui::Combo("Shadow Filter", &filter, filterNames, 3))
            applyInstant(app, "Set Light Shadow Filter",
                        [&] { light.setShadowFilter(static_cast<ShadowFilter>(filter)); });
    }

    int cullMask = static_cast<int>(light.cullMask());
    if (dragIntProperty(app, "Cull Mask", cullMask, 1.0f, "Change Light Cull Mask", 0, 0x7fffffff))
        light.setCullMask(static_cast<unsigned int>(cullMask));
}

void drawDirectionalLightProperties(EditorApplication &app, DirectionalLight2D &light)
{
    Color color = light.color();
    if (colorEdit(app, "Color", color, "Recolor Light"))
        light.setColor(color.r, color.g, color.b, color.a);

    float energy = light.energy();
    if (dragFloatProperty(app, "Energy", energy, 0.05f, "Adjust Light Energy", 0.0f, 100.0f))
        light.setEnergy(energy);

    float height = light.height();
    if (dragFloatProperty(app, "Height", height, 0.5f, "Adjust Light Height"))
        light.setHeight(height);

    bool castShadow = light.castShadow();
    if (ImGui::Checkbox("Cast Shadow", &castShadow))
        applyInstant(app, "Toggle Light Shadow", [&] { light.setCastShadow(castShadow); });

    if (castShadow)
    {
        Color shadowColor = light.shadowColor();
        if (colorEdit(app, "Shadow Color", shadowColor, "Recolor Light Shadow"))
            light.setShadowColor(shadowColor.r, shadowColor.g, shadowColor.b, shadowColor.a);

        static const char *filterNames[] = {"Nearest", "PCF 5", "PCF 13"};
        int filter = static_cast<int>(light.shadowFilter());
        if (ImGui::Combo("Shadow Filter", &filter, filterNames, 3))
            applyInstant(app, "Set Light Shadow Filter",
                        [&] { light.setShadowFilter(static_cast<ShadowFilter>(filter)); });
    }

    int cullMask = static_cast<int>(light.cullMask());
    if (dragIntProperty(app, "Cull Mask", cullMask, 1.0f, "Change Light Cull Mask", 0, 0x7fffffff))
        light.setCullMask(static_cast<unsigned int>(cullMask));
}

void drawCameraProperties(EditorApplication &app, CameraComponent &cameraComponent)
{
    float viewportW = cameraComponent.viewportWidth();
    float viewportH = cameraComponent.viewportHeight();
    bool viewportChanged = false;
    viewportChanged |= dragFloatProperty(app, "Viewport Width", viewportW, 1.0f, "Resize Camera Viewport");
    viewportChanged |= dragFloatProperty(app, "Viewport Height", viewportH, 1.0f, "Resize Camera Viewport");
    if (viewportChanged)
        cameraComponent.setViewport(viewportW, viewportH);

    Camera2D &camera = cameraComponent.camera();

    Math::Vec2 position = camera.position;
    if (dragVec2(app, "Position", position, 0.5f, "Move Camera"))
        camera.position = position;

    float rotation = camera.rotationDegrees;
    if (dragFloatProperty(app, "Rotation", rotation, 0.5f, "Rotate Camera"))
        camera.rotationDegrees = rotation;

    Math::Vec2 zoom = camera.zoom;
    if (dragVec2(app, "Zoom", zoom, 0.01f, "Zoom Camera"))
        camera.zoom = zoom;

    Math::Vec2 offset = camera.offset;
    if (dragVec2(app, "Offset", offset, 0.5f, "Offset Camera"))
        camera.offset = offset;

    bool limitEnabled = camera.limitEnabled;
    if (ImGui::Checkbox("Limit Enabled", &limitEnabled))
        applyInstant(app, "Toggle Camera Limit", [&] { camera.limitEnabled = limitEnabled; });
    if (limitEnabled)
    {
        Math::Vec4 limits = camera.limits;
        if (dragVec4(app, "Limits (L,T,R,B)", limits, 0.5f, "Adjust Camera Limits"))
            camera.limits = limits;
    }

    bool smoothingEnabled = camera.smoothingEnabled;
    if (ImGui::Checkbox("Smoothing Enabled", &smoothingEnabled))
        applyInstant(app, "Toggle Camera Smoothing", [&] { camera.smoothingEnabled = smoothingEnabled; });
    if (smoothingEnabled)
    {
        float smoothingSpeed = camera.smoothingSpeed;
        if (dragFloatProperty(app, "Smoothing Speed", smoothingSpeed, 0.1f, "Adjust Camera Smoothing", 0.0f, 1000.0f))
            camera.smoothingSpeed = smoothingSpeed;
    }

    bool deadZoneEnabled = camera.deadZoneEnabled;
    if (ImGui::Checkbox("Dead Zone Enabled", &deadZoneEnabled))
        applyInstant(app, "Toggle Camera Dead Zone", [&] { camera.deadZoneEnabled = deadZoneEnabled; });
    if (deadZoneEnabled)
    {
        Math::Vec4 deadZone = camera.deadZone;
        if (dragVec4(app, "Dead Zone (L,T,R,B)", deadZone, 0.5f, "Adjust Camera Dead Zone"))
            camera.deadZone = deadZone;
    }

    bool targetEnabled = camera.targetEnabled;
    if (ImGui::Checkbox("Target Enabled", &targetEnabled))
        applyInstant(app, "Toggle Camera Target", [&] { camera.targetEnabled = targetEnabled; });
    if (targetEnabled)
    {
        Math::Vec2 target = camera.target;
        if (dragVec2(app, "Target", target, 0.5f, "Move Camera Target"))
            camera.target = target;
    }

    ImGui::TextUnformatted("Follow");
    ImGui::SameLine();
    const bool hasFollow = cameraComponent.hasFollowTarget();
    ct::String followLabel = hasFollow ? cameraComponent.followTargetName() : ct::String("None");
    if (hasFollow && !app.scene().find(cameraComponent.followTargetName().c_str()))
        followLabel += " (missing)";
    ImGui::Button(followLabel.c_str(), ImVec2(150.0f, 0.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kNodeDragDropPayload))
        {
            const uint64_t draggedId = *static_cast<const uint64_t *>(payload->Data);
            GameObject *dragged = findById(app.scene().root(), draggedId);
            if (dragged)
                applyInstant(app, "Set Camera Follow Target", [&] { cameraComponent.setFollowTarget(dragged); });
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Drag a node here from Hierarchy. Overrides Target every frame while set.");
    if (hasFollow)
    {
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_CLOSE "##clearFollow"))
            applyInstant(app, "Clear Camera Follow Target", [&] { cameraComponent.setFollowTarget(nullptr); });
    }
}

void drawSpriteBatchProperties(EditorApplication &app, SpriteBatch &batch)
{
    BlendMode blendMode = batch.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set SpriteBatch Blend Mode", [&] { batch.setBlendMode(blendMode); });

    ImGui::Text("%d entries", batch.count());
    ImGui::TextDisabled("Adding/editing individual entries is not available in the Inspector yet.");
}

void drawAnimationProperties(EditorApplication &app, Animation2D &anim)
{
    static const char *modeNames[] = {"One Shot", "Loop", "Ping Pong"};
    int mode = static_cast<int>(anim.mode());
    if (ImGui::Combo("Mode", &mode, modeNames, 3))
        applyInstant(app, "Set Animation Mode", [&] { anim.setMode(static_cast<AnimationMode>(mode)); });

    if (ImGui::Button(anim.playing() ? "Stop" : "Play"))
    {
        applyInstant(app, anim.playing() ? "Stop Animation" : "Play Animation", [&]
        {
            if (anim.playing())
                anim.stop();
            else
                anim.play();
        });
    }
    ImGui::SameLine();
    ImGui::Text("Frame %d / %d", anim.frame(), anim.frameCount());

    ImGui::Text("%d clip(s)%s%s", static_cast<int>(anim.clipCount()),
               anim.currentClip() ? ", current: " : "", anim.currentClip() ? anim.currentClip() : "");
    for (size_t i = 0; i < anim.clipCount(); ++i)
    {
        const AnimationClip *clip = anim.clipAt(i);
        if (!clip)
            continue;
        ImGui::PushID(static_cast<int>(i));
        const bool isCurrent = anim.currentClip() && clip->name == anim.currentClip();
        if (ImGui::Selectable(clip->name.c_str(), isCurrent))
            applyInstant(app, "Change Animation Clip", [&] { anim.play(clip->name.c_str()); });
        ImGui::PopID();
    }

    static char clipName[64] = "clip";
    static int frameWidth = 32;
    static int frameHeight = 32;
    static int frameCount = 1;
    static float framesPerSecond = 10.0f;
    static int clipMode = 1;
    static Texture *clipTexture = nullptr;

    if (ImGui::Button(ICON_MDI_PLAYLIST_PLUS " Add Clip"))
        ImGui::OpenPopup("Add Clip");
    if (ImGui::BeginPopup("Add Clip"))
    {
        ImGui::InputText("Name", clipName, sizeof(clipName));
        Texture *droppedClipTexture = nullptr;
        if (textureField(app, "Texture", clipTexture, droppedClipTexture))
            clipTexture = droppedClipTexture;
        ImGui::InputInt("Frame Width", &frameWidth);
        ImGui::InputInt("Frame Height", &frameHeight);
        ImGui::InputInt("Frame Count", &frameCount);
        ImGui::DragFloat("FPS", &framesPerSecond, 0.5f, 0.0f, 240.0f);
        ImGui::Combo("Mode", &clipMode, modeNames, 3);

        const bool valid = clipName[0] != '\0' && frameWidth > 0 && frameHeight > 0 && frameCount > 0;
        ImGui::BeginDisabled(!valid);
        if (ImGui::Button("Add"))
        {
            applyInstant(app, "Add Animation Clip", [&]
            {
                anim.addClip(clipName, clipTexture, frameWidth, frameHeight, frameCount, framesPerSecond,
                            static_cast<AnimationMode>(clipMode));
            });
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::TextDisabled("Editing an existing clip's frames is not available yet.");
}

void drawParticleProperties(EditorApplication &app, ParticleComponent &particleComponent)
{
    ParticleSystem &system = particleComponent.system();

    Texture *texture = system.GetTexture();
    Texture *newTexture = nullptr;
    if (textureField(app, "Texture", texture, newTexture))
        applyInstant(app, "Set Particle Texture", [&] { system.SetTexture(newTexture); });

    int capacity = static_cast<int>(system.Capacity());
    if (dragIntProperty(app, "Capacity", capacity, 1.0f, "Resize Particle Capacity", 1, 1000000))
        system.SetCapacity(static_cast<size_t>(capacity));

    static const char *modeNames[] = {"One Shot", "Persistent", "Loop"};
    int mode = static_cast<int>(system.GetMode());
    if (ImGui::Combo("Mode", &mode, modeNames, 3))
        applyInstant(app, "Set Particle Mode", [&] { system.SetMode(static_cast<ParticleMode>(mode)); });

    static const char *shapeNames[] = {"Point", "Circle", "Rectangle"};
    int shape = static_cast<int>(system.GetEmitterShape());
    if (ImGui::Combo("Emitter Shape", &shape, shapeNames, 3))
        applyInstant(app, "Set Particle Emitter Shape",
                    [&] { system.SetEmitterShape(static_cast<ParticleEmitterShape>(shape)); });

    Math::Vec2 emitterSize = system.EmitterSize();
    if (dragVec2(app, "Emitter Size", emitterSize, 0.5f, "Resize Particle Emitter"))
        system.SetEmitterSize(emitterSize);

    Math::Vec2 emitterPosition = system.EmitterPosition();
    if (dragVec2(app, "Emitter Position", emitterPosition, 0.5f, "Move Particle Emitter"))
        system.SetEmitterPosition(emitterPosition);

    float emissionRate = system.EmissionRate();
    if (dragFloatProperty(app, "Emission Rate", emissionRate, 0.25f, "Adjust Particle Emission Rate", 0.0f, 100000.0f))
        system.SetEmissionRate(emissionRate);

    int oneShotCount = static_cast<int>(system.OneShotCount());
    if (dragIntProperty(app, "One Shot Count", oneShotCount, 1.0f, "Adjust Particle One Shot Count", 0, 1000000))
        system.SetOneShotCount(static_cast<size_t>(oneShotCount));

    Math::Vec2 gravity = system.Gravity();
    if (dragVec2(app, "Gravity", gravity, 0.5f, "Adjust Particle Gravity"))
        system.SetGravity(gravity);

    if (ImGui::Button(system.IsPlaying() ? "Stop" : "Start"))
    {
        applyInstant(app, system.IsPlaying() ? "Stop Particles" : "Start Particles", [&]
        {
            if (system.IsPlaying())
                system.Stop();
            else
                system.Start();
        });
    }

    BlendMode blendMode = particleComponent.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set Particle Blend Mode", [&] { particleComponent.setBlendMode(blendMode); });

    bool ySort = particleComponent.ySort();
    if (ImGui::Checkbox("Y Sort", &ySort))
        applyInstant(app, "Toggle Particle Y Sort", [&] { particleComponent.setYSort(ySort); });
    ImGui::SameLine();
    bool followOwner = particleComponent.followOwner();
    if (ImGui::Checkbox("Follow Owner", &followOwner))
        applyInstant(app, "Toggle Particle Follow Owner", [&] { particleComponent.setFollowOwner(followOwner); });

    ImGui::SeparatorText("Emission Prefab");
    ParticlePrefab prefab = system.GetPrefab();

    if (dragVec2(app, "Direction", prefab.direction, 0.05f, "Adjust Particle Direction"))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Spread Degrees", prefab.spreadDegrees, 0.5f, "Adjust Particle Spread", 0.0f, 360.0f))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Speed Min", prefab.speedMin, 0.5f, "Adjust Particle Speed"))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Speed Max", prefab.speedMax, 0.5f, "Adjust Particle Speed"))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Life Min", prefab.lifeMin, 0.05f, "Adjust Particle Life", 0.001f, 100000.0f))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Life Max", prefab.lifeMax, 0.05f, "Adjust Particle Life", 0.001f, 100000.0f))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Size Min", prefab.sizeMin, 0.05f, "Adjust Particle Size", 0.0f, 100000.0f))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Size Max", prefab.sizeMax, 0.05f, "Adjust Particle Size", 0.0f, 100000.0f))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "End Size", prefab.endSize, 0.05f, "Adjust Particle End Size", 0.0f, 100000.0f))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Rotation Min", prefab.rotationMin, 0.5f, "Adjust Particle Rotation"))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Rotation Max", prefab.rotationMax, 0.5f, "Adjust Particle Rotation"))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Angular Velocity Min", prefab.angularVelocityMin, 0.5f, "Adjust Particle Spin"))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Angular Velocity Max", prefab.angularVelocityMax, 0.5f, "Adjust Particle Spin"))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Drag", prefab.drag, 0.01f, "Adjust Particle Drag"))
        system.SetPrefab(prefab);

    bool faceDirection = prefab.faceDirection;
    if (ImGui::Checkbox("Face Direction", &faceDirection))
    {
        prefab.faceDirection = faceDirection;
        applyInstant(app, "Toggle Particle Face Direction", [&] { system.SetPrefab(prefab); });
    }
    if (prefab.faceDirection)
    {
        if (dragFloatProperty(app, "Face Direction Offset", prefab.faceDirectionOffsetDegrees, 0.5f,
                              "Adjust Particle Face Direction Offset"))
            system.SetPrefab(prefab);
    }

    if (dragFloatProperty(app, "Fade In", prefab.fadeIn, 0.01f, "Adjust Particle Fade In", 0.0f, 100000.0f))
        system.SetPrefab(prefab);
    if (dragFloatProperty(app, "Fade Out", prefab.fadeOut, 0.01f, "Adjust Particle Fade Out", 0.0f, 100000.0f))
        system.SetPrefab(prefab);

    if (colorEdit(app, "Color Start", prefab.colorStart, "Recolor Particle Start"))
        system.SetPrefab(prefab);
    if (colorEdit(app, "Color End", prefab.colorEnd, "Recolor Particle End"))
        system.SetPrefab(prefab);

    if (dragVec4(app, "Atlas Bounds", prefab.atlasBounds, 0.5f, "Adjust Particle Atlas Bounds"))
        system.SetPrefab(prefab);
}

void drawComponentProperties(EditorApplication &app, Component &component)
{
    switch (component.type())
    {
    case ComponentType::Sprite:
        drawSpriteProperties(app, static_cast<SpriteComponent &>(component));
        break;
    case ComponentType::TileMap:
        drawTileMapProperties(app, static_cast<TileMapComponent &>(component));
        break;
    case ComponentType::Polygon2D:
        drawPolygonProperties(app, static_cast<Polygon2D &>(component));
        break;
    case ComponentType::LinePath:
        drawLineProperties(app, static_cast<Line2D &>(component));
        break;
    case ComponentType::NinePatch:
        drawNinePatchProperties(app, static_cast<NinePatchComponent &>(component));
        break;
    case ComponentType::SpriteBatch:
        drawSpriteBatchProperties(app, static_cast<SpriteBatch &>(component));
        break;
    case ComponentType::Animation:
        drawAnimationProperties(app, static_cast<Animation2D &>(component));
        break;
    case ComponentType::Light:
        if (Light2D *light = dynamic_cast<Light2D *>(&component))
            drawPointLightProperties(app, *light);
        else if (DirectionalLight2D *directional = dynamic_cast<DirectionalLight2D *>(&component))
            drawDirectionalLightProperties(app, *directional);
        break;
    case ComponentType::Occluder:
        drawOccluderProperties(app, static_cast<LightOccluder2D &>(component));
        break;
    case ComponentType::Camera:
        drawCameraProperties(app, static_cast<CameraComponent &>(component));
        break;
    case ComponentType::Particle:
        drawParticleProperties(app, static_cast<ParticleComponent &>(component));
        break;
    default:
        break;
    }
}
}

void InspectorPanel::drawContents()
{
    GameObject *object = app().selection().resolve(app().scene());
    if (!object)
    {
        ImGui::TextDisabled("Select an object in the Hierarchy or Scene view.");
        return;
    }

    bool locked = object->locked();
    if (ImGui::Checkbox(ICON_MDI_LOCK " Locked", &locked))
        applyInstant(app(), locked ? "Lock GameObject" : "Unlock GameObject", [&] { object->setLocked(locked); });
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Locked objects cannot be edited here or moved in the Scene view.");

    ImGui::BeginDisabled(locked);

    char name[256];
    size_t nameLength = object->name().size();
    if (nameLength >= sizeof(name))
        nameLength = sizeof(name) - 1;
    for (size_t i = 0; i < nameLength; ++i)
        name[i] = object->name()[i];
    name[nameLength] = '\0';
    EditorApplication::SceneChange before = app().beginChange();
    const bool nameChanged = ImGui::InputText("Name", name, sizeof(name));
    if (ImGui::IsItemActivated())
        app().beginTransaction("Rename GameObject", before);
    if (nameChanged)
        object->setName(name);
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();

    ImGui::TextDisabled("ID: %llu", static_cast<unsigned long long>(object->id()));

    char tag[128];
    size_t tagLength = object->tag().size();
    if (tagLength >= sizeof(tag))
        tagLength = sizeof(tag) - 1;
    for (size_t i = 0; i < tagLength; ++i)
        tag[i] = object->tag()[i];
    tag[tagLength] = '\0';
    before = app().beginChange();
    const bool tagChanged = ImGui::InputText("Tag", tag, sizeof(tag));
    if (ImGui::IsItemActivated())
        app().beginTransaction("Retag GameObject", before);
    if (tagChanged)
        object->setTag(tag);
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();

    bool active = object->active();
    before = app().beginChange();
    if (ImGui::Checkbox("Active", &active))
    {
        object->setActive(active);
        app().commitChange(active ? "Enable GameObject" : "Disable GameObject", before);
    }
    ImGui::SameLine();
    bool visible = object->visible();
    before = app().beginChange();
    if (ImGui::Checkbox("Visible", &visible))
    {
        object->setVisible(visible);
        app().commitChange(visible ? "Show GameObject" : "Hide GameObject", before);
    }

    Math::Vec2 position = object->position();
    float positionValues[2] = {position.x, position.y};
    before = app().beginChange();
    const bool positionChanged = ImGui::DragFloat2("Position", positionValues, 0.25f);
    if (ImGui::IsItemActivated())
        app().beginTransaction("Move GameObject", before);
    if (positionChanged)
        object->setPosition(Math::Vec2(positionValues[0], positionValues[1]));
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();
    ImGui::SameLine(0.0f, 8.0f);
    if (ImGui::Button(ICON_MDI_RESTORE "##resetPosition", ImVec2(26.0f, 0.0f)))
        applyInstant(app(), "Reset Position", [&] { object->setPosition(Math::Vec2(0.0f, 0.0f)); });
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reset Position");

    float rotation = object->rotationDegrees();
    before = app().beginChange();
    const bool rotationChanged = ImGui::DragFloat("Rotation", &rotation, 0.25f);
    if (ImGui::IsItemActivated())
        app().beginTransaction("Rotate GameObject", before);
    if (rotationChanged)
        object->setRotationDegrees(rotation);
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();
    ImGui::SameLine(0.0f, 8.0f);
    if (ImGui::Button(ICON_MDI_RESTORE "##resetRotation", ImVec2(26.0f, 0.0f)))
        applyInstant(app(), "Reset Rotation", [&] { object->setRotationDegrees(0.0f); });
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reset Rotation");

    Math::Vec2 scale = object->scale();
    float scaleValues[2] = {scale.x, scale.y};
    before = app().beginChange();
    const bool scaleChanged = ImGui::DragFloat2("Scale", scaleValues, 0.01f);
    if (ImGui::IsItemActivated())
        app().beginTransaction("Scale GameObject", before);
    if (scaleChanged)
        object->setScale(Math::Vec2(scaleValues[0], scaleValues[1]));
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();
    ImGui::SameLine(0.0f, 8.0f);
    if (ImGui::Button(ICON_MDI_RESTORE "##resetScale", ImVec2(26.0f, 0.0f)))
        applyInstant(app(), "Reset Scale", [&] { object->setScale(Math::Vec2(1.0f, 1.0f)); });
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reset Scale");

    int zIndex = object->zIndex();
    before = app().beginChange();
    const bool zChanged = ImGui::DragInt("Z Index", &zIndex);
    if (ImGui::IsItemActivated())
        app().beginTransaction("Change Z Index", before);
    if (zChanged)
        object->setZIndex(zIndex);
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();

    ImGui::SeparatorText("Components");
    bool any = false;
    for (unsigned int value = 0; value < static_cast<unsigned int>(ComponentType::Count); ++value)
    {
        const ComponentType type = static_cast<ComponentType>(value);
        const size_t count = object->rawComponentCount(type);
        for (size_t index = 0; index < count; ++index)
        {
            Component *component = object->rawComponent(type, index);
            if (!component)
                continue;
            any = true;
            ImGui::PushID(static_cast<int>(component->id()));
            bool componentActive = component->active();
            before = app().beginChange();
            if (ImGui::Checkbox("##active", &componentActive))
            {
                component->setActive(componentActive);
                app().commitChange(componentActive ? "Enable Component" : "Disable Component", before);
            }
            ImGui::SameLine();
            const bool open = ImGui::TreeNodeEx("##header", ImGuiTreeNodeFlags_DefaultOpen |
                                                ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth,
                                                "%s  #%u", componentName(type), component->id());
            if (open)
            {
                ImGui::Indent();
                drawComponentProperties(app(), *component);
                ImGui::Unindent();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    if (!any)
        ImGui::TextDisabled("No components yet.");

    if (ImGui::Button("Add Component..."))
        ImGui::OpenPopup("Add Component");
    if (ImGui::BeginPopup("Add Component"))
    {
        if (ImGui::MenuItem("Sprite"))
        {
            const EditorApplication::SceneChange addBefore = app().beginChange();
            object->addComponent<SpriteComponent>(placeholderSpriteTexture(app()));
            app().commitChange("Add Sprite Component", addBefore);
        }
        if (ImGui::MenuItem("Camera"))
        {
            const EditorApplication::SceneChange addBefore = app().beginChange();
            object->addComponent<CameraComponent>();
            app().commitChange("Add Camera Component", addBefore);
        }
        ImGui::Separator();
        ImGui::TextDisabled("More component types coming later.");
        ImGui::EndPopup();
    }

    ImGui::EndDisabled();
}

}
