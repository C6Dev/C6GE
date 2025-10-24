#include "EditorTheme.h"

namespace EditorTheme
{
    void SetupImGuiStyle()
    {
    // Modern macOS-inspired style
    ImGuiStyle &style = ImGui::GetStyle();

    style.Alpha = 0.95f;
    style.DisabledAlpha = 0.5f;
    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.WindowRounding = 18.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowMinSize = ImVec2(40.0f, 40.0f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ChildRounding = 14.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupRounding = 12.0f;
    style.PopupBorderSize = 0.0f;
    style.FramePadding = ImVec2(10.0f, 8.0f);
    style.FrameRounding = 10.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(12.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.CellPadding = ImVec2(8.0f, 4.0f);
    style.IndentSpacing = 24.0f;
    style.ColumnsMinSpacing = 10.0f;
    style.ScrollbarSize = 12.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabMinSize = 16.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 10.0f;
    style.TabBorderSize = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    // macOS-inspired color palette: subtle gradients, translucency, vibrant blue accents
    ImVec4 accent = ImVec4(0.0f, 0.478f, 1.0f, 1.0f); // macOS blue
    ImVec4 accent_hover = ImVec4(0.0f, 0.6f, 1.0f, 1.0f);
    ImVec4 bg = ImVec4(0.13f, 0.14f, 0.16f, 0.85f); // window bg, translucent
    ImVec4 bg_light = ImVec4(0.18f, 0.19f, 0.22f, 0.92f);
    ImVec4 bg_ultra = ImVec4(0.98f, 0.98f, 1.0f, 0.7f); // for highlights
    ImVec4 border = ImVec4(0.65f, 0.65f, 0.7f, 0.18f);
    ImVec4 text = ImVec4(0.98f, 0.98f, 1.0f, 1.0f);
    ImVec4 text_disabled = ImVec4(0.7f, 0.7f, 0.8f, 0.7f);

    style.Colors[ImGuiCol_Text] = text;
    style.Colors[ImGuiCol_TextDisabled] = text_disabled;
    style.Colors[ImGuiCol_WindowBg] = bg;
    style.Colors[ImGuiCol_ChildBg] = bg_light;
    style.Colors[ImGuiCol_PopupBg] = bg_light;
    style.Colors[ImGuiCol_Border] = border;
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0,0,0,0);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.19f, 0.22f, 0.92f);
    style.Colors[ImGuiCol_FrameBgHovered] = accent_hover;
    style.Colors[ImGuiCol_FrameBgActive] = accent;
    style.Colors[ImGuiCol_TitleBg] = bg;
    style.Colors[ImGuiCol_TitleBgActive] = bg_light;
    style.Colors[ImGuiCol_TitleBgCollapsed] = bg;
    style.Colors[ImGuiCol_MenuBarBg] = bg_light;
    style.Colors[ImGuiCol_ScrollbarBg] = bg;
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.7f, 0.7f, 0.8f, 0.25f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.7f, 0.7f, 0.8f, 0.35f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.7f, 0.7f, 0.8f, 0.45f);
    style.Colors[ImGuiCol_CheckMark] = accent;
    style.Colors[ImGuiCol_SliderGrab] = accent;
    style.Colors[ImGuiCol_SliderGrabActive] = accent_hover;
    style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.19f, 0.22f, 0.92f);
    style.Colors[ImGuiCol_ButtonHovered] = accent_hover;
    style.Colors[ImGuiCol_ButtonActive] = accent;
    style.Colors[ImGuiCol_Header] = ImVec4(0.18f, 0.19f, 0.22f, 0.92f);
    style.Colors[ImGuiCol_HeaderHovered] = accent_hover;
    style.Colors[ImGuiCol_HeaderActive] = accent;
    style.Colors[ImGuiCol_Separator] = border;
    style.Colors[ImGuiCol_SeparatorHovered] = accent_hover;
    style.Colors[ImGuiCol_SeparatorActive] = accent;
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.7f, 0.7f, 0.8f, 0.15f);
    style.Colors[ImGuiCol_ResizeGripHovered] = accent_hover;
    style.Colors[ImGuiCol_ResizeGripActive] = accent;
    style.Colors[ImGuiCol_Tab] = bg_light;
    style.Colors[ImGuiCol_TabHovered] = accent_hover;
    style.Colors[ImGuiCol_TabActive] = accent;
    style.Colors[ImGuiCol_TabUnfocused] = bg;
    style.Colors[ImGuiCol_TabUnfocusedActive] = accent;
    style.Colors[ImGuiCol_PlotLines] = accent;
    style.Colors[ImGuiCol_PlotLinesHovered] = accent_hover;
    style.Colors[ImGuiCol_PlotHistogram] = accent;
    style.Colors[ImGuiCol_PlotHistogramHovered] = accent_hover;
    style.Colors[ImGuiCol_TableHeaderBg] = bg_light;
    style.Colors[ImGuiCol_TableBorderStrong] = border;
    style.Colors[ImGuiCol_TableBorderLight] = border;
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
    style.Colors[ImGuiCol_TextSelectedBg] = accent;
    style.Colors[ImGuiCol_DragDropTarget] = accent;
    style.Colors[ImGuiCol_NavHighlight] = accent;
    style.Colors[ImGuiCol_NavWindowingHighlight] = bg_ultra;
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.9f, 0.18f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.13f, 0.14f, 0.16f, 0.85f);
        style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882353f, 0.1882353f, 0.2f, 1.0f);
        style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30980393f, 0.30980393f, 0.34901962f, 1.0f);
        style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.22745098f, 0.22745098f, 0.24705882f, 1.0f);
        style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.0f, 0.46666667f, 0.78431374f, 1.0f);
        style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.14509805f, 0.14509805f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.14509805f, 0.14509805f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.14509805f, 0.14509805f, 0.14901961f, 1.0f);
    }

} // namespace EditorTheme