#ifndef K2D_EDITOR_THEME_H
#define K2D_EDITOR_THEME_H

 
#include <imgui.h>
#include <IconsMaterialDesignIcons.h>

 
#include "MaterialDesign.inl"

namespace k2d::editor
{

enum class EditorThemeKind
{
    RadionDark,
    Light,
    Blender,
    Nord,
    Ember,
};
constexpr int kEditorThemeCount = static_cast<int>(EditorThemeKind::Ember) + 1;

inline const char* editorThemeName(EditorThemeKind kind)
{
    switch (kind)
    {
        case EditorThemeKind::RadionDark: return "Radion Dark";
        case EditorThemeKind::Light: return "Light";
        case EditorThemeKind::Blender: return "Blender";
        case EditorThemeKind::Nord: return "Nord";
        case EditorThemeKind::Ember: return "Ember";
    }
    return "Radion Dark";
}

inline void applyRadionDarkTheme()
{
    ImVec4* c = ImGui::GetStyle().Colors;
    c[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    c[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
    c[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    c[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    c[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    c[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
    c[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    c[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    c[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    c[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
    c[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    c[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    c[ImGuiCol_DockingEmptyBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    c[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    c[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    c[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    c[ImGuiCol_NavHighlight] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.PopupRounding = 3.0f;
}

// Blender 3.0 "Blender Dark" palette.
inline void applyBlenderTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;
    ImGui::StyleColorsDark(&style);
    c[ImGuiCol_Text] = ImVec4(0.84f, 0.84f, 0.84f, 1.00f);
    c[ImGuiCol_WindowBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    c[ImGuiCol_Border] = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.10f, 0.10f, 0.10f, 0.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    c[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.54f, 0.54f, 0.54f, 1.00f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.19f, 0.39f, 0.69f, 1.00f);
    c[ImGuiCol_Tab] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    c[ImGuiCol_TabActive] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    c[ImGuiCol_PlotHistogram] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.20f, 0.39f, 0.69f, 1.00f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    c[ImGuiCol_NavHighlight] = ImVec4(0.28f, 0.45f, 0.70f, 1.00f);
    style.WindowPadding = ImVec2(12.00f, 8.00f);
    style.ItemSpacing = ImVec2(7.00f, 3.00f);
    style.GrabMinSize = 20.00f;
    style.WindowRounding = 8.00f;
    style.FrameBorderSize = 0.00f;
    style.FrameRounding = 4.00f;
    style.GrabRounding = 12.00f;
}

// Nord/Nordic GTK palette.
inline void applyNordTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;
    ImGui::StyleColorsDark(&style);
    c[ImGuiCol_Text] = ImVec4(0.85f, 0.87f, 0.91f, 0.88f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.49f, 0.50f, 0.53f, 1.00f);
    c[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.17f, 0.20f, 1.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
    c[ImGuiCol_Border] = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.09f, 0.09f, 0.09f, 0.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.56f, 0.74f, 0.73f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.53f, 0.75f, 0.82f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.23f, 0.26f, 0.32f, 0.60f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
    c[ImGuiCol_CheckMark] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.63f, 0.76f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.51f, 0.63f, 0.76f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.51f, 0.63f, 0.76f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.53f, 0.75f, 0.82f, 1.00f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.56f, 0.74f, 0.73f, 1.00f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.53f, 0.75f, 0.82f, 1.00f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.53f, 0.75f, 0.82f, 0.86f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.61f, 0.74f, 0.87f, 1.00f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
    c[ImGuiCol_Tab] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.24f, 0.31f, 1.00f);
    c[ImGuiCol_TabActive] = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
    c[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.17f, 0.19f, 0.23f, 1.00f);
    c[ImGuiCol_PlotHistogram] = ImVec4(0.56f, 0.74f, 0.73f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.53f, 0.75f, 0.82f, 1.00f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.37f, 0.51f, 0.67f, 1.00f);
    c[ImGuiCol_NavHighlight] = ImVec4(0.53f, 0.75f, 0.82f, 0.86f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.37f, 0.51f, 0.67f, 0.65f);
    style.WindowBorderSize = 1.00f;
    style.ChildBorderSize = 1.00f;
    style.PopupBorderSize = 1.00f;
    style.FrameBorderSize = 1.00f;
}

// Neutral grey with a warm orange accent.
inline void applyEmberTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;
    ImGui::StyleColorsDark(&style);
    c[ImGuiCol_Text] = ImVec4(1.000000f, 1.000000f, 1.000000f, 1.000000f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.500000f, 0.500000f, 0.500000f, 1.000000f);
    c[ImGuiCol_WindowBg] = ImVec4(0.160000f, 0.160000f, 0.160000f, 1.000000f);
    c[ImGuiCol_ChildBg] = ImVec4(0.160000f, 0.160000f, 0.160000f, 1.000000f);
    c[ImGuiCol_PopupBg] = ImVec4(0.140000f, 0.140000f, 0.140000f, 1.000000f);
    c[ImGuiCol_Border] = ImVec4(0.240000f, 0.240000f, 0.240000f, 1.000000f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.000000f, 0.000000f, 0.000000f, 0.000000f);
    c[ImGuiCol_FrameBg] = ImVec4(0.260000f, 0.260000f, 0.260000f, 1.000000f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.320000f, 0.320000f, 0.320000f, 1.000000f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.370000f, 0.370000f, 0.370000f, 1.000000f);
    c[ImGuiCol_TitleBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 1.000000f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.156863f, 0.156863f, 0.156863f, 1.000000f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.160000f, 0.160000f, 0.160000f, 1.000000f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.140000f, 0.140000f, 0.140000f, 1.000000f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.020000f, 0.020000f, 0.020000f, 0.000000f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.310000f, 0.310000f, 0.310000f, 1.000000f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.410000f, 0.410000f, 0.410000f, 1.000000f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.510000f, 0.510000f, 0.510000f, 1.000000f);
    c[ImGuiCol_CheckMark] = ImVec4(0.510000f, 0.510000f, 0.510000f, 1.000000f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.510000f, 0.510000f, 0.510000f, 1.000000f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.560000f, 0.560000f, 0.560000f, 1.000000f);
    c[ImGuiCol_Button] = ImVec4(0.270000f, 0.270000f, 0.270000f, 1.000000f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.340000f, 0.340000f, 0.340000f, 1.000000f);
    c[ImGuiCol_ButtonActive] = ImVec4(1.000000f, 0.501961f, 0.000000f, 1.000000f);
    c[ImGuiCol_Header] = ImVec4(0.350000f, 0.350000f, 0.350000f, 1.000000f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.390000f, 0.390000f, 0.390000f, 1.000000f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.440000f, 0.440000f, 0.440000f, 1.000000f);
    c[ImGuiCol_Separator] = ImVec4(0.240000f, 0.240000f, 0.240000f, 1.000000f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.310000f, 0.310000f, 0.310000f, 1.000000f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.340000f, 0.340000f, 0.340000f, 1.000000f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.240000f, 0.240000f, 0.240000f, 1.000000f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.310000f, 0.310000f, 0.310000f, 1.000000f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.370000f, 0.370000f, 0.370000f, 1.000000f);
    c[ImGuiCol_Tab] = ImVec4(0.313726f, 0.313726f, 0.313726f, 1.000000f);
    c[ImGuiCol_TabHovered] = ImVec4(0.579487f, 0.579487f, 0.579487f, 1.000000f);
    c[ImGuiCol_TabActive] = ImVec4(0.501961f, 0.501961f, 0.501961f, 1.000000f);
    c[ImGuiCol_TabUnfocused] = ImVec4(0.313726f, 0.313726f, 0.313726f, 1.000000f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.376471f, 0.376471f, 0.376471f, 1.000000f);
    c[ImGuiCol_DockingPreview] = ImVec4(0.550000f, 0.550000f, 0.550000f, 1.000000f);
    c[ImGuiCol_DockingEmptyBg] = ImVec4(0.200000f, 0.200000f, 0.200000f, 1.000000f);
    c[ImGuiCol_PlotLines] = ImVec4(0.610000f, 0.610000f, 0.610000f, 1.000000f);
    c[ImGuiCol_PlotLinesHovered] = ImVec4(1.000000f, 0.430000f, 0.350000f, 1.000000f);
    c[ImGuiCol_PlotHistogram] = ImVec4(0.900000f, 0.700000f, 0.000000f, 1.000000f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(1.000000f, 0.600000f, 0.000000f, 1.000000f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.190000f, 0.190000f, 0.200000f, 1.000000f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.310000f, 0.310000f, 0.350000f, 1.000000f);
    c[ImGuiCol_TableBorderLight] = ImVec4(0.230000f, 0.230000f, 0.250000f, 1.000000f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.000000f, 0.000000f, 0.000000f, 0.000000f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.000000f, 1.000000f, 1.000000f, 0.060000f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.350000f);
    c[ImGuiCol_DragDropTarget] = ImVec4(1.000000f, 1.000000f, 0.000000f, 0.900000f);
    c[ImGuiCol_NavHighlight] = ImVec4(0.780000f, 0.880000f, 1.000000f, 1.000000f);
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000000f, 1.000000f, 1.000000f, 0.700000f);
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000f, 0.800000f, 0.800000f, 0.200000f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.440000f, 0.440000f, 0.440000f, 0.650000f);
    style.WindowMenuButtonPosition = ImGuiDir_Right;
    style.Alpha = 1.000000f;
    style.WindowPadding = ImVec2(4.000000f, 4.000000f);
    style.WindowRounding = 2.000000f;
    style.WindowBorderSize = 1.000000f;
    style.WindowMinSize = ImVec2(32.000000f, 32.000000f);
    style.WindowTitleAlign = ImVec2(0.000000f, 0.500000f);
    style.ChildRounding = 2.000000f;
    style.ChildBorderSize = 1.000000f;
    style.PopupRounding = 2.000000f;
    style.PopupBorderSize = 1.000000f;
    style.FramePadding = ImVec2(8.000000f, 2.000000f);
    style.FrameRounding = 2.000000f;
    style.FrameBorderSize = 0.000000f;
    style.ItemSpacing = ImVec2(4.000000f, 2.000000f);
    style.ItemInnerSpacing = ImVec2(2.000000f, 4.000000f);
    style.TouchExtraPadding = ImVec2(0.000000f, 0.000000f);
    style.IndentSpacing = 21.000000f;
    style.ColumnsMinSpacing = 6.000000f;
    style.ScrollbarSize = 14.000000f;
    style.ScrollbarRounding = 2.000000f;
    style.GrabMinSize = 10.000000f;
    style.GrabRounding = 2.000000f;
    style.TabRounding = 2.000000f;
    style.TabBorderSize = 0.000000f;
    style.DisplayWindowPadding = ImVec2(19.000000f, 19.000000f);
    style.DisplaySafeAreaPadding = ImVec2(3.000000f, 3.000000f);
}

inline void applyEditorTheme(EditorThemeKind kind)
{
    switch (kind)
    {
        case EditorThemeKind::RadionDark: applyRadionDarkTheme(); break;
        case EditorThemeKind::Light: ImGui::StyleColorsLight(); break;
        case EditorThemeKind::Blender: applyBlenderTheme(); break;
        case EditorThemeKind::Nord: applyNordTheme(); break;
        case EditorThemeKind::Ember: applyEmberTheme(); break;
    }
}

inline void applyEditorTheme()
{
    applyEditorTheme(EditorThemeKind::RadionDark);
}

// Call after io.Fonts->AddFontDefault(), before the first NewFrame()/font
// atlas build - merge mode layers the icon glyphs onto whatever font was
// added last.
inline bool loadEditorIconFont(ImGuiIO& io, float iconSizePixels = 16.0f)
{
    static const ImWchar iconRanges[] = {ICON_MIN_MDI, ICON_MAX_MDI, 0};
    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;
    config.GlyphMinAdvanceX = iconSizePixels;
    config.FontDataOwnedByAtlas = false;

    return io.Fonts->AddFontFromMemoryCompressedTTF(
              (void*)MaterialDesign_compressed_data, (int)MaterialDesign_compressed_size,
              iconSizePixels, &config, iconRanges) != nullptr;
}

} // namespace k2d::editor

#endif // K2D_EDITOR_THEME_H
