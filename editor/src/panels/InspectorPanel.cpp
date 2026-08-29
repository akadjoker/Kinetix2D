#include "InspectorPanel.h"

#include "core/EditorApplication.h"
#include "panels/AssetsPanel.h"
#include "panels/HierarchyPanel.h"

#include <k2d/Animation2D.h>
#include <k2d/AudioPlayer.h>
#include <k2d/CameraComponent.h>
#include <k2d/CircleShape.h>
#include <k2d/CapsuleShape.h>
#include <k2d/Component.h>
#include <k2d/DirectionalLight2D.h>
#include <k2d/GameObject.h>
#include <k2d/Light2D.h>
#include <k2d/LightOccluder2D.h>
#include <k2d/Line2D.h>
#include <k2d/NavigationAgent2D.h>
#include <k2d/NavigationRegion2D.h>
#include <k2d/NinePatchComponent.h>
#include <k2d/ParallaxLayerComponent.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Polygon2D.h>
#include <k2d/RectShape.h>
#include <k2d/Scene.h>
#include <k2d/SpriteBatch.h>
#include <k2d/SpriteComponent.h>
#include <k2d/Texture.h>
#include <k2d/TileMapComponent.h>
#include <k2d/UiControls.h>
#include <k2d/BoxCollider2D.h>
#include <k2d/ChainCollider2D.h>
#include <k2d/CharacterBody2D.h>
#include <k2d/CircleCollider2D.h>
#include <k2d/DistanceJoint2D.h>
#include <k2d/EdgeCollider2D.h>
#include <k2d/GearJoint2D.h>
#include <k2d/Joint2D.h>
#include <k2d/MotorJoint2D.h>
#include <k2d/MouseJoint2D.h>
#include <k2d/PolygonCollider2D.h>
#include <k2d/RevoluteJoint2D.h>
#include <k2d/RigidBody2D.h>
#include <k2d/TileMapCollider2D.h>
#include <k2d/WheelJoint2D.h>
#include <k2d/ZenRuntime.h>
#include <k2d/ZenScriptComponent.h>
#include <IconsMaterialDesignIcons.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>

namespace k2d::editor
{

namespace
{
GameObject* findById(GameObject& object, uint64_t id)
{
    if (object.id() == id)
        return &object;
    for (size_t i = 0; i < object.childCount(); ++i)
        if (GameObject* found = findById(*object.child(i), id))
            return found;
    return nullptr;
}

struct JointDropFeedback
{
    const void* joint = nullptr;
    ct::String message;
    double expiresAt = 0.0;
};

JointDropFeedback& jointDropFeedback()
{
    static JointDropFeedback feedback;
    return feedback;
}

void rejectJointDrop(const void* joint, const ct::String& objectName, const char* reason)
{
    JointDropFeedback& feedback = jointDropFeedback();
    feedback.joint = joint;
    feedback.message = ct::String("'");
    feedback.message += objectName;
    feedback.message += "' ";
    feedback.message += reason;
    feedback.expiresAt = ImGui::GetTime() + 4.0;
}

void drawJointDropFeedback(const void* joint)
{
    JointDropFeedback& feedback = jointDropFeedback();
    if (feedback.joint == joint && ImGui::GetTime() < feedback.expiresAt)
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", feedback.message.c_str());
}

const char* componentName(ComponentType type)
{
    static const char* names[] = {"Sprite",
                                  "Script",
                                  "Camera",
                                  "TileMap",
                                  "SpriteBatch",
                                  "Polygon2D",
                                  "Animation2D",
                                  "Light2D",
                                  "LightOccluder2D",
                                  "Line2D",
                                  "NinePatch",
                                  "Particle",
                                  "AudioPlayer",
                                  "RigidBody2D",
                                  "Collider2D",
                                  "Joint2D",
                                  "CircleShape",
                                  "RectShape",
                                  "CapsuleShape",
                                  "UiCanvas",
                                  "UiPanel",
                                  "UiLabel",
                                  "UiButton",
                                  "UiCheckBox",
                                  "UiSlider",
                                  "NavigationRegion2D",
                                  "NavigationAgent2D",
                                  "MotionTween2D",
                                  "MotionStreak2D",
                                  "CharacterBody2D",
                                  "Skeleton2D",
                                  "Bone2D",
                                  "ParallaxLayer"};
    const unsigned int index = static_cast<unsigned int>(type);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "Unknown";
}

const char* componentName(const Component& component)
{
    if (dynamic_cast<const BoxCollider2D*>(&component))
        return "BoxCollider2D";
    if (dynamic_cast<const CircleCollider2D*>(&component))
        return "CircleCollider2D";
    if (dynamic_cast<const EdgeCollider2D*>(&component))
        return "EdgeCollider2D";
    if (dynamic_cast<const PolygonCollider2D*>(&component))
        return "PolygonCollider2D";
    if (dynamic_cast<const ChainCollider2D*>(&component))
        return "ChainCollider2D";
    if (dynamic_cast<const TileMapCollider2D*>(&component))
        return "TileMapCollider2D";
    if (dynamic_cast<const DirectionalLight2D*>(&component))
        return "DirectionalLight2D";
    if (dynamic_cast<const DistanceJoint2D*>(&component))
        return "DistanceJoint2D";
    if (dynamic_cast<const RevoluteJoint2D*>(&component))
        return "RevoluteJoint2D";
    if (dynamic_cast<const WheelJoint2D*>(&component))
        return "WheelJoint2D";
    if (dynamic_cast<const MotorJoint2D*>(&component))
        return "MotorJoint2D";
    if (dynamic_cast<const MouseJoint2D*>(&component))
        return "MouseJoint2D";
    if (dynamic_cast<const GearJoint2D*>(&component))
        return "GearJoint2D";
    return componentName(component.type());
}

const char* componentDescription(const Component& component)
{
    if (dynamic_cast<const BoxCollider2D*>(&component))
        return "Physical rectangular collision shape. Add a Rigid Body to simulate it.";
    if (dynamic_cast<const CircleCollider2D*>(&component))
        return "Physical circular collision shape. Add a Rigid Body to simulate it.";
    if (dynamic_cast<const EdgeCollider2D*>(&component))
        return "Physical line segment, useful for floors and one-dimensional obstacles.";
    if (dynamic_cast<const PolygonCollider2D*>(&component))
        return "Physical convex polygon collision shape.";
    if (dynamic_cast<const ChainCollider2D*>(&component))
        return "Physical chain of segments, useful for terrain.";
    if (dynamic_cast<const TileMapCollider2D*>(&component))
        return "Adds a box per merged run of solid cells from the sibling TileMapComponent.";
    if (dynamic_cast<const DistanceJoint2D*>(&component))
        return "Keeps two bodies within a fixed or spring-loaded distance range.";
    if (dynamic_cast<const RevoluteJoint2D*>(&component))
        return "Hinges two bodies around a shared point, with optional motor and limits.";
    if (dynamic_cast<const WheelJoint2D*>(&component))
        return "Suspension joint: a body slides along an axis with a spring, plus optional motor.";
    if (dynamic_cast<const MotorJoint2D*>(&component))
        return "Drives one body toward an offset from another, useful for scripted movement.";
    if (dynamic_cast<const MouseJoint2D*>(&component))
        return "Pulls this object's body toward a moving target point.";
    if (dynamic_cast<const GearJoint2D*>(&component))
        return "Couples two RevoluteJoint2D hinges with a fixed ratio.";
    switch (component.type())
    {
    case ComponentType::Sprite:
        return "Draws one texture with transform, colour and material settings.";
    case ComponentType::TileMap:
        return "Draws a grid of tiles from a texture atlas.";
    case ComponentType::ParallaxLayer:
        return "Draws a camera-relative repeating background layer for parallax scrolling.";
    case ComponentType::Polygon2D:
        return "Draws a filled custom polygon.";
    case ComponentType::LinePath:
        return "Draws a line or closed outline through editable points.";
    case ComponentType::CircleShape:
        return "Draws a circle; this is visual only, not a physics collider.";
    case ComponentType::RectShape:
        return "Draws a rectangle; this is visual only, not a physics collider.";
    case ComponentType::CapsuleShape:
        return "Draws a filled or outlined capsule; this is visual only, not a physics collider.";
    case ComponentType::NinePatch:
        return "Draws a scalable UI-style panel with protected borders.";
    case ComponentType::SpriteBatch:
        return "Draws many sprites efficiently from one component.";
    case ComponentType::Animation:
        return "Plays frame-based sprite animation clips.";
    case ComponentType::Script:
        return "Runs a Zen script. ScriptComponent scripts receive self.node.";
    case ComponentType::RigidBody:
        return "Gives this object a simulated physics body during Play.";
    case ComponentType::Particle:
        return "Emits and simulates particle effects.";
    case ComponentType::AudioPlayer:
        return "Plays an audio file as SFX or music during Play.";
    case ComponentType::Light:
        return "Adds point or directional lighting to the scene.";
    case ComponentType::Occluder:
        return "Blocks shadows cast by 2D lights.";
    case ComponentType::Camera:
        return "Controls the Game view projection and can follow another object.";
    case ComponentType::UiCanvas:
        return "Marks a screen-space UI hierarchy. UI children ignore the world camera.";
    case ComponentType::UiPanel:
        return "Screen-space panel with anchor and offset layout.";
    case ComponentType::UiLabel:
        return "Screen-space text label with anchor and offset layout.";
    case ComponentType::UiButton:
        return "Clickable screen-space button.";
    case ComponentType::UiCheckBox:
        return "Clickable boolean screen-space control.";
    case ComponentType::UiSlider:
        return "Clickable numeric screen-space control.";
    case ComponentType::NavigationRegion:
        return "Walkable polygon baked as a local triangle navigation mesh.";
    case ComponentType::NavigationAgent:
        return "Requests paths in NavigationRegion2D and optionally follows them.";
    case ComponentType::CharacterBody:
        return "Script-driven kinematic movement using this object's RigidBody2D and Collider2D components.";
    default:
        return "";
    }
}

bool componentMenuItem(const char* label, const char* description)
{
    const bool selected = ImGui::MenuItem(label);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", description);
    return selected;
}

constexpr int kPlaceholderSize = 32;

Texture* placeholderSpriteTexture(EditorApplication& application)
{
    constexpr const char* kName = "__editor_sprite_placeholder";
    Texture* texture = application.assets().GetTexture(kName);
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

Texture* acceptTextureDrop(EditorApplication& app)
{
    Texture* result = nullptr;
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kTextureDragDropPayload))
        {
            const char* path = static_cast<const char*>(payload->Data);
            Texture* loaded = app.assets().GetTexture(path);
            if (!loaded)
                loaded = app.assets().LoadTexture(path, path, true, false);
            result = loaded;
        }
        ImGui::EndDragDropTarget();
    }
    return result;
}

bool textureField(EditorApplication& app, const char* label, Texture* current, Texture*& outTexture)
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
    Texture* dropped = acceptTextureDrop(app);
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

bool dragVec2(EditorApplication& app, const char* label, Math::Vec2& value, float speed, const char* undoLabel)
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

bool dragVec4(EditorApplication& app, const char* label, Math::Vec4& value, float speed, const char* undoLabel)
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

bool dragFloatProperty(EditorApplication& app, const char* label, float& value, float speed, const char* undoLabel,
                       float minValue = 0.0f, float maxValue = 0.0f)
{
    const bool changed = ImGui::DragFloat(label, &value, speed, minValue, maxValue);
    if (ImGui::IsItemActivated())
        app.beginTransaction(undoLabel, app.beginChange());
    if (ImGui::IsItemDeactivatedAfterEdit())
        app.commitTransaction();
    return changed;
}

bool dragIntProperty(EditorApplication& app, const char* label, int& value, float speed, const char* undoLabel,
                     int minValue = 0, int maxValue = 0)
{
    const bool changed = ImGui::DragInt(label, &value, speed, minValue, maxValue);
    if (ImGui::IsItemActivated())
        app.beginTransaction(undoLabel, app.beginChange());
    if (ImGui::IsItemDeactivatedAfterEdit())
        app.commitTransaction();
    return changed;
}

bool colorEdit(EditorApplication& app, const char* label, Color& value, const char* undoLabel)
{
    const bool changed = ImGui::ColorEdit4(label, &value.r);
    if (ImGui::IsItemActivated())
        app.beginTransaction(undoLabel, app.beginChange());
    if (ImGui::IsItemDeactivatedAfterEdit())
        app.commitTransaction();
    return changed;
}

void colorToBytes(const Color& c, unsigned char& r, unsigned char& g, unsigned char& b, unsigned char& a)
{
    r = static_cast<unsigned char>(std::lround(c.r * 255.0f));
    g = static_cast<unsigned char>(std::lround(c.g * 255.0f));
    b = static_cast<unsigned char>(std::lround(c.b * 255.0f));
    a = static_cast<unsigned char>(std::lround(c.a * 255.0f));
}

bool blendModeCombo(BlendMode& mode)
{
    static const char* names[] = {"Mix", "Add", "Sub", "Mul"};
    int index = static_cast<int>(mode);
    const bool changed = ImGui::Combo("Blend Mode", &index, names, 4);
    if (changed)
        mode = static_cast<BlendMode>(index);
    return changed;
}

template <class Setter> void applyInstant(EditorApplication& app, const char* undoLabel, Setter&& setter)
{
    const EditorApplication::SceneChange before = app.beginChange();
    setter();
    app.commitChange(undoLabel, before);
}

template <class Setter>
void pivotPresetPicker(EditorApplication& app, const char* undoLabel, const Math::Vec2& current, Setter&& setter)
{
    static const Math::Vec2 presets[9] = {Math::Vec2(0.0f, 0.0f), Math::Vec2(0.5f, 0.0f), Math::Vec2(1.0f, 0.0f),
                                          Math::Vec2(0.0f, 0.5f), Math::Vec2(0.5f, 0.5f), Math::Vec2(1.0f, 0.5f),
                                          Math::Vec2(0.0f, 1.0f), Math::Vec2(0.5f, 1.0f), Math::Vec2(1.0f, 1.0f)};
    static const char* names[9] = {"Top Left",     "Top Center",  "Top Right",     "Center Left", "Center",
                                   "Center Right", "Bottom Left", "Bottom Center", "Bottom Right"};

    ImGui::TextUnformatted("Pivot Preset");
    for (int i = 0; i < 9; ++i)
    {
        if (i % 3 != 0)
            ImGui::SameLine();
        ImGui::PushID(i);
        const bool isCurrent = fabsf(current.x - presets[i].x) < 0.001f && fabsf(current.y - presets[i].y) < 0.001f;
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

template <class Apply>
void pointListEditor(EditorApplication& app, ct::Vector<Math::Vec2> points, size_t minPoints, const char* undoLabel,
                     Apply&& apply)
{
    for (size_t i = 0; i < points.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        float values[2] = {points[i].x, points[i].y};
        char label[16];
        std::snprintf(label, sizeof(label), "%d", static_cast<int>(i));
        const bool changed = ImGui::DragFloat2(label, values, 0.5f);
        if (ImGui::IsItemActivated())
            app.beginTransaction(undoLabel, app.beginChange());
        if (changed)
        {
            points[i] = Math::Vec2(values[0], values[1]);
            apply(points);
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            app.commitTransaction();
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::BeginDisabled(points.size() <= minPoints);
        if (ImGui::Button(ICON_MDI_CLOSE "##removePoint", ImVec2(26.0f, 0.0f)))
        {
            points.erase(points.begin() + i);
            applyInstant(app, undoLabel, [&] { apply(points); });
            ImGui::EndDisabled();
            ImGui::PopID();
            return;
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove point");
        ImGui::PopID();
    }
    if (ImGui::Button(ICON_MDI_PLUS " Add Point"))
    {
        const Math::Vec2 last = points.empty() ? Math::Vec2(0.0f, 0.0f) : points.back();
        points.push_back(last + Math::Vec2(20.0f, 0.0f));
        applyInstant(app, undoLabel, [&] { apply(points); });
    }
}

void drawSpriteProperties(EditorApplication& app, SpriteComponent& sprite)
{
    Texture* texture = sprite.texture();
    Texture* newTexture = nullptr;
    if (textureField(app, "Texture", texture, newTexture))
    {
        applyInstant(app, "Set Sprite Texture",
                     [&]
                     {
                         const Math::Vec2 previousSize = sprite.size();
                         const bool sizeMatchedOldTexture =
                             texture && previousSize.x == (float)texture->Width() &&
                             previousSize.y == (float)texture->Height();
                         sprite.setTexture(newTexture);
                         if (newTexture && (sizeMatchedOldTexture || previousSize.x <= 0.0f ||
                                            previousSize.y <= 0.0f))
                             sprite.setSize(
                                 Math::Vec2((float)newTexture->Width(), (float)newTexture->Height()));
                     });
    }

    Texture* normalMap = sprite.normalMap();
    Texture* newNormalMap = nullptr;
    if (textureField(app, "Normal Map", normalMap, newNormalMap))
        applyInstant(app, "Set Sprite Normal Map", [&] { sprite.setNormalMap(newNormalMap); });

    bool waterEnabled = sprite.waterEnabled();
    if (ImGui::Checkbox("Water Effect", &waterEnabled))
        applyInstant(app, "Toggle Sprite Water Effect", [&] { sprite.setWaterEnabled(waterEnabled); });
    if (waterEnabled)
    {
        if (!sprite.normalMap())
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "Water needs a normal map (it is sampled twice).");
        // Edit the persisted values directly. The transaction helpers take
        // their final snapshot when the ImGui item is released, so using a
        // temporary here would omit the final drag value from undo history.
        WaterEffect& water = sprite.water();
        dragFloatProperty(app, "Water Strength", water.strength, 0.0005f, "Adjust Water Refraction", 0.0f, 0.15f);
        dragFloatProperty(app, "Water Normal Scale", water.normalScale, 0.05f, "Adjust Water Detail Scale", 0.01f,
                          64.0f);
        dragVec2(app, "Water Flow A", water.flowA, 0.005f, "Adjust First Water Flow");
        dragVec2(app, "Water Flow B", water.flowB, 0.005f, "Adjust Second Water Flow");
        dragFloatProperty(app, "Water Highlight", water.highlight, 0.01f, "Adjust Water Crest Highlight", 0.0f, 2.0f);
        colorEdit(app, "Water Tint", water.tint, "Recolor Water");
    }

    Math::Vec2 size = sprite.size();
    if (dragVec2(app, "Size", size, 0.5f, "Resize Sprite"))
        sprite.setSize(size);

    Math::Vec2 pivot = sprite.pivot();
    if (dragVec2(app, "Pivot", pivot, 0.05f, "Adjust Sprite Pivot"))
        sprite.setPivot(pivot);
    pivotPresetPicker(app, "Set Sprite Pivot", sprite.pivot(), [&](const Math::Vec2& p) { sprite.setPivot(p); });

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
        applyInstant(app, "Toggle Sprite Source Rect",
                     [&]
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

void drawTileMapProperties(EditorApplication& app, TileMapComponent& tileMap)
{
    Texture* texture = tileMap.texture();
    Texture* newTexture = nullptr;
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

    Math::Vec2 atlasPadding = tileMap.atlasPadding();
    if (dragVec2(app, "Atlas Padding", atlasPadding, 0.5f, "Set TileMap Atlas Padding"))
        tileMap.setAtlasPadding(atlasPadding.x, atlasPadding.y);
    Math::Vec2 atlasGap = tileMap.atlasGap();
    if (dragVec2(app, "Atlas Gap", atlasGap, 0.5f, "Set TileMap Atlas Gap"))
        tileMap.setAtlasGap(atlasGap.x, atlasGap.y);

    bool hasCull = tileMap.hasCullRect();
    if (ImGui::Checkbox("Cull Rect", &hasCull))
    {
        applyInstant(app, "Toggle TileMap Cull Rect",
                     [&]
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

    ImGui::TextDisabled("Use the Tile Painter panel to paint tiles and collision cells.");
}

void drawParallaxLayerProperties(EditorApplication& app, ParallaxLayerComponent& layer)
{
    Texture* texture = layer.texture();
    Texture* newTexture = nullptr;
    if (textureField(app, "Texture", texture, newTexture))
    {
        applyInstant(app, "Set Parallax Layer Texture",
                     [&]
                     {
                         layer.setTexture(newTexture);
                         if (newTexture && layer.tileSize().x <= 0.0f)
                             layer.setTileSize(Math::Vec2((float)newTexture->Width(), (float)newTexture->Height()));
                     });
    }

    Math::Vec2 motionScale = layer.motionScale();
    if (dragVec2(app, "Motion Scale", motionScale, 0.01f, "Set Parallax Motion Scale"))
        layer.setMotionScale(motionScale);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("0 keeps the layer fixed to the camera; 1 moves with the world at full speed.");

    Math::Vec2 tileSize = layer.tileSize();
    if (dragVec2(app, "Tile Size", tileSize, 0.5f, "Resize Parallax Tile"))
        layer.setTileSize(tileSize);

    Color color = layer.color();
    if (colorEdit(app, "Color", color, "Recolor Parallax Layer"))
        layer.setColor(color);

    int zIndex = layer.zIndex();
    if (dragIntProperty(app, "Z Index", zIndex, 1.0f, "Change Parallax Z Index", -100000, 100000))
        layer.setZIndex(zIndex);
}

void drawPolygonProperties(EditorApplication& app, Polygon2D& polygon)
{
    Texture* texture = polygon.texture();
    Texture* newTexture = nullptr;
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

    ImGui::SeparatorText("Points");
    pointListEditor(app, polygon.polygon(), 3, "Edit Polygon Points",
                    [&](const ct::Vector<Math::Vec2>& pts) { polygon.setPolygon(pts.data(), (int)pts.size()); });
}

void drawNavigationRegionProperties(EditorApplication& app, NavigationRegion2D& region)
{
    ImGui::TextDisabled("Draw the walkable area. This is independent from TileMap and baked into triangles.");
    pointListEditor(app, region.polygon(), 3, "Edit Navigation Region", [&](const ct::Vector<Math::Vec2>& points)
                    { region.setPolygon(points.data(), static_cast<int>(points.size())); });
    ImGui::TextDisabled("%d triangle(s) baked", static_cast<int>(region.triangles().size() / 3));
}

void drawNavigationAgentProperties(EditorApplication& app, NavigationAgent2D& agent)
{
    Math::Vec2 target = agent.targetPosition();
    if (dragVec2(app, "Target", target, 0.5f, "Set Navigation Target"))
        agent.setTargetPosition(target);
    float tolerance = agent.pathDesiredDistance();
    if (dragFloatProperty(app, "Waypoint Distance", tolerance, 0.25f, "Set Navigation Waypoint Distance", 0.01f,
                          4096.0f))
        agent.setPathDesiredDistance(tolerance);
    float speed = agent.maxSpeed();
    if (dragFloatProperty(app, "Max Speed", speed, 1.0f, "Set Navigation Speed", 0.0f, 100000.0f))
        agent.setMaxSpeed(speed);
    bool autoMove = agent.autoMove();
    if (ImGui::Checkbox("Auto Move", &autoMove))
        applyInstant(app, "Toggle Navigation Auto Move", [&] { agent.setAutoMove(autoMove); });
    bool orientToPath = agent.orientToPath();
    if (ImGui::Checkbox("Orient To Path", &orientToPath))
        applyInstant(app, "Toggle Navigation Orientation", [&] { agent.setOrientToPath(orientToPath); });
    if (orientToPath)
    {
        float turnSpeed = agent.rotationLerpSpeed();
        if (dragFloatProperty(app, "Rotation Lerp", turnSpeed, 0.1f, "Set Navigation Rotation Lerp", 0.0f, 1000.0f))
            agent.setRotationLerpSpeed(turnSpeed);
        float offset = agent.rotationOffsetDegrees();
        if (dragFloatProperty(app, "Facing Offset", offset, 1.0f, "Set Navigation Facing Offset", -360.0f, 360.0f))
            agent.setRotationOffsetDegrees(offset);
        ImGui::TextDisabled("0 lerp speed rotates instantly. Use offset when the sprite does not face right.");
    }
    ImGui::TextDisabled("%d waypoint(s)%s", static_cast<int>(agent.path().size()),
                        agent.isNavigationFinished() ? " — finished" : "");
}

void drawLineProperties(EditorApplication& app, Line2D& line)
{
    Texture* texture = line.texture();
    Texture* newTexture = nullptr;
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

    ImGui::SeparatorText("Points");
    pointListEditor(app, line.points(), 2, "Edit Line Points",
                    [&](const ct::Vector<Math::Vec2>& pts) { line.setPoints(pts.data(), (int)pts.size()); });
}

bool shapeModeCombo(ShapeRenderMode& mode)
{
    int value = mode == ShapeRenderMode::Line ? 1 : 0;
    if (!ImGui::Combo("Mode", &value, "Fill\0Line\0"))
        return false;
    mode = value == 1 ? ShapeRenderMode::Line : ShapeRenderMode::Fill;
    return true;
}

void drawCircleShapeProperties(EditorApplication& app, CircleShape& shape)
{
    float radius = shape.radius();
    if (dragFloatProperty(app, "Radius", radius, 0.5f, "Resize Circle Shape", 0.0f, 100000.0f))
        shape.setRadius(radius);

    int segments = shape.segments();
    if (dragIntProperty(app, "Segments", segments, 1.0f, "Set Circle Shape Segments", 3, 512))
        shape.setSegments(segments);

    ShapeRenderMode mode = shape.mode();
    if (shapeModeCombo(mode))
        applyInstant(app, "Set Circle Shape Mode", [&] { shape.setMode(mode); });
    if (mode == ShapeRenderMode::Line)
    {
        float width = shape.lineWidth();
        if (dragFloatProperty(app, "Line Width", width, 0.25f, "Set Circle Shape Line Width", 0.01f, 4096.0f))
            shape.setLineWidth(width);
    }

    Color color = shape.color();
    if (colorEdit(app, "Color", color, "Recolor Circle Shape"))
    {
        unsigned char r, g, b, a;
        colorToBytes(color, r, g, b, a);
        shape.setColor(r, g, b, a);
    }
    BlendMode blendMode = shape.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set Circle Shape Blend Mode", [&] { shape.setBlendMode(blendMode); });
}

void drawRectShapeProperties(EditorApplication& app, RectShape& shape)
{
    Math::Vec2 size = shape.size();
    if (dragVec2(app, "Size", size, 0.5f, "Resize Rect Shape"))
        shape.setSize(size);

    ShapeRenderMode mode = shape.mode();
    if (shapeModeCombo(mode))
        applyInstant(app, "Set Rect Shape Mode", [&] { shape.setMode(mode); });
    if (mode == ShapeRenderMode::Line)
    {
        float width = shape.lineWidth();
        if (dragFloatProperty(app, "Line Width", width, 0.25f, "Set Rect Shape Line Width", 0.01f, 4096.0f))
            shape.setLineWidth(width);
    }

    Color color = shape.color();
    if (colorEdit(app, "Color", color, "Recolor Rect Shape"))
    {
        unsigned char r, g, b, a;
        colorToBytes(color, r, g, b, a);
        shape.setColor(r, g, b, a);
    }
    BlendMode blendMode = shape.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set Rect Shape Blend Mode", [&] { shape.setBlendMode(blendMode); });
}

void drawCapsuleShapeProperties(EditorApplication& app, CapsuleShape& shape)
{
    Math::Vec2 size = shape.size();
    if (dragVec2(app, "Size", size, 0.5f, "Resize Capsule Shape"))
        shape.setSize(size);

    int segments = shape.segments();
    if (dragIntProperty(app, "Segments", segments, 1.0f, "Set Capsule Shape Segments", 3, 512))
        shape.setSegments(segments);

    ShapeRenderMode mode = shape.mode();
    if (shapeModeCombo(mode))
        applyInstant(app, "Set Capsule Shape Mode", [&] { shape.setMode(mode); });
    if (mode == ShapeRenderMode::Line)
    {
        float width = shape.lineWidth();
        if (dragFloatProperty(app, "Line Width", width, 0.25f, "Set Capsule Shape Line Width", 0.01f, 4096.0f))
            shape.setLineWidth(width);
    }

    Color color = shape.color();
    if (colorEdit(app, "Color", color, "Recolor Capsule Shape"))
    {
        unsigned char r, g, b, a;
        colorToBytes(color, r, g, b, a);
        shape.setColor(r, g, b, a);
    }
    BlendMode blendMode = shape.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set Capsule Shape Blend Mode", [&] { shape.setBlendMode(blendMode); });
}

void drawAudioPlayerProperties(EditorApplication& app, AudioPlayer& player)
{
    char source[512];
    std::snprintf(source, sizeof(source), "%s", player.source());
    if (ImGui::InputText("Source", source, sizeof(source)))
        applyInstant(app, "Set Audio Source", [&] { player.setSource(source); });

    bool music = player.music();
    if (ImGui::Checkbox("Music", &music))
        applyInstant(app, "Set Audio Player Type", [&] { player.setMusic(music); });
    bool autoplay = player.autoplay();
    if (ImGui::Checkbox("Autoplay", &autoplay))
        applyInstant(app, "Set Audio Player Autoplay", [&] { player.setAutoplay(autoplay); });
    bool loop = player.loop();
    if (ImGui::Checkbox("Loop", &loop))
        applyInstant(app, "Set Audio Player Loop", [&] { player.setLoop(loop); });

    float volume = player.volume();
    if (dragFloatProperty(app, "Volume", volume, 0.01f, "Set Audio Player Volume", 0.0f, 4.0f))
        player.setVolume(volume);
    if (!music)
    {
        float pitch = player.pitch();
        if (dragFloatProperty(app, "Pitch", pitch, 0.01f, "Set Audio Player Pitch", 0.01f, 4.0f))
            player.setPitch(pitch);
        float pan = player.pan();
        if (dragFloatProperty(app, "Pan", pan, 0.01f, "Set Audio Player Pan", -1.0f, 1.0f))
            player.setPan(pan);
        bool spatial = player.spatial();
        if (ImGui::Checkbox("Spatial 2D", &spatial))
            applyInstant(app, "Toggle Spatial Audio", [&] { player.setSpatial(spatial); });
        if (spatial)
        {
            float minDistance = player.minDistance();
            if (dragFloatProperty(app, "Min Distance", minDistance, 1.0f, "Set Spatial Min Distance", 0.0f, 1000000.0f))
                player.setMinDistance(minDistance);
            float maxDistance = player.maxDistance();
            if (dragFloatProperty(app, "Max Distance", maxDistance, 1.0f, "Set Spatial Max Distance", minDistance,
                                  1000000.0f))
                player.setMaxDistance(maxDistance);
            float rolloff = player.rolloff();
            if (dragFloatProperty(app, "Rolloff", rolloff, 0.01f, "Set Spatial Rolloff", 0.0f, 100.0f))
                player.setRolloff(rolloff);
        }
    }
}

void drawNinePatchProperties(EditorApplication& app, NinePatchComponent& ninePatch)
{
    Texture* texture = ninePatch.texture();
    Texture* newTexture = nullptr;
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
                      [&](const Math::Vec2& p) { ninePatch.setPivot(p); });

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

void drawUiLayoutProperties(EditorApplication& app, UiControl& control)
{
    ImGui::SeparatorText("Layout (screen space)");
    Math::Vec4 anchors = control.anchors();
    if (dragVec4(app, "Anchors (L,T,R,B)", anchors, 0.01f, "Adjust UI Anchors"))
        control.setAnchors(anchors);
    Math::Vec4 offsets = control.offsets();
    if (dragVec4(app, "Offsets (L,T,R,B)", offsets, 0.5f, "Adjust UI Offsets"))
        control.setOffsets(offsets);
    ImGui::TextDisabled("Anchors are normalized (0..1); offsets are pixels.");
}

void drawUiTextProperty(EditorApplication& app, const char* label, const ct::String& value,
                        const std::function<void(const char*)>& set)
{
    char text[256];
    std::snprintf(text, sizeof(text), "%s", value.c_str());
    if (ImGui::InputText(label, text, sizeof(text)))
        applyInstant(app, "Edit UI Text", [&] { set(text); });
}

void drawUiPanelProperties(EditorApplication& app, UiPanel& panel)
{
    drawUiLayoutProperties(app, panel);
    Color color = panel.color();
    if (colorEdit(app, "Color", color, "Recolor UI Panel"))
        panel.setColor(color);
}

void drawUiLabelProperties(EditorApplication& app, UiLabel& label)
{
    drawUiLayoutProperties(app, label);
    drawUiTextProperty(app, "Text", label.text(), [&](const char* value) { label.setText(value); });
    float size = label.fontSize();
    if (dragFloatProperty(app, "Font Size", size, 0.5f, "Set UI Label Font Size", 1.0f, 256.0f))
        label.setFontSize(size);
    Color color = label.color();
    if (colorEdit(app, "Color", color, "Recolor UI Label"))
        label.setColor(color);
}

void drawUiButtonProperties(EditorApplication& app, UiButton& button)
{
    drawUiLayoutProperties(app, button);
    drawUiTextProperty(app, "Text", button.text(), [&](const char* value) { button.setText(value); });
}

void drawUiCheckBoxProperties(EditorApplication& app, UiCheckBox& check)
{
    drawUiLayoutProperties(app, check);
    drawUiTextProperty(app, "Text", check.text(), [&](const char* value) { check.setText(value); });
    bool checked = check.checked();
    if (ImGui::Checkbox("Checked", &checked))
        applyInstant(app, "Toggle UI CheckBox", [&] { check.setChecked(checked); });
}

void drawUiSliderProperties(EditorApplication& app, UiSlider& slider)
{
    drawUiLayoutProperties(app, slider);
    float minimum = slider.minimum();
    float maximum = slider.maximum();
    if (dragFloatProperty(app, "Minimum", minimum, 0.05f, "Set UI Slider Minimum"))
        slider.setRange(minimum, slider.maximum());
    if (dragFloatProperty(app, "Maximum", maximum, 0.05f, "Set UI Slider Maximum"))
        slider.setRange(slider.minimum(), maximum);
    float value = slider.value();
    if (dragFloatProperty(app, "Value", value, 0.01f, "Set UI Slider Value", slider.minimum(), slider.maximum()))
        slider.setValue(value);
}

void drawOccluderProperties(EditorApplication& app, LightOccluder2D& occluder)
{
    ImGui::SeparatorText("Points");
    pointListEditor(app, occluder.points(), 2, "Edit Occluder Points",
                    [&](const ct::Vector<Math::Vec2>& pts) { occluder.setPolygon(pts.data(), (int)pts.size()); });
}

void drawPointLightProperties(EditorApplication& app, Light2D& light)
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

        static const char* filterNames[] = {"Nearest", "PCF 5", "PCF 13"};
        int filter = static_cast<int>(light.shadowFilter());
        if (ImGui::Combo("Shadow Filter", &filter, filterNames, 3))
            applyInstant(app, "Set Light Shadow Filter",
                         [&] { light.setShadowFilter(static_cast<ShadowFilter>(filter)); });
    }

    int cullMask = static_cast<int>(light.cullMask());
    if (dragIntProperty(app, "Cull Mask", cullMask, 1.0f, "Change Light Cull Mask", 0, 0x7fffffff))
        light.setCullMask(static_cast<unsigned int>(cullMask));
}

void drawDirectionalLightProperties(EditorApplication& app, DirectionalLight2D& light)
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

        static const char* filterNames[] = {"Nearest", "PCF 5", "PCF 13"};
        int filter = static_cast<int>(light.shadowFilter());
        if (ImGui::Combo("Shadow Filter", &filter, filterNames, 3))
            applyInstant(app, "Set Light Shadow Filter",
                         [&] { light.setShadowFilter(static_cast<ShadowFilter>(filter)); });
    }

    int cullMask = static_cast<int>(light.cullMask());
    if (dragIntProperty(app, "Cull Mask", cullMask, 1.0f, "Change Light Cull Mask", 0, 0x7fffffff))
        light.setCullMask(static_cast<unsigned int>(cullMask));
}

void drawCameraProperties(EditorApplication& app, CameraComponent& cameraComponent)
{
    Scene& scene = app.scene();
    const bool isActive = scene.activeCamera() == &cameraComponent;
    if (isActive)
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), ICON_MDI_CHECK_CIRCLE " Active camera");
    else
        ImGui::TextDisabled("Not the active camera");
    ImGui::SameLine();
    ImGui::BeginDisabled(isActive);
    if (ImGui::Button("Make Active"))
        applyInstant(app, "Make Camera Active", [&] { scene.makeCameraActive(cameraComponent); });
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Scene::activeCamera() picks the enabled camera with the highest render priority.");

    float viewportW = cameraComponent.viewportWidth();
    float viewportH = cameraComponent.viewportHeight();
    bool viewportChanged = false;
    viewportChanged |= dragFloatProperty(app, "Viewport Width", viewportW, 1.0f, "Resize Camera Viewport");
    viewportChanged |= dragFloatProperty(app, "Viewport Height", viewportH, 1.0f, "Resize Camera Viewport");
    if (viewportChanged)
        cameraComponent.setViewport(viewportW, viewportH);

    int renderPriority = cameraComponent.renderPriority();
    if (dragIntProperty(app, "Render Priority", renderPriority, 1.0f,
                        "Higher priority becomes the active camera for the Game view"))
        cameraComponent.setRenderPriority(renderPriority);

    Camera2D& camera = cameraComponent.camera();

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

    if (ImGui::CollapsingHeader("Screen Shake", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("Runtime-only offset in screen pixels; it never changes Position or Offset.");
        Math::Vec2 traumaAmplitude(camera.trauma.amplitudeX, camera.trauma.amplitudeY);
        if (dragVec2(app, "Trauma Amplitude", traumaAmplitude, 0.25f, "Adjust Camera Trauma Amplitude"))
            camera.setTraumaProfile(traumaAmplitude.x, traumaAmplitude.y, camera.trauma.frequency, camera.trauma.decay);
        float traumaFrequency = camera.trauma.frequency;
        if (dragFloatProperty(app, "Trauma Frequency", traumaFrequency, 0.25f, "Adjust Camera Trauma Frequency", 0.0f,
                              240.0f))
            camera.setTraumaProfile(camera.trauma.amplitudeX, camera.trauma.amplitudeY, traumaFrequency,
                                    camera.trauma.decay);
        float traumaDecay = camera.trauma.decay;
        if (dragFloatProperty(app, "Trauma Decay", traumaDecay, 0.05f, "Adjust Camera Trauma Decay", 0.0f, 20.0f))
            camera.setTraumaProfile(camera.trauma.amplitudeX, camera.trauma.amplitudeY, camera.trauma.frequency,
                                    traumaDecay);

        if (ImGui::Button("Preview Shake"))
            camera.startShake(12.0f, 8.0f, 12.0f, 4.0f);
        ImGui::SameLine();
        if (ImGui::Button("Preview Trauma"))
            camera.addTrauma(1.0f);
        ImGui::SameLine();
        if (ImGui::Button("Stop Shake"))
        {
            camera.stopShake();
            camera.clearTrauma();
            camera.stopZoomPunch();
        }
        ImGui::TextDisabled("Trauma %.2f%s", camera.traumaValue(),
                            camera.isZoomPunching() ? "  |  zoom punch active" : "");
    }

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
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodeDragDropPayload))
        {
            const uint64_t draggedId = *static_cast<const uint64_t*>(payload->Data);
            GameObject* dragged = findById(app.scene().root(), draggedId);
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

void drawRigidBodyProperties(EditorApplication& app, RigidBody2D& body)
{
    static const char* kTypes[] = {"Static", "Kinematic", "Dynamic"};
    int typeIndex = body.bodyType() == k2d::BodyType::Static ? 0 : body.bodyType() == k2d::BodyType::Kinematic ? 1 : 2;
    if (ImGui::Combo("Body Type", &typeIndex, kTypes, 3))
    {
        const k2d::BodyType picked = typeIndex == 0   ? k2d::BodyType::Static
                                    : typeIndex == 1 ? k2d::BodyType::Kinematic
                                                     : k2d::BodyType::Dynamic;
        applyInstant(app, "Set Body Type", [&] { body.setBodyType(picked); });
    }

    float density = body.density();
    if (dragFloatProperty(app, "Density", density, 0.05f, "Set Density", 0.0f, 100.0f))
        body.setDensity(density);

    float friction = body.friction();
    if (dragFloatProperty(app, "Friction", friction, 0.01f, "Set Friction", 0.0f, 2.0f))
        body.setFriction(friction);

    float restitution = body.restitution();
    if (dragFloatProperty(app, "Restitution", restitution, 0.01f, "Set Restitution", 0.0f, 1.0f))
        body.setRestitution(restitution);

    float linearDamping = body.linearDamping();
    if (dragFloatProperty(app, "Linear Damping", linearDamping, 0.01f, "Set Linear Damping", 0.0f, 10.0f))
        body.setLinearDamping(linearDamping);

    float angularDamping = body.angularDamping();
    if (dragFloatProperty(app, "Angular Damping", angularDamping, 0.01f, "Set Angular Damping", 0.0f, 10.0f))
        body.setAngularDamping(angularDamping);

    float gravityScale = body.gravityScale();
    if (dragFloatProperty(app, "Gravity Scale", gravityScale, 0.05f, "Set Gravity Scale", -10.0f, 10.0f))
        body.setGravityScale(gravityScale);

    bool fixedRotation = body.fixedRotation();
    if (ImGui::Checkbox("Fixed Rotation", &fixedRotation))
        applyInstant(app, "Set Fixed Rotation", [&] { body.setFixedRotation(fixedRotation); });

    bool bullet = body.bullet();
    if (ImGui::Checkbox("Bullet", &bullet))
        applyInstant(app, "Set Bullet", [&] { body.setBullet(bullet); });
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Continuous collision for fast movers");

    if (body.inWorld())
    {
        const Math::Vec2 velocity = body.velocity();
        ImGui::TextDisabled("Live: v=(%.1f, %.1f)  w=%.1f deg/s", velocity.x, velocity.y, body.angularVelocity());
    }
    else
        ImGui::TextDisabled("Simulated on Play only.");
}

void drawCharacterBodyProperties(EditorApplication& app, CharacterBody2D& body)
{
    ImGui::TextDisabled("Uses the RigidBody2D and Collider2D on this object.");
    Math::Vec2 velocity = body.velocity();
    if (dragVec2(app, "Velocity", velocity, 1.0f, "Set Character Velocity"))
        body.setVelocity(velocity);

    float margin = body.safeMargin();
    if (dragFloatProperty(app, "Safe Margin", margin, 0.05f, "Set Character Safe Margin", 0.0f, 20.0f))
        body.setSafeMargin(margin);
    int maxSlides = body.maxSlides();
    if (ImGui::InputInt("Max Slides", &maxSlides))
        applyInstant(app, "Set Character Max Slides", [&] { body.setMaxSlides(maxSlides); });

    static const char* kModes[] = {"Floating (top-down)", "Grounded (platformer)"};
    int mode = body.motionMode() == CharacterBody2D::MotionMode::Grounded ? 1 : 0;
    if (ImGui::Combo("Motion Mode", &mode, kModes, 2))
        applyInstant(app, "Set Character Motion Mode",
                     [&] {
                         body.setMotionMode(mode == 1 ? CharacterBody2D::MotionMode::Grounded
                                                      : CharacterBody2D::MotionMode::Floating);
                     });
    Math::Vec2 up = body.upDirection();
    if (dragVec2(app, "Up Direction", up, 0.05f, "Set Character Up Direction"))
        body.setUpDirection(up);
    float floorAngle = body.floorMaxAngleDegrees();
    if (dragFloatProperty(app, "Floor Max Angle", floorAngle, 1.0f, "Set Floor Max Angle", 0.0f, 89.0f))
        body.setFloorMaxAngleDegrees(floorAngle);

    ImGui::TextDisabled("Live: floor=%s wall=%s ceiling=%s", body.isOnFloor() ? "yes" : "no",
                        body.isOnWall() ? "yes" : "no", body.isOnCeiling() ? "yes" : "no");
}

void drawColliderShared(EditorApplication& app, Collider2D& collider)
{
    Math::Vec2 offset = collider.offset();
    if (dragVec2(app, "Offset", offset, 0.5f, "Set Collider Offset"))
        collider.setOffset(offset);

    bool sensor = collider.isSensor();
    if (ImGui::Checkbox("Sensor", &sensor))
        applyInstant(app, "Set Collider Sensor", [&] { collider.setSensor(sensor); });
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reports contacts without blocking");

    int category = collider.category();
    int mask = collider.mask();
    bool filterChanged = false;
    if (ImGui::InputInt("Category", &category, 1, 16, ImGuiInputTextFlags_CharsHexadecimal))
        filterChanged = true;
    if (ImGui::InputInt("Mask", &mask, 1, 16, ImGuiInputTextFlags_CharsHexadecimal))
        filterChanged = true;
    if (filterChanged)
    {
        const uint16_t newCategory = static_cast<uint16_t>(category);
        const uint16_t newMask = static_cast<uint16_t>(mask);
        applyInstant(app, "Set Collider Filter", [&] { collider.setFilter(newCategory, newMask); });
    }

    ImGui::Separator();
    GameObject* owner = collider.owner();
    RigidBody2D* body = owner ? owner->getComponent<RigidBody2D>() : nullptr;
    ImGui::BeginDisabled(body == nullptr);
    if (ImGui::Button(ICON_MDI_HAMMER " Build"))
    {
        Scene* scene = owner->scene();
        if (scene)
            scene->buildBodyShapes(*body);
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "Collider built: %d shape(s) on body", body->ShapeCount());
        app.log(buffer);
        app.toasts().success(buffer);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(body ? "Rebuild this body's physics shapes from the current collider data"
                               : "No RigidBody2D on this object - add one to build shapes");
}

void drawBoxColliderProperties(EditorApplication& app, BoxCollider2D& collider)
{
    Math::Vec2 size = collider.size();
    if (dragVec2(app, "Size", size, 0.5f, "Resize Box Collider"))
        collider.setSize(size);
    drawColliderShared(app, collider);
}

void drawCircleColliderProperties(EditorApplication& app, CircleCollider2D& collider)
{
    float radius = collider.radius();
    if (dragFloatProperty(app, "Radius", radius, 0.5f, "Resize Circle Collider", 0.0f, 10000.0f))
        collider.setRadius(radius);
    drawColliderShared(app, collider);
}

void drawEdgeColliderProperties(EditorApplication& app, EdgeCollider2D& collider)
{
    Math::Vec2 start = collider.start();
    Math::Vec2 end = collider.end();
    if (dragVec2(app, "Start", start, 0.5f, "Move Edge Start"))
        collider.setPoints(start, end);
    if (dragVec2(app, "End", end, 0.5f, "Move Edge End"))
        collider.setPoints(start, end);
    drawColliderShared(app, collider);
}

void drawPolygonColliderProperties(EditorApplication& app, PolygonCollider2D& collider)
{
    ImGui::Text("%d points", static_cast<int>(collider.points().size()));
    static int sides = 6;
    static float regularRadius = 32.0f;
    ImGui::SetNextItemWidth(90.0f);
    ImGui::DragInt("Sides", &sides, 1.0f, 3, 16);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::DragFloat("Radius##regular", &regularRadius, 0.5f, 1.0f, 1000.0f);
    ImGui::SameLine();
    if (ImGui::Button("Regular"))
        applyInstant(app, "Set Polygon Collider", [&] { collider.setRegular(sides, regularRadius); });

    pointListEditor(app, collider.points(), 3, "Move Collider Point", [&](const ct::Vector<Math::Vec2>& points)
                    { collider.setPoints(points.data(), static_cast<int>(points.size())); });
    drawColliderShared(app, collider);
}

void drawChainColliderProperties(EditorApplication& app, ChainCollider2D& collider)
{
    ImGui::Text("%d points", static_cast<int>(collider.points().size()));
    bool loop = collider.loop();
    if (ImGui::Checkbox("Loop", &loop))
        applyInstant(app, "Set Chain Loop", [&] { collider.setLoop(loop); });

    pointListEditor(app, collider.points(), 2, "Move Collider Point", [&](const ct::Vector<Math::Vec2>& points)
                    { collider.setPoints(points.data(), static_cast<int>(points.size())); });
    drawColliderShared(app, collider);
}

void drawTileMapColliderProperties(EditorApplication& app, TileMapCollider2D& collider)
{
    ImGui::TextDisabled("Adds a box per merged run of solid cells from the sibling TileMapComponent.");
    drawColliderShared(app, collider);
}

void drawJointShared(EditorApplication& app, Joint2D& joint)
{
    char nameBuffer[128];
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", joint.targetName().c_str());
    if (ImGui::InputText("Target Object", nameBuffer, sizeof(nameBuffer)))
        applyInstant(app, "Set Joint Target", [&] { joint.setTargetName(nameBuffer); });
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodeDragDropPayload))
        {
            const uint64_t draggedId = *static_cast<const uint64_t*>(payload->Data);
            if (GameObject* dragged = findById(app.scene().root(), draggedId))
            {
                if (dragged->getComponent<RigidBody2D>())
                    applyInstant(app, "Set Joint Target", [&] { joint.setTargetName(dragged->name().c_str()); });
                else
                    rejectJointDrop(&joint, dragged->name(), "has no RigidBody2D - drop rejected");
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Drag a GameObject here from Hierarchy, or type its name. Resolved once when Play starts.");
    if (joint.targetName().empty())
        ImGui::TextDisabled("Drop a GameObject here");
    drawJointDropFeedback(&joint);

    bool collideConnected = joint.collideConnected();
    if (ImGui::Checkbox("Collide Connected", &collideConnected))
        applyInstant(app, "Set Joint Collide Connected", [&] { joint.setCollideConnected(collideConnected); });

    ImGui::TextDisabled(joint.isConnected() ? "Connected" : "Not connected (Play only)");
}

void drawDistanceJointProperties(EditorApplication& app, DistanceJoint2D& joint)
{
    Math::Vec2 anchorA = joint.localAnchorA();
    if (dragVec2(app, "Local Anchor A", anchorA, 0.5f, "Set Distance Joint Anchor A"))
        joint.setLocalAnchorA(anchorA);
    Math::Vec2 anchorB = joint.localAnchorB();
    if (dragVec2(app, "Local Anchor B", anchorB, 0.5f, "Set Distance Joint Anchor B"))
        joint.setLocalAnchorB(anchorB);

    float length = joint.length();
    if (dragFloatProperty(app, "Length", length, 0.5f, "Set Distance Joint Length", 0.0f, 100000.0f))
        joint.setLength(length);
    if (!joint.lengthConfigured())
        ImGui::TextDisabled("Length will be derived from the current gap between the bodies when Play connects this joint.");

    float minLength = joint.minLength();
    float maxLength = joint.maxLength();
    bool rangeChanged = false;
    rangeChanged |= dragFloatProperty(app, "Min Length", minLength, 0.5f, "Set Distance Joint Min Length", 0.0f, 100000.0f);
    rangeChanged |= dragFloatProperty(app, "Max Length", maxLength, 0.5f, "Set Distance Joint Max Length", 0.0f, 100000.0f);
    if (rangeChanged)
        applyInstant(app, "Set Distance Joint Length Range", [&] { joint.setLengthRange(minLength, maxLength); });

    float frequency = joint.springFrequency();
    float damping = joint.springDamping();
    bool springChanged = false;
    springChanged |= dragFloatProperty(app, "Spring Frequency", frequency, 0.05f, "Set Distance Joint Spring", 0.0f, 60.0f);
    springChanged |= dragFloatProperty(app, "Spring Damping", damping, 0.01f, "Set Distance Joint Spring", 0.0f, 5.0f);
    if (springChanged)
        applyInstant(app, "Set Distance Joint Spring", [&] { joint.setSpring(frequency, damping); });

    drawJointShared(app, joint);
}

void drawRevoluteJointProperties(EditorApplication& app, RevoluteJoint2D& joint)
{
    Math::Vec2 anchorA = joint.localAnchorA();
    if (dragVec2(app, "Local Anchor A", anchorA, 0.5f, "Set Revolute Joint Anchor A"))
    {
        joint.setLocalAnchorA(anchorA);
        joint.setAnchorsConfigured(true);
    }
    Math::Vec2 anchorB = joint.localAnchorB();
    if (dragVec2(app, "Local Anchor B", anchorB, 0.5f, "Set Revolute Joint Anchor B"))
    {
        joint.setLocalAnchorB(anchorB);
        joint.setAnchorsConfigured(true);
    }
    if (!joint.anchorsConfigured())
        ImGui::TextDisabled("Anchor B will be derived from Anchor A's position when Play connects this joint.");
    float referenceAngle = joint.referenceAngle();
    if (dragFloatProperty(app, "Reference Angle", referenceAngle, 0.01f, "Set Revolute Joint Reference Angle", -6.3f, 6.3f))
        joint.setReferenceAngle(referenceAngle);

    bool motorEnabled = joint.motorEnabled();
    float motorSpeed = joint.motorSpeed();
    float maxMotorTorque = joint.maxMotorTorque();
    bool motorChanged = ImGui::Checkbox("Motor Enabled", &motorEnabled);
    motorChanged |= dragFloatProperty(app, "Motor Speed", motorSpeed, 0.05f, "Set Revolute Joint Motor", -1000.0f, 1000.0f);
    motorChanged |= dragFloatProperty(app, "Max Motor Torque", maxMotorTorque, 1.0f, "Set Revolute Joint Motor", 0.0f, 1.0e7f);
    if (motorChanged)
        applyInstant(app, "Set Revolute Joint Motor", [&] { joint.setMotor(motorEnabled, motorSpeed, maxMotorTorque); });

    bool limitEnabled = joint.limitEnabled();
    float lowerAngle = joint.lowerAngle();
    float upperAngle = joint.upperAngle();
    bool limitChanged = ImGui::Checkbox("Limit Enabled", &limitEnabled);
    limitChanged |= dragFloatProperty(app, "Lower Angle", lowerAngle, 0.01f, "Set Revolute Joint Limits", -6.3f, 6.3f);
    limitChanged |= dragFloatProperty(app, "Upper Angle", upperAngle, 0.01f, "Set Revolute Joint Limits", -6.3f, 6.3f);
    if (limitChanged)
        applyInstant(app, "Set Revolute Joint Limits", [&] { joint.setLimits(limitEnabled, lowerAngle, upperAngle); });

    drawJointShared(app, joint);
}

void drawWheelJointProperties(EditorApplication& app, WheelJoint2D& joint)
{
    Math::Vec2 anchorA = joint.localAnchorA();
    if (dragVec2(app, "Local Anchor A", anchorA, 0.5f, "Set Wheel Joint Anchor A"))
        joint.setLocalAnchorA(anchorA);
    Math::Vec2 anchorB = joint.localAnchorB();
    if (dragVec2(app, "Local Anchor B", anchorB, 0.5f, "Set Wheel Joint Anchor B"))
        joint.setLocalAnchorB(anchorB);
    if (!joint.anchorsConfigured())
        ImGui::TextDisabled("Anchor B is derived from the current placement when Play connects this joint.");
    Math::Vec2 axis = joint.localAxisA();
    if (dragVec2(app, "Local Axis A", axis, 0.05f, "Set Wheel Joint Axis"))
        joint.setLocalAxisA(axis);

    bool motorEnabled = joint.motorEnabled();
    float motorSpeed = joint.motorSpeed();
    float maxMotorTorque = joint.maxMotorTorque();
    bool motorChanged = ImGui::Checkbox("Motor Enabled", &motorEnabled);
    motorChanged |= dragFloatProperty(app, "Motor Speed", motorSpeed, 0.05f, "Set Wheel Joint Motor", -1000.0f, 1000.0f);
    motorChanged |= dragFloatProperty(app, "Max Motor Torque", maxMotorTorque, 1.0f, "Set Wheel Joint Motor", 0.0f, 1.0e7f);
    if (motorChanged)
        applyInstant(app, "Set Wheel Joint Motor", [&] { joint.setMotor(motorEnabled, motorSpeed, maxMotorTorque); });

    float frequency = joint.springFrequency();
    float damping = joint.springDamping();
    bool springChanged = false;
    springChanged |= dragFloatProperty(app, "Spring Frequency", frequency, 0.05f, "Set Wheel Joint Spring", 0.0f, 60.0f);
    springChanged |= dragFloatProperty(app, "Spring Damping", damping, 0.01f, "Set Wheel Joint Spring", 0.0f, 5.0f);
    if (springChanged)
        applyInstant(app, "Set Wheel Joint Spring", [&] { joint.setSpring(frequency, damping); });

    drawJointShared(app, joint);
}

void drawMotorJointProperties(EditorApplication& app, MotorJoint2D& joint)
{
    Math::Vec2 linearOffset = joint.linearOffset();
    if (dragVec2(app, "Linear Offset", linearOffset, 0.5f, "Set Motor Joint Linear Offset"))
        joint.setLinearOffset(linearOffset);
    float angularOffset = joint.angularOffset();
    if (dragFloatProperty(app, "Angular Offset", angularOffset, 0.01f, "Set Motor Joint Angular Offset", -6.3f, 6.3f))
        joint.setAngularOffset(angularOffset);
    float maxForce = joint.maxForce();
    if (dragFloatProperty(app, "Max Force", maxForce, 1.0f, "Set Motor Joint Max Force", 0.0f, 1.0e7f))
        joint.setMaxForce(maxForce);
    float maxTorque = joint.maxTorque();
    if (dragFloatProperty(app, "Max Torque", maxTorque, 1.0f, "Set Motor Joint Max Torque", 0.0f, 1.0e7f))
        joint.setMaxTorque(maxTorque);
    float correction = joint.correctionFactor();
    if (dragFloatProperty(app, "Correction Factor", correction, 0.01f, "Set Motor Joint Correction Factor", 0.0f, 1.0f))
        joint.setCorrectionFactor(correction);

    drawJointShared(app, joint);
}

void drawMouseJointProperties(EditorApplication& app, MouseJoint2D& joint)
{
    ImGui::TextDisabled("Pulls this object's RigidBody2D toward Target. No target object needed.");
    Math::Vec2 target = joint.target();
    if (dragVec2(app, "Target", target, 0.5f, "Set Mouse Joint Target"))
        joint.setTarget(target);
    float maxForce = joint.maxForce();
    if (dragFloatProperty(app, "Max Force", maxForce, 1.0f, "Set Mouse Joint Max Force", 0.0f, 1.0e7f))
        joint.setMaxForce(maxForce);

    float frequency = joint.springFrequency();
    float damping = joint.springDamping();
    bool springChanged = false;
    springChanged |= dragFloatProperty(app, "Spring Frequency", frequency, 0.05f, "Set Mouse Joint Spring", 0.0f, 60.0f);
    springChanged |= dragFloatProperty(app, "Spring Damping", damping, 0.01f, "Set Mouse Joint Spring", 0.0f, 5.0f);
    if (springChanged)
        applyInstant(app, "Set Mouse Joint Spring", [&] { joint.setSpring(frequency, damping); });

    bool collideConnected = joint.collideConnected();
    if (ImGui::Checkbox("Collide Connected", &collideConnected))
        applyInstant(app, "Set Joint Collide Connected", [&] { joint.setCollideConnected(collideConnected); });
}

void drawGearJointProperties(EditorApplication& app, GearJoint2D& joint)
{
    char nameA[128];
    std::snprintf(nameA, sizeof(nameA), "%s", joint.jointATargetName().c_str());
    int indexA = joint.jointAIndex();
    bool changedA = ImGui::InputText("Joint A Object", nameA, sizeof(nameA));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodeDragDropPayload))
        {
            const uint64_t draggedId = *static_cast<const uint64_t*>(payload->Data);
            if (GameObject* dragged = findById(app.scene().root(), draggedId))
            {
                if (dragged->getComponent<RevoluteJoint2D>())
                {
                    std::snprintf(nameA, sizeof(nameA), "%s", dragged->name().c_str());
                    changedA = true;
                }
                else
                {
                    rejectJointDrop(&joint, dragged->name(), "has no RevoluteJoint2D - drop rejected");
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (nameA[0] == '\0')
        ImGui::TextDisabled("Drop a GameObject with a RevoluteJoint2D here");
    changedA |= ImGui::InputInt("Joint A Index", &indexA);
    if (changedA)
        applyInstant(app, "Set Gear Joint A", [&] { joint.setJointA(nameA, indexA); });

    char nameB[128];
    std::snprintf(nameB, sizeof(nameB), "%s", joint.jointBTargetName().c_str());
    int indexB = joint.jointBIndex();
    bool changedB = ImGui::InputText("Joint B Object", nameB, sizeof(nameB));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodeDragDropPayload))
        {
            const uint64_t draggedId = *static_cast<const uint64_t*>(payload->Data);
            if (GameObject* dragged = findById(app.scene().root(), draggedId))
            {
                if (dragged->getComponent<RevoluteJoint2D>())
                {
                    std::snprintf(nameB, sizeof(nameB), "%s", dragged->name().c_str());
                    changedB = true;
                }
                else
                {
                    rejectJointDrop(&joint, dragged->name(), "has no RevoluteJoint2D - drop rejected");
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (nameB[0] == '\0')
        ImGui::TextDisabled("Drop a GameObject with a RevoluteJoint2D here");
    changedB |= ImGui::InputInt("Joint B Index", &indexB);
    if (changedB)
        applyInstant(app, "Set Gear Joint B", [&] { joint.setJointB(nameB, indexB); });

    ImGui::TextDisabled("Each names a GameObject and which RevoluteJoint2D on it (0-based)");
    drawJointDropFeedback(&joint);

    float ratio = joint.ratio();
    if (dragFloatProperty(app, "Ratio", ratio, 0.05f, "Set Gear Joint Ratio", -100.0f, 100.0f))
        joint.setRatio(ratio);

    bool collideConnected = joint.collideConnected();
    if (ImGui::Checkbox("Collide Connected", &collideConnected))
        applyInstant(app, "Set Joint Collide Connected", [&] { joint.setCollideConnected(collideConnected); });

    ImGui::TextDisabled(joint.isConnected() ? "Connected" : "Not connected (Play only)");
}

void drawZenScriptOverrides(EditorApplication& app, ZenScriptComponent& script)
{
    const size_t declaredCount = script.declaredPropertyCount();
    if (declaredCount == 0 && script.overrideCount() == 0)
        return;

    if (!ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    for (size_t i = 0; i < declaredCount; ++i)
    {
        const ZenScriptProperty* declared = script.declaredPropertyAt(i);
        const ZenScriptProperty* current = script.findOverride(declared->name.c_str());
        const ZenScriptProperty& value = current ? *current : *declared;
        const char* name = declared->name.c_str();

        ImGui::PushID(static_cast<int>(i));
        ImGui::SetNextItemWidth(-60.0f);

        if (declared->kind == ZenScriptProperty::Kind::Bool)
        {
            bool flag = value.flag;
            if (ImGui::Checkbox(name, &flag))
                applyInstant(app, "Set Script Property", [&] { script.setBoolOverride(name, flag); });
        }
        else if (declared->kind == ZenScriptProperty::Kind::String)
        {
            char text[256];
            size_t length = value.text.size();
            if (length >= sizeof(text))
                length = sizeof(text) - 1;
            for (size_t c = 0; c < length; ++c)
                text[c] = value.text[c];
            text[length] = '\0';

            const EditorApplication::SceneChange before = app.beginChange();
            const bool changed = ImGui::InputText(name, text, sizeof(text));
            if (ImGui::IsItemActivated())
                app.beginTransaction("Set Script Property", before);
            if (changed)
                script.setStringOverride(name, text);
            if (ImGui::IsItemDeactivatedAfterEdit())
                app.commitTransaction();
        }
        else if (declared->integer)
        {
            int number = static_cast<int>(value.number);
            const EditorApplication::SceneChange before = app.beginChange();
            const bool changed = ImGui::DragInt(name, &number, 1.0f);
            if (ImGui::IsItemActivated())
                app.beginTransaction("Set Script Property", before);
            if (changed)
                script.setNumberOverride(name, static_cast<double>(number), true);
            if (ImGui::IsItemDeactivatedAfterEdit())
                app.commitTransaction();
        }
        else
        {
            float number = static_cast<float>(value.number);
            const EditorApplication::SceneChange before = app.beginChange();
            const bool changed = ImGui::DragFloat(name, &number, 0.1f);
            if (ImGui::IsItemActivated())
                app.beginTransaction("Set Script Property", before);
            if (changed)
                script.setNumberOverride(name, static_cast<double>(number));
            if (ImGui::IsItemDeactivatedAfterEdit())
                app.commitTransaction();
        }

        if (current)
        {
            ImGui::SameLine();
            if (ImGui::Button(ICON_MDI_RESTORE "##revert", ImVec2(26.0f, 0.0f)))
                applyInstant(app, "Revert Script Property", [&] { script.clearOverride(name); });
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Back to the script default");
        }
        ImGui::PopID();
    }

    ct::String orphan;
    for (size_t i = 0; i < script.overrideCount(); ++i)
    {
        const ZenScriptProperty* stored = script.overrideAt(i);
        if (script.declaredProperty(stored->name.c_str()))
            continue;
        ImGui::PushID(static_cast<int>(declaredCount + i));
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), ICON_MDI_ALERT " %s", stored->name.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Drop"))
            orphan = stored->name;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The script no longer declares this field");
        ImGui::PopID();
    }
    if (!orphan.empty())
        applyInstant(app, "Drop Script Property", [&] { script.clearOverride(orphan.c_str()); });

    if (script.overrideCount() > 0)
    {
        if (ImGui::Button(ICON_MDI_RESTORE " Reset All"))
            applyInstant(app, "Reset Script Properties", [&] { script.clearOverrides(); });
    }
}

void drawZenScriptProperties(EditorApplication& app, ZenScriptComponent& script)
{
    ImGui::TextUnformatted("Script");
    ImGui::SameLine(130.0f);
    const bool hasPath = !script.scriptPath().empty();
    ImGui::Button(hasPath ? script.scriptPath().c_str() : "None", ImVec2(-70.0f, 0.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kScriptDragDropPayload))
        {
            const char* path = static_cast<const char*>(payload->Data);
            applyInstant(app, "Set Zen Script", [&] { script.loadFile(path); });
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Drag a .py file here from Assets");
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasPath);
    if (ImGui::Button(ICON_MDI_RESTART " Reload"))
    {
        const ct::String path = script.scriptPath();
        if (ZenRuntime::instance().recompile(path.c_str()))
            app.toasts().info("Script recompiled");
        else if (script.loadFile(path.c_str()))
            app.toasts().info("Script loaded");
        else
            app.toasts().error("Script reload failed - check the console output");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Recompiles the file for every object using it");
    ImGui::EndDisabled();

    if (script.loaded())
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.4f, 1.0f), "Loaded");
    else if (script.pendingLoad())
        ImGui::TextColored(ImVec4(0.85f, 0.8f, 0.4f, 1.0f), "Compiles on the next frame");
    else if (hasPath)
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.3f, 1.0f), "Failed to load - check the console output");
    else
        ImGui::TextDisabled("No script assigned.");

    drawZenScriptOverrides(app, script);

    ImGui::TextDisabled("Contract: class with __init__(self, node) and on_update(self, dt)");
    ImGui::TextDisabled("Scripts run in Play mode only.");
}

void drawSpriteBatchProperties(EditorApplication& app, SpriteBatch& batch)
{
    BlendMode blendMode = batch.blendMode();
    if (blendModeCombo(blendMode))
        applyInstant(app, "Set SpriteBatch Blend Mode", [&] { batch.setBlendMode(blendMode); });

    ImGui::Text("%d entries", batch.count());
    int removeIndex = -1;
    for (int i = 0; i < batch.count(); ++i)
    {
        SpriteBatch::Entry* entry = batch.entryAt(i);
        if (!entry)
            continue;
        ImGui::PushID(i);
        if (ImGui::TreeNodeEx("##entry", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth, "Entry %d", i))
        {
            ImGui::Indent();
            Texture* newTexture = nullptr;
            if (textureField(app, "Texture", entry->texture, newTexture))
                applyInstant(app, "Set Batch Entry Texture", [&] { entry->texture = newTexture; });
            if (dragVec2(app, "Position", entry->position, 0.5f, "Move Batch Entry"))
            {
            }
            if (dragVec2(app, "Size", entry->size, 0.5f, "Resize Batch Entry"))
            {
            }
            if (dragVec4(app, "Source (x,y,w,h)", entry->source, 0.5f, "Adjust Batch Entry Source"))
            {
            }
            if (colorEdit(app, "Color", entry->color, "Recolor Batch Entry"))
            {
            }

            bool flipX = (entry->flags & 0x1u) != 0;
            bool flipY = (entry->flags & 0x2u) != 0;
            if (ImGui::Checkbox("Flip X", &flipX))
                applyInstant(app, "Flip Batch Entry X", [&] { batch.setFlip(i, flipX, flipY); });
            ImGui::SameLine();
            if (ImGui::Checkbox("Flip Y", &flipY))
                applyInstant(app, "Flip Batch Entry Y", [&] { batch.setFlip(i, flipX, flipY); });

            if (ImGui::Button(ICON_MDI_DELETE " Remove"))
                removeIndex = i;
            ImGui::Unindent();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removeIndex >= 0)
        applyInstant(app, "Remove Batch Entry", [&] { batch.remove(removeIndex); });
    if (ImGui::Button(ICON_MDI_PLUS " Add Entry"))
    {
        applyInstant(app, "Add Batch Entry", [&]
                     { batch.add(placeholderSpriteTexture(app), Math::Vec2(0.0f, 0.0f), Math::Vec2(32.0f, 32.0f)); });
    }
}

void drawAnimationProperties(EditorApplication& app, Animation2D& anim)
{
    static const char* modeNames[] = {"One Shot", "Loop", "Ping Pong"};
    GameObject* owner = anim.owner();
    if (!owner || !owner->getComponent<SpriteComponent>())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f),
                           "Animation2D requires a Sprite Renderer to display frames.");
        if (ImGui::Button("Add Sprite Renderer"))
            applyInstant(app, "Add Sprite Renderer",
                         [&]
                         {
                             if (owner && !owner->getComponent<SpriteComponent>())
                                 owner->addComponent<SpriteComponent>();
                         });
    }
    int mode = static_cast<int>(anim.mode());
    if (ImGui::Combo("Mode", &mode, modeNames, 3))
        applyInstant(app, "Set Animation Mode", [&] { anim.setMode(static_cast<AnimationMode>(mode)); });

    if (ImGui::Button(anim.playing() ? "Stop Preview" : "Preview in Scene"))
    {
        applyInstant(app, anim.playing() ? "Stop Animation" : "Play Animation",
                     [&]
                     {
                         if (anim.playing())
                             anim.stop();
                         else
                         {
                             app.settings().viewportLivePreview = true;
                             anim.play();
                         }
                     });
    }
    ImGui::SameLine();
    ImGui::Text("Frame %d / %d", anim.frame(), anim.frameCount());
    ImGui::TextDisabled("Edit the sprite library, clips and timeline in Window > Animator.");
    return;

    ImGui::Text("%d clip(s)%s%s", static_cast<int>(anim.clipCount()), anim.currentClip() ? ", current: " : "",
                anim.currentClip() ? anim.currentClip() : "");
    ct::String clipToDelete;
    for (size_t i = 0; i < anim.clipCount(); ++i)
    {
        AnimationClip* clip = anim.clipAt(i);
        if (!clip)
            continue;
        ImGui::PushID(static_cast<int>(i));
        const bool isCurrent = anim.currentClip() && clip->name == anim.currentClip();
        const bool open = ImGui::TreeNodeEx("##clip", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth,
                                            "%s%s", clip->name.c_str(), isCurrent ? "  (current)" : "");
        if (open)
        {
            ImGui::Indent();
            if (!isCurrent && ImGui::Button("Set Active"))
                applyInstant(app, "Change Animation Clip", [&] { anim.play(clip->name.c_str()); });

            const ct::String clipName = clip->name;
            const bool individualFrames = !clip->frames.empty() || (!clip->texture && clip->frameWidth == 0 &&
                                                                    clip->frameHeight == 0 && clip->frameCount == 0);
            if (individualFrames)
            {
                float fps = clip->framesPerSecond;
                if (dragFloatProperty(app, "FPS", fps, 0.5f, "Edit Animation Clip", 0.0f, 240.0f))
                    clip->framesPerSecond = fps;
                int modeIdx = static_cast<int>(clip->mode);
                if (ImGui::Combo("Mode", &modeIdx, modeNames, 3))
                    applyInstant(app, "Set Animation Clip Mode",
                                 [&]
                                 {
                                     AnimationClip* editable = anim.clipAt(i);
                                     if (editable)
                                         editable->mode = static_cast<AnimationMode>(modeIdx);
                                 });

                ImGui::SeparatorText("Frames");
                ImGui::Button("Drop a Sprite Editor region here", ImVec2(-1.0f, 28.0f));
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kSpriteRegionDragDropPayload))
                    {
                        const auto* data = static_cast<const SpriteRegionDragDropData*>(payload->Data);
                        const Math::Vec4 rect(data->x, data->y, data->width, data->height);
                        applyInstant(app, "Add Sprite Editor Frame",
                                     [&]
                                     {
                                         anim.addFrame(clipName.c_str(), data->texture, rect, data->texturePath);
                                         app.settings().viewportLivePreview = true;
                                         anim.play(clipName.c_str());
                                     });
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::TextDisabled("Drag named regions from Sprite Editor to build this clip.");
                size_t frameToDelete = static_cast<size_t>(-1);
                for (size_t frameIndex = 0; frameIndex < clip->frames.size(); ++frameIndex)
                {
                    const AnimationFrame& frame = clip->frames[frameIndex];
                    ImGui::PushID(static_cast<int>(frameIndex));
                    ImGui::Text("Frame %d", static_cast<int>(frameIndex) + 1);
                    Texture* frameTexture = frame.texture;
                    Texture* droppedFrameTexture = nullptr;
                    if (textureField(app, "Texture", frameTexture, droppedFrameTexture))
                    {
                        applyInstant(app, "Set Animation Frame Texture",
                                     [&]
                                     {
                                         anim.setFrame(clipName.c_str(), frameIndex, droppedFrameTexture, frame.rect,
                                                       app.assets().FindTextureName(droppedFrameTexture));
                                     });
                    }
                    Math::Vec4 rect = frame.rect;
                    if (dragVec4(app, "Rect (0 = image)", rect, 0.5f, "Edit Animation Frame"))
                        anim.setFrame(clipName.c_str(), frameIndex, frameTexture, rect);
                    ImGui::SameLine();
                    if (ImGui::SmallButton(ICON_MDI_DELETE))
                        frameToDelete = frameIndex;
                    ImGui::PopID();
                }
                if (frameToDelete != static_cast<size_t>(-1))
                    applyInstant(app, "Delete Animation Frame",
                                 [&] { anim.removeFrame(clipName.c_str(), frameToDelete); });

                static Texture* newFrameTexture = nullptr;
                static Math::Vec4 newFrameRect(0.0f);
                ImGui::SeparatorText("Add Frame");
                Texture* droppedNewFrameTexture = nullptr;
                if (textureField(app, "Frame Texture", newFrameTexture, droppedNewFrameTexture))
                    newFrameTexture = droppedNewFrameTexture;
                ImGui::DragFloat4("Frame Rect (0 = image)", &newFrameRect.x, 0.5f);
                if (ImGui::Button(ICON_MDI_PLUS " Add Frame"))
                {
                    applyInstant(app, "Add Animation Frame",
                                 [&]
                                 {
                                     anim.addFrame(clipName.c_str(), newFrameTexture, newFrameRect);
                                     app.settings().viewportLivePreview = true;
                                     anim.play(clipName.c_str());
                                 });
                    newFrameTexture = nullptr;
                    newFrameRect = Math::Vec4(0.0f);
                }
                if (ImGui::Button(ICON_MDI_DELETE " Delete Clip"))
                    clipToDelete = clipName;
                ImGui::Unindent();
                ImGui::TreePop();
                ImGui::PopID();
                continue;
            }
            const bool wasPlaying = clip->playing;
            Texture* clipTexture = clip->texture;
            int fw = clip->frameWidth;
            int fh = clip->frameHeight;
            int fc = clip->frameCount;
            Math::Vec2 atlasPadding = clip->atlasPadding;
            Math::Vec2 atlasGap = clip->atlasGap;
            float fps = clip->framesPerSecond;
            int modeIdx = static_cast<int>(clip->mode);

            const auto updateClip = [&]
            {
                anim.addClip(clipName.c_str(), clipTexture, fw, fh, fc, fps, static_cast<AnimationMode>(modeIdx));
                anim.setClipAtlasLayout(clipName.c_str(), atlasPadding, atlasGap);
                if (isCurrent && wasPlaying)
                    anim.play(clipName.c_str());
            };

            Texture* droppedClipTexture = nullptr;
            if (textureField(app, "Texture", clipTexture, droppedClipTexture))
            {
                clipTexture = droppedClipTexture;
                applyInstant(app, "Set Animation Clip Texture", updateClip);
            }
            if (dragIntProperty(app, "Frame Width", fw, 1.0f, "Edit Animation Clip", 1, 4096))
                updateClip();
            if (dragIntProperty(app, "Frame Height", fh, 1.0f, "Edit Animation Clip", 1, 4096))
                updateClip();
            if (dragIntProperty(app, "Frame Count", fc, 1.0f, "Edit Animation Clip", 1, 4096))
                updateClip();
            if (dragVec2(app, "Atlas Padding", atlasPadding, 0.5f, "Edit Animation Clip"))
                updateClip();
            if (dragVec2(app, "Atlas Gap", atlasGap, 0.5f, "Edit Animation Clip"))
                updateClip();
            if (dragFloatProperty(app, "FPS", fps, 0.5f, "Edit Animation Clip", 0.0f, 240.0f))
                updateClip();
            if (ImGui::Combo("Mode", &modeIdx, modeNames, 3))
                applyInstant(app, "Set Animation Clip Mode", updateClip);
            if (ImGui::Button("Use Individual Frames"))
                applyInstant(app, "Convert Animation Clip to Frames",
                             [&] { anim.addFrame(clipName.c_str(), clipTexture, Math::Vec4(0.0f)); });
            if (ImGui::Button(ICON_MDI_DELETE " Delete Clip"))
                clipToDelete = clipName;
            ImGui::Unindent();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (!clipToDelete.empty())
        applyInstant(app, "Delete Animation Clip", [&] { anim.removeClip(clipToDelete.c_str()); });

    static char clipName[64] = "default";
    static float framesPerSecond = 10.0f;
    static int clipMode = 1;

    if (ImGui::TreeNodeEx(
            "##newClip", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen,
            ICON_MDI_PLAYLIST_PLUS " New Clip"))
    {
        ImGui::Indent();
        ImGui::InputText("Name", clipName, sizeof(clipName));
        ImGui::DragFloat("FPS", &framesPerSecond, 0.5f, 0.0f, 240.0f);
        ImGui::Combo("Mode", &clipMode, modeNames, 3);

        const bool valid = clipName[0] != '\0';
        ImGui::BeginDisabled(!valid);
        if (ImGui::Button("Add Clip"))
        {
            applyInstant(app, "Add Animation Clip",
                         [&]
                         {
                             anim.addClip(clipName, nullptr, 0, 0, 0, framesPerSecond,
                                          static_cast<AnimationMode>(clipMode));
                             anim.play(clipName);
                         });
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("Then drag regions from Sprite Editor into its Frames area.");
        ImGui::Unindent();
        ImGui::TreePop();
    }
}

enum class ParticlePreset
{
    Fire,
    Smoke,
    Explosion,
    EngineFlame,
    Rain,
    Snow
};

void applyParticlePreset(EditorApplication& app, ParticleComponent& particleComponent, ParticlePreset preset)
{
    ParticleSystem& system = particleComponent.system();
    if (!system.GetTexture())
        system.SetTexture(app.particlePlaceholderTexture());
    ParticlePrefab prefab;

    switch (preset)
    {
    case ParticlePreset::Fire:
        system.SetMode(ParticleMode::Loop);
        system.SetEmissionRate(30.0f);
        system.SetGravity(Math::Vec2(0.0f, -20.0f));
        particleComponent.setBlendMode(BLEND_ADD);
        prefab.direction = Math::Vec2(0.0f, -1.0f);
        prefab.spreadDegrees = 20.0f;
        prefab.speedMin = 30.0f;
        prefab.speedMax = 60.0f;
        prefab.lifeMin = 0.6f;
        prefab.lifeMax = 1.2f;
        prefab.sizeMin = 10.0f;
        prefab.sizeMax = 18.0f;
        prefab.endSize = 2.0f;
        prefab.rotationMin = 0.0f;
        prefab.rotationMax = 360.0f;
        prefab.angularVelocityMin = -60.0f;
        prefab.angularVelocityMax = 60.0f;
        prefab.drag = 0.5f;
        prefab.fadeIn = 0.05f;
        prefab.fadeOut = 0.4f;
        prefab.colorStart = Color(1.0f, 0.9f, 0.3f, 1.0f);
        prefab.colorEnd = Color(0.6f, 0.1f, 0.05f, 0.0f);
        break;
    case ParticlePreset::Smoke:
        system.SetMode(ParticleMode::Loop);
        system.SetEmissionRate(10.0f);
        system.SetGravity(Math::Vec2(0.0f, -8.0f));
        particleComponent.setBlendMode(BLEND_MIX);
        prefab.direction = Math::Vec2(0.0f, -1.0f);
        prefab.spreadDegrees = 50.0f;
        prefab.speedMin = 10.0f;
        prefab.speedMax = 25.0f;
        prefab.lifeMin = 2.0f;
        prefab.lifeMax = 4.0f;
        prefab.sizeMin = 6.0f;
        prefab.sizeMax = 12.0f;
        prefab.endSize = 40.0f;
        prefab.rotationMin = 0.0f;
        prefab.rotationMax = 360.0f;
        prefab.angularVelocityMin = -20.0f;
        prefab.angularVelocityMax = 20.0f;
        prefab.drag = 0.3f;
        prefab.fadeIn = 0.3f;
        prefab.fadeOut = 1.5f;
        prefab.colorStart = Color(0.7f, 0.7f, 0.7f, 0.5f);
        prefab.colorEnd = Color(0.3f, 0.3f, 0.3f, 0.0f);
        break;
    case ParticlePreset::Explosion:
        system.SetMode(ParticleMode::OneShot);
        system.SetOneShotCount(40);
        system.SetEmitterShape(ParticleEmitterShape::Point);
        system.SetGravity(Math::Vec2(0.0f, 40.0f));
        particleComponent.setBlendMode(BLEND_ADD);
        prefab.direction = Math::Vec2(0.0f, -1.0f);
        prefab.spreadDegrees = 360.0f;
        prefab.speedMin = 100.0f;
        prefab.speedMax = 260.0f;
        prefab.lifeMin = 0.3f;
        prefab.lifeMax = 0.7f;
        prefab.sizeMin = 12.0f;
        prefab.sizeMax = 24.0f;
        prefab.endSize = 0.0f;
        prefab.rotationMin = 0.0f;
        prefab.rotationMax = 360.0f;
        prefab.angularVelocityMin = -180.0f;
        prefab.angularVelocityMax = 180.0f;
        prefab.drag = 2.0f;
        prefab.fadeIn = 0.0f;
        prefab.fadeOut = 0.3f;
        prefab.colorStart = Color(1.0f, 1.0f, 0.8f, 1.0f);
        prefab.colorEnd = Color(1.0f, 0.3f, 0.05f, 0.0f);
        break;
    case ParticlePreset::EngineFlame:
        system.SetMode(ParticleMode::Loop);
        system.SetEmissionRate(90.0f);
        system.SetEmitterShape(ParticleEmitterShape::Point);
        system.SetGravity(Math::Vec2(0.0f, 0.0f));
        particleComponent.setBlendMode(BLEND_ADD);
        prefab.direction = Math::Vec2(0.0f, 1.0f);
        prefab.spreadDegrees = 14.0f;
        prefab.speedMin = 140.0f;
        prefab.speedMax = 260.0f;
        prefab.lifeMin = 0.15f;
        prefab.lifeMax = 0.4f;
        prefab.sizeMin = 10.0f;
        prefab.sizeMax = 16.0f;
        prefab.endSize = 3.0f;
        prefab.rotationMin = 0.0f;
        prefab.rotationMax = 360.0f;
        prefab.angularVelocityMin = -90.0f;
        prefab.angularVelocityMax = 90.0f;
        prefab.drag = 1.5f;
        prefab.fadeIn = 0.02f;
        prefab.fadeOut = 0.12f;
        prefab.colorStart = Color(1.0f, 0.95f, 0.6f, 1.0f);
        prefab.colorEnd = Color(1.0f, 0.35f, 0.05f, 0.0f);
        break;
    case ParticlePreset::Rain:
        system.SetMode(ParticleMode::Loop);
        system.SetEmissionRate(220.0f);
        if (system.Capacity() < 600)
            system.SetCapacity(600);
        system.SetEmitterShape(ParticleEmitterShape::Rectangle);
        system.SetEmitterSize(Math::Vec2(300.0f, 4.0f));
        system.SetGravity(Math::Vec2(0.0f, 260.0f));
        particleComponent.setBlendMode(BLEND_MIX);
        prefab.direction = Math::Vec2(0.0f, 1.0f);
        prefab.spreadDegrees = 4.0f;
        prefab.speedMin = 380.0f;
        prefab.speedMax = 520.0f;
        prefab.lifeMin = 1.6f;
        prefab.lifeMax = 2.2f;
        prefab.sizeMin = 2.5f;
        prefab.sizeMax = 4.0f;
        prefab.endSize = 0.0f;
        prefab.rotationMin = 0.0f;
        prefab.rotationMax = 0.0f;
        prefab.angularVelocityMin = 0.0f;
        prefab.angularVelocityMax = 0.0f;
        prefab.drag = 0.0f;
        prefab.fadeIn = 0.0f;
        prefab.fadeOut = 0.15f;
        prefab.colorStart = Color(0.6f, 0.7f, 0.9f, 0.75f);
        prefab.colorEnd = Color(0.6f, 0.7f, 0.9f, 0.5f);
        break;
    case ParticlePreset::Snow:
        system.SetMode(ParticleMode::Loop);
        system.SetEmissionRate(45.0f);
        if (system.Capacity() < 400)
            system.SetCapacity(400);
        system.SetEmitterShape(ParticleEmitterShape::Rectangle);
        system.SetEmitterSize(Math::Vec2(300.0f, 4.0f));
        system.SetGravity(Math::Vec2(0.0f, 12.0f));
        particleComponent.setBlendMode(BLEND_MIX);
        prefab.direction = Math::Vec2(0.0f, 1.0f);
        prefab.spreadDegrees = 30.0f;
        prefab.speedMin = 18.0f;
        prefab.speedMax = 45.0f;
        prefab.lifeMin = 4.0f;
        prefab.lifeMax = 7.0f;
        prefab.sizeMin = 3.0f;
        prefab.sizeMax = 7.0f;
        prefab.endSize = 0.0f;
        prefab.rotationMin = 0.0f;
        prefab.rotationMax = 360.0f;
        prefab.angularVelocityMin = -40.0f;
        prefab.angularVelocityMax = 40.0f;
        prefab.drag = 0.15f;
        prefab.fadeIn = 0.4f;
        prefab.fadeOut = 1.2f;
        prefab.colorStart = Color(1.0f, 1.0f, 1.0f, 0.9f);
        prefab.colorEnd = Color(0.95f, 0.97f, 1.0f, 0.6f);
        break;
    }

    system.SetPrefab(prefab);
    system.Reset();
}

void drawParticleProperties(EditorApplication& app, ParticleComponent& particleComponent)
{
    ParticleSystem& system = particleComponent.system();

    ImGui::TextUnformatted("Presets");
    if (ImGui::Button("Fire"))
        applyInstant(app, "Apply Fire Preset",
                     [&] { applyParticlePreset(app, particleComponent, ParticlePreset::Fire); });
    ImGui::SameLine();
    if (ImGui::Button("Smoke"))
        applyInstant(app, "Apply Smoke Preset",
                     [&] { applyParticlePreset(app, particleComponent, ParticlePreset::Smoke); });
    ImGui::SameLine();
    if (ImGui::Button("Explosion"))
    {
        applyInstant(app, "Apply Explosion Preset",
                     [&] { applyParticlePreset(app, particleComponent, ParticlePreset::Explosion); });
    }
    if (ImGui::Button("Engine Flame"))
    {
        applyInstant(app, "Apply Engine Flame Preset",
                     [&] { applyParticlePreset(app, particleComponent, ParticlePreset::EngineFlame); });
    }
    ImGui::SameLine();
    if (ImGui::Button("Rain"))
        applyInstant(app, "Apply Rain Preset",
                     [&] { applyParticlePreset(app, particleComponent, ParticlePreset::Rain); });
    ImGui::SameLine();
    if (ImGui::Button("Snow"))
        applyInstant(app, "Apply Snow Preset",
                     [&] { applyParticlePreset(app, particleComponent, ParticlePreset::Snow); });
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "Presets set emission, blend mode, gravity, emitter shape and the whole emission prefab below.");
    ImGui::Separator();

    Texture* texture = system.GetTexture();
    Texture* newTexture = nullptr;
    if (textureField(app, "Texture", texture, newTexture))
        applyInstant(app, "Set Particle Texture", [&] { system.SetTexture(newTexture); });
    if (!texture)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "No texture - particles will not render.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Use default dot"))
            applyInstant(app, "Set Particle Texture", [&] { system.SetTexture(app.particlePlaceholderTexture()); });
    }

    int capacity = static_cast<int>(system.Capacity());
    if (dragIntProperty(app, "Capacity", capacity, 1.0f, "Resize Particle Capacity", 1, 1000000))
        system.SetCapacity(static_cast<size_t>(capacity));

    static const char* modeNames[] = {"One Shot", "Persistent", "Loop"};
    int mode = static_cast<int>(system.GetMode());
    if (ImGui::Combo("Mode", &mode, modeNames, 3))
        applyInstant(app, "Set Particle Mode", [&] { system.SetMode(static_cast<ParticleMode>(mode)); });

    static const char* shapeNames[] = {"Point", "Circle", "Rectangle"};
    int shape = static_cast<int>(system.GetEmitterShape());
    if (ImGui::Combo("Emitter Shape", &shape, shapeNames, 3))
        applyInstant(app, "Set Particle Emitter Shape",
                     [&] { system.SetEmitterShape(static_cast<ParticleEmitterShape>(shape)); });

    Math::Vec2 emitterSize = system.EmitterSize();
    if (dragVec2(app, "Emitter Size", emitterSize, 0.5f, "Resize Particle Emitter"))
        system.SetEmitterSize(emitterSize);

    ImGui::BeginDisabled(particleComponent.followOwner());
    Math::Vec2 emitterPosition = system.EmitterPosition();
    if (dragVec2(app, "Emitter Position", emitterPosition, 0.5f, "Move Particle Emitter"))
        system.SetEmitterPosition(emitterPosition);
    ImGui::EndDisabled();
    if (particleComponent.followOwner())
        ImGui::TextDisabled("Emitter position follows the owner while Follow Owner is on.");

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
        applyInstant(app, system.IsPlaying() ? "Stop Particles" : "Start Particles",
                     [&]
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

void drawComponentProperties(EditorApplication& app, Component& component)
{
    switch (component.type())
    {
    case ComponentType::Sprite:
        drawSpriteProperties(app, static_cast<SpriteComponent&>(component));
        break;
    case ComponentType::TileMap:
        drawTileMapProperties(app, static_cast<TileMapComponent&>(component));
        break;
    case ComponentType::ParallaxLayer:
        drawParallaxLayerProperties(app, static_cast<ParallaxLayerComponent&>(component));
        break;
    case ComponentType::Polygon2D:
        drawPolygonProperties(app, static_cast<Polygon2D&>(component));
        break;
    case ComponentType::NavigationRegion:
        drawNavigationRegionProperties(app, static_cast<NavigationRegion2D&>(component));
        break;
    case ComponentType::NavigationAgent:
        drawNavigationAgentProperties(app, static_cast<NavigationAgent2D&>(component));
        break;
    case ComponentType::LinePath:
        drawLineProperties(app, static_cast<Line2D&>(component));
        break;
    case ComponentType::CircleShape:
        drawCircleShapeProperties(app, static_cast<CircleShape&>(component));
        break;
    case ComponentType::RectShape:
        drawRectShapeProperties(app, static_cast<RectShape&>(component));
        break;
    case ComponentType::CapsuleShape:
        drawCapsuleShapeProperties(app, static_cast<CapsuleShape&>(component));
        break;
    case ComponentType::AudioPlayer:
        drawAudioPlayerProperties(app, static_cast<AudioPlayer&>(component));
        break;
    case ComponentType::NinePatch:
        drawNinePatchProperties(app, static_cast<NinePatchComponent&>(component));
        break;
    case ComponentType::UiCanvas:
        ImGui::TextDisabled("Create UI controls as children of this GameObject.");
        break;
    case ComponentType::UiPanel:
        drawUiPanelProperties(app, static_cast<UiPanel&>(component));
        break;
    case ComponentType::UiLabel:
        drawUiLabelProperties(app, static_cast<UiLabel&>(component));
        break;
    case ComponentType::UiButton:
        drawUiButtonProperties(app, static_cast<UiButton&>(component));
        break;
    case ComponentType::UiCheckBox:
        drawUiCheckBoxProperties(app, static_cast<UiCheckBox&>(component));
        break;
    case ComponentType::UiSlider:
        drawUiSliderProperties(app, static_cast<UiSlider&>(component));
        break;
    case ComponentType::SpriteBatch:
        drawSpriteBatchProperties(app, static_cast<SpriteBatch&>(component));
        break;
    case ComponentType::Animation:
        drawAnimationProperties(app, static_cast<Animation2D&>(component));
        break;
    case ComponentType::Light:
        if (Light2D* light = dynamic_cast<Light2D*>(&component))
            drawPointLightProperties(app, *light);
        else if (DirectionalLight2D* directional = dynamic_cast<DirectionalLight2D*>(&component))
            drawDirectionalLightProperties(app, *directional);
        break;
    case ComponentType::Occluder:
        drawOccluderProperties(app, static_cast<LightOccluder2D&>(component));
        break;
    case ComponentType::Camera:
        drawCameraProperties(app, static_cast<CameraComponent&>(component));
        break;
    case ComponentType::Particle:
        drawParticleProperties(app, static_cast<ParticleComponent&>(component));
        break;
    case ComponentType::RigidBody:
        if (RigidBody2D* body = dynamic_cast<RigidBody2D*>(&component))
            drawRigidBodyProperties(app, *body);
        break;
    case ComponentType::CharacterBody:
        drawCharacterBodyProperties(app, static_cast<CharacterBody2D&>(component));
        break;
    case ComponentType::Collider:
        if (BoxCollider2D* box = dynamic_cast<BoxCollider2D*>(&component))
            drawBoxColliderProperties(app, *box);
        else if (CircleCollider2D* circle = dynamic_cast<CircleCollider2D*>(&component))
            drawCircleColliderProperties(app, *circle);
        else if (EdgeCollider2D* edge = dynamic_cast<EdgeCollider2D*>(&component))
            drawEdgeColliderProperties(app, *edge);
        else if (PolygonCollider2D* polygon = dynamic_cast<PolygonCollider2D*>(&component))
            drawPolygonColliderProperties(app, *polygon);
        else if (ChainCollider2D* chain = dynamic_cast<ChainCollider2D*>(&component))
            drawChainColliderProperties(app, *chain);
        else if (TileMapCollider2D* tileMap = dynamic_cast<TileMapCollider2D*>(&component))
            drawTileMapColliderProperties(app, *tileMap);
        break;
    case ComponentType::Joint:
        if (DistanceJoint2D* distance = dynamic_cast<DistanceJoint2D*>(&component))
            drawDistanceJointProperties(app, *distance);
        else if (RevoluteJoint2D* revolute = dynamic_cast<RevoluteJoint2D*>(&component))
            drawRevoluteJointProperties(app, *revolute);
        else if (WheelJoint2D* wheel = dynamic_cast<WheelJoint2D*>(&component))
            drawWheelJointProperties(app, *wheel);
        else if (MotorJoint2D* motor = dynamic_cast<MotorJoint2D*>(&component))
            drawMotorJointProperties(app, *motor);
        else if (MouseJoint2D* mouse = dynamic_cast<MouseJoint2D*>(&component))
            drawMouseJointProperties(app, *mouse);
        else if (GearJoint2D* gear = dynamic_cast<GearJoint2D*>(&component))
            drawGearJointProperties(app, *gear);
        break;
    case ComponentType::Script:
        if (ZenScriptComponent* script = dynamic_cast<ZenScriptComponent*>(&component))
            drawZenScriptProperties(app, *script);
        else
            ImGui::TextDisabled("Native script component (code-attached).");
        break;
    default:
        break;
    }
}
} // namespace

void InspectorPanel::drawContents()
{
    GameObject* object = app().selection().resolve(app().scene());
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
    if (dragVec2(app(), "Position", position, 0.25f, "Move GameObject"))
        object->setPosition(position);
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
    Component* componentToRemove = nullptr;
    EditorApplication::SceneChange removeBefore;
    ct::String removeLabel;
    for (unsigned int value = 0; value < static_cast<unsigned int>(ComponentType::Count); ++value)
    {
        const ComponentType type = static_cast<ComponentType>(value);
        const size_t count = object->rawComponentCount(type);
        for (size_t index = 0; index < count; ++index)
        {
            Component* component = object->rawComponent(type, index);
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
            const bool componentSelected = app().selection().componentId() == component->id();
            if (componentSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            }
            const bool open = ImGui::TreeNodeEx("##header",
                                                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
                                                    ImGuiTreeNodeFlags_SpanAvailWidth,
                                                "%s  #%u", componentName(*component), component->id());
            if (componentSelected)
                ImGui::PopStyleColor(2);
            if (ImGui::IsItemClicked())
                app().selection().selectComponent(component->id());
            if (open)
            {
                ImGui::Indent();
                const char* description = componentDescription(*component);
                if (description[0] != '\0')
                    ImGui::TextDisabled("%s", description);
                drawComponentProperties(app(), *component);
                ImGui::Separator();
                if (ImGui::Button(ICON_MDI_DELETE " Remove Component"))
                {
                    componentToRemove = component;
                    removeBefore = app().beginChange();
                    removeLabel = "Remove ";
                    removeLabel += componentName(*component);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Remove this component. Ctrl+Z restores it.");
                ImGui::Unindent();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    if (componentToRemove && object->removeComponent(componentToRemove))
    {
        app().commitChange(removeLabel.c_str(), removeBefore);
        app().toasts().info("Component removed");
    }
    if (!any)
        ImGui::TextDisabled("No components yet.");

    if (ImGui::Button("Add Component..."))
        ImGui::OpenPopup("Add Component");
    if (ImGui::BeginPopup("Add Component"))
    {
        if (ImGui::BeginMenu("Rendering"))
        {
            if (componentMenuItem("Sprite", "Draw a texture on this object."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<SpriteComponent>(placeholderSpriteTexture(app()));
                app().commitChange("Add Sprite Component", addBefore);
            }
            if (componentMenuItem("NinePatch", "Draw a scalable panel with fixed borders."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                NinePatchComponent* ninePatch = object->addComponent<NinePatchComponent>();
                ninePatch->setTexture(placeholderSpriteTexture(app()));
                app().commitChange("Add NinePatch Component", addBefore);
            }
            if (componentMenuItem("SpriteBatch", "Draw many sprites efficiently."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<SpriteBatch>();
                app().commitChange("Add SpriteBatch Component", addBefore);
            }
            if (componentMenuItem("Animation2D", "Play a frame-based sprite animation."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                Animation2D* anim = object->addComponent<Animation2D>();
                anim->addClip("default", nullptr, 0, 0, 0, 10.0f, AnimationMode::Loop);
                anim->play("default");
                app().commitChange("Add Animation2D Component", addBefore);
            }
            if (componentMenuItem("Polygon2D", "Draw a filled custom polygon."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                Polygon2D* polygon = object->addComponent<Polygon2D>();
                const Math::Vec2 defaultShape[3] = {Math::Vec2(0.0f, -30.0f), Math::Vec2(26.0f, 20.0f),
                                                    Math::Vec2(-26.0f, 20.0f)};
                polygon->setPolygon(defaultShape, 3);
                app().commitChange("Add Polygon2D Component", addBefore);
            }
            if (componentMenuItem("Line2D", "Draw an editable line or outline."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                Line2D* line = object->addComponent<Line2D>();
                const Math::Vec2 defaultPoints[2] = {Math::Vec2(-30.0f, 0.0f), Math::Vec2(30.0f, 0.0f)};
                line->setPoints(defaultPoints, 2);
                app().commitChange("Add Line2D Component", addBefore);
            }
            if (componentMenuItem("Circle Shape", "Draw a visual circle. Use Circle Collider for physics."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<CircleShape>();
                app().commitChange("Add Circle Shape Component", addBefore);
            }
            if (componentMenuItem("Rect Shape", "Draw a visual rectangle. Use Box Collider for physics."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<RectShape>();
                app().commitChange("Add Rect Shape Component", addBefore);
            }
            if (componentMenuItem("Capsule Shape", "Draw a visual filled or outlined capsule."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<CapsuleShape>();
                app().commitChange("Add Capsule Shape Component", addBefore);
            }
            if (componentMenuItem("TileMap", "Draw a grid of tiles from a texture atlas."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                TileMapComponent* tileMap = object->addComponent<TileMapComponent>();
                tileMap->setMapSize(8, 8);
                app().commitChange("Add TileMap Component", addBefore);
            }
            if (componentMenuItem("Parallax Layer",
                                  "Camera-relative repeating background layer for parallax scrolling."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                ParallaxLayerComponent* layer = object->addComponent<ParallaxLayerComponent>();
                Texture* texture = placeholderSpriteTexture(app());
                layer->setTexture(texture);
                layer->setMotionScale(Math::Vec2(0.5f, 0.5f));
                app().commitChange("Add Parallax Layer Component", addBefore);
            }
            if (componentMenuItem("Particle", "Emit and simulate a particle effect."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                ParticleComponent* particle = object->addComponent<ParticleComponent>();
                particle->system().SetTexture(app().particlePlaceholderTexture());
                particle->system().SetMode(ParticleMode::Loop);
                particle->system().SetEmissionRate(20.0f);
                ParticlePrefab prefab;
                prefab.spreadDegrees = 40.0f;
                prefab.speedMin = 40.0f;
                prefab.speedMax = 90.0f;
                prefab.lifeMin = 0.8f;
                prefab.lifeMax = 1.6f;
                prefab.sizeMin = 6.0f;
                prefab.sizeMax = 12.0f;
                prefab.endSize = 2.0f;
                prefab.fadeOut = 0.4f;
                particle->system().SetPrefab(prefab);
                particle->system().Start();
                app().commitChange("Add Particle Component", addBefore);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Lighting"))
        {
            if (componentMenuItem("Point Light", "Illuminate an area from this object's position."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<Light2D>();
                app().commitChange("Add Point Light Component", addBefore);
            }
            if (componentMenuItem("Directional Light", "Illuminate the scene with one direction."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<DirectionalLight2D>();
                app().commitChange("Add Directional Light Component", addBefore);
            }
            if (componentMenuItem("Light Occluder", "Block shadows cast by 2D lights."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                LightOccluder2D* occluder = object->addComponent<LightOccluder2D>();
                const Math::Vec2 defaultShape[3] = {Math::Vec2(0.0f, -30.0f), Math::Vec2(26.0f, 20.0f),
                                                    Math::Vec2(-26.0f, 20.0f)};
                occluder->setPolygon(defaultShape, 3);
                app().commitChange("Add Light Occluder Component", addBefore);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Camera"))
        {
            if (componentMenuItem("Camera", "Control the Game view and optionally follow another object."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<CameraComponent>();
                app().commitChange("Add Camera Component", addBefore);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Physics"))
        {
            if (componentMenuItem("Rigid Body", "Simulate this object with Physics 2D during Play."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<RigidBody2D>();
                app().commitChange("Add Rigid Body Component", addBefore);
            }
            if (componentMenuItem("Character Body",
                                  "Script-driven kinematic movement with move_and_collide and move_and_slide."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                RigidBody2D* body = object->getComponent<RigidBody2D>();
                if (!body)
                    body = object->addComponent<RigidBody2D>();
                if (body)
                    body->setBodyType(k2d::BodyType::Kinematic);
                object->addComponent<CharacterBody2D>();
                app().commitChange("Add Character Body Component", addBefore);
            }
            ImGui::Separator();
            if (componentMenuItem("Box Collider", "Physical rectangular collision shape."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<BoxCollider2D>();
                app().commitChange("Add Box Collider Component", addBefore);
            }
            if (componentMenuItem("Circle Collider", "Physical circular collision shape."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<CircleCollider2D>();
                app().commitChange("Add Circle Collider Component", addBefore);
            }
            if (componentMenuItem("Edge Collider", "Physical line segment; useful for floors."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<EdgeCollider2D>();
                app().commitChange("Add Edge Collider Component", addBefore);
            }
            if (componentMenuItem("Polygon Collider", "Physical convex polygon collision shape."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<PolygonCollider2D>();
                app().commitChange("Add Polygon Collider Component", addBefore);
            }
            if (componentMenuItem("Chain Collider", "Physical chain of segments for terrain."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                ChainCollider2D* chain = object->addComponent<ChainCollider2D>();
                const Math::Vec2 defaults[3] = {Math::Vec2(-60.0f, 0.0f), Math::Vec2(0.0f, 30.0f),
                                                Math::Vec2(60.0f, 0.0f)};
                chain->setPoints(defaults, 3);
                app().commitChange("Add Chain Collider Component", addBefore);
            }
            if (componentMenuItem("TileMap Collider", "Collision from the sibling TileMapComponent's solid cells."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<TileMapCollider2D>();
                app().commitChange("Add TileMap Collider Component", addBefore);
            }
            ImGui::Separator();
            if (componentMenuItem("Distance Joint", "Keeps two bodies within a fixed or spring distance range."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<DistanceJoint2D>();
                app().commitChange("Add Distance Joint Component", addBefore);
            }
            if (componentMenuItem("Revolute Joint", "Hinges two bodies, with optional motor and limits."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<RevoluteJoint2D>();
                app().commitChange("Add Revolute Joint Component", addBefore);
            }
            if (componentMenuItem("Wheel Joint", "Suspension joint sliding along an axis with a spring."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<WheelJoint2D>();
                app().commitChange("Add Wheel Joint Component", addBefore);
            }
            if (componentMenuItem("Motor Joint", "Drives one body toward an offset from another."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<MotorJoint2D>();
                app().commitChange("Add Motor Joint Component", addBefore);
            }
            if (componentMenuItem("Mouse Joint", "Pulls this object's body toward a moving target point."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<MouseJoint2D>();
                app().commitChange("Add Mouse Joint Component", addBefore);
            }
            if (componentMenuItem("Gear Joint", "Couples two Revolute Joints with a fixed ratio."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<GearJoint2D>();
                app().commitChange("Add Gear Joint Component", addBefore);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Navigation"))
        {
            if (componentMenuItem("Navigation Region 2D", "Editable walkable polygon; does not require a TileMap."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                NavigationRegion2D* region = object->addComponent<NavigationRegion2D>();
                const Math::Vec2 defaultShape[4] = {Math::Vec2(-160.0f, -100.0f), Math::Vec2(160.0f, -100.0f),
                                                    Math::Vec2(160.0f, 100.0f), Math::Vec2(-160.0f, 100.0f)};
                region->setPolygon(defaultShape, 4);
                app().commitChange("Add Navigation Region 2D", addBefore);
            }
            if (componentMenuItem("Navigation Agent 2D",
                                  "Gets a path from a Navigation Region. Auto Move is optional."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<NavigationAgent2D>();
                app().commitChange("Add Navigation Agent 2D", addBefore);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("UI"))
        {
            if (componentMenuItem("UI Canvas", "Root marker for screen-space UI controls."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<UiCanvas>();
                app().commitChange("Add UI Canvas", addBefore);
            }
            if (componentMenuItem("Panel", "Screen-space coloured panel; can contain other UI nodes."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<UiPanel>();
                app().commitChange("Add UI Panel", addBefore);
            }
            if (componentMenuItem("Label", "Screen-space bitmap-font text."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<UiLabel>();
                app().commitChange("Add UI Label", addBefore);
            }
            if (componentMenuItem("Button", "Screen-space clickable button."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<UiButton>();
                app().commitChange("Add UI Button", addBefore);
            }
            if (componentMenuItem("CheckBox", "Screen-space boolean control."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<UiCheckBox>();
                app().commitChange("Add UI CheckBox", addBefore);
            }
            if (componentMenuItem("Slider", "Screen-space numeric control."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<UiSlider>();
                app().commitChange("Add UI Slider", addBefore);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Audio"))
        {
            if (componentMenuItem("Audio Player", "Play an SFX or music asset during Play."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<AudioPlayer>();
                app().commitChange("Add Audio Player Component", addBefore);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Scripting"))
        {
            if (componentMenuItem("Zen Script", "Run a Python-style Zen script with self.node."))
            {
                const EditorApplication::SceneChange addBefore = app().beginChange();
                object->addComponent<ZenScriptComponent>();
                app().commitChange("Add Zen Script Component", addBefore);
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImGui::EndDisabled();
}

} // namespace k2d::editor
