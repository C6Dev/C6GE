#include "EditorTheme.h"

namespace EditorTheme
{
    void SetupImGuiStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.Alpha                 = 1.0f;
        style.DisabledAlpha         = 0.45f;
        style.WindowPadding         = ImVec2(12.0f, 10.0f);
        style.WindowRounding        = 4.0f;
        style.WindowBorderSize      = 1.0f;
        style.WindowMinSize         = ImVec2(380.0f, 240.0f);
        style.WindowTitleAlign      = ImVec2(0.0f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.ChildRounding         = 4.0f;
        style.ChildBorderSize       = 1.0f;
        style.PopupRounding         = 4.0f;
        style.PopupBorderSize       = 1.0f;
        style.FramePadding          = ImVec2(10.0f, 8.0f);
        style.FrameRounding         = 3.0f;
        style.FrameBorderSize       = 1.0f;
        style.ItemSpacing           = ImVec2(12.0f, 8.0f);
        style.ItemInnerSpacing      = ImVec2(6.0f, 4.0f);
        style.CellPadding           = ImVec2(8.0f, 6.0f);
        style.IndentSpacing         = 18.0f;
        style.ColumnsMinSpacing     = 6.0f;
        style.ScrollbarSize         = 15.0f;
        style.ScrollbarRounding     = 5.0f;
        style.GrabMinSize           = 12.0f;
        style.GrabRounding          = 3.0f;
        style.TabRounding           = 4.0f;
        style.TabBorderSize         = 0.0f;
        style.ColorButtonPosition   = ImGuiDir_Right;
        style.ButtonTextAlign       = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign   = ImVec2(0.0f, 0.5f);

        const ImVec4 baseBg        = ImVec4(0.11f, 0.12f, 0.14f, 1.00f); // editor background
        const ImVec4 basePanel     = ImVec4(0.15f, 0.16f, 0.18f, 1.00f);
        const ImVec4 baseFrame     = ImVec4(0.19f, 0.20f, 0.23f, 1.00f);
        const ImVec4 baseFrameHi   = ImVec4(0.24f, 0.26f, 0.29f, 1.00f);
        const ImVec4 border        = ImVec4(0.21f, 0.22f, 0.25f, 1.00f);
        const ImVec4 borderStrong  = ImVec4(0.28f, 0.29f, 0.33f, 1.00f);
        const ImVec4 accent        = ImVec4(0.99f, 0.54f, 0.20f, 1.00f); // orange accent
        const ImVec4 accentHover   = ImVec4(1.00f, 0.62f, 0.28f, 1.00f);
        const ImVec4 accentActive  = ImVec4(1.00f, 0.70f, 0.36f, 1.00f);
        const ImVec4 textPrimary   = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
        const ImVec4 textMuted     = ImVec4(0.60f, 0.63f, 0.68f, 1.00f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text]                 = textPrimary;
        colors[ImGuiCol_TextDisabled]         = textMuted;
        colors[ImGuiCol_WindowBg]             = baseBg;
        colors[ImGuiCol_ChildBg]              = basePanel;
        colors[ImGuiCol_PopupBg]              = ImVec4(basePanel.x, basePanel.y, basePanel.z, 0.98f);
        colors[ImGuiCol_Border]               = border;
        colors[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_FrameBg]              = baseFrame;
        colors[ImGuiCol_FrameBgHovered]       = ImVec4(accent.x, accent.y, accent.z, 0.28f);
        colors[ImGuiCol_FrameBgActive]        = ImVec4(accent.x, accent.y, accent.z, 0.45f);
        colors[ImGuiCol_TitleBg]              = baseBg;
        colors[ImGuiCol_TitleBgActive]        = baseFrameHi;
        colors[ImGuiCol_TitleBgCollapsed]     = baseBg;
        colors[ImGuiCol_MenuBarBg]            = basePanel;
        colors[ImGuiCol_ScrollbarBg]          = basePanel;
        colors[ImGuiCol_ScrollbarGrab]        = baseFrame;
        colors[ImGuiCol_ScrollbarGrabHovered] = accentHover;
        colors[ImGuiCol_ScrollbarGrabActive]  = accentActive;
        colors[ImGuiCol_CheckMark]            = accent;
        colors[ImGuiCol_SliderGrab]           = accentHover;
        colors[ImGuiCol_SliderGrabActive]     = accentActive;
        colors[ImGuiCol_Button]               = baseFrame;
        colors[ImGuiCol_ButtonHovered]        = accentHover;
        colors[ImGuiCol_ButtonActive]         = accentActive;
        colors[ImGuiCol_Header]               = ImVec4(accent.x, accent.y, accent.z, 0.22f);
        colors[ImGuiCol_HeaderHovered]        = ImVec4(accentHover.x, accentHover.y, accentHover.z, 0.35f);
        colors[ImGuiCol_HeaderActive]         = ImVec4(accentActive.x, accentActive.y, accentActive.z, 0.45f);
        colors[ImGuiCol_Separator]            = borderStrong;
        colors[ImGuiCol_SeparatorHovered]     = accentHover;
        colors[ImGuiCol_SeparatorActive]      = accentActive;
        colors[ImGuiCol_ResizeGrip]           = ImVec4(accent.x, accent.y, accent.z, 0.15f);
        colors[ImGuiCol_ResizeGripHovered]    = accentHover;
        colors[ImGuiCol_ResizeGripActive]     = accentActive;
        colors[ImGuiCol_Tab]                  = baseFrame;
        colors[ImGuiCol_TabHovered]           = accentHover;
        colors[ImGuiCol_TabActive]            = baseFrameHi;
        colors[ImGuiCol_TabUnfocused]         = basePanel;
        colors[ImGuiCol_TabUnfocusedActive]   = baseFrame;
        colors[ImGuiCol_PlotLines]            = accent;
        colors[ImGuiCol_PlotLinesHovered]     = accentHover;
        colors[ImGuiCol_PlotHistogram]        = accent;
        colors[ImGuiCol_PlotHistogramHovered] = accentHover;
        colors[ImGuiCol_TableHeaderBg]        = baseFrame;
        colors[ImGuiCol_TableBorderStrong]    = borderStrong;
        colors[ImGuiCol_TableBorderLight]     = border;
        colors[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1, 1, 1, 0.04f);
        colors[ImGuiCol_TextSelectedBg]       = ImVec4(accent.x, accent.y, accent.z, 0.28f);
        colors[ImGuiCol_DragDropTarget]       = ImVec4(accent.x, accent.y, accent.z, 0.70f);
        colors[ImGuiCol_NavHighlight]         = ImVec4(accent.x, accent.y, accent.z, 0.40f);
        colors[ImGuiCol_NavWindowingHighlight]= ImVec4(1, 1, 1, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]    = ImVec4(0, 0, 0, 0.30f);
        colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0, 0, 0, 0.60f);
        colors[ImGuiCol_DockingPreview]       = ImVec4(accent.x, accent.y, accent.z, 0.35f);
        colors[ImGuiCol_DockingEmptyBg]       = baseBg;

        // Leave font scaling neutral; caller may override later.
        ImGui::GetIO().FontGlobalScale = 1.0f;
    }

} // namespace EditorTheme