#include "EditorTheme.h"

namespace EditorTheme
{
    void SetupImGuiStyle()
    {
        // Bold, distinct theme: dark slate base with vibrant teal/magenta accents
        ImGuiStyle &style = ImGui::GetStyle();

        style.Alpha = 1.0f;
        style.DisabledAlpha = 0.5f;
        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.WindowRounding = 8.0f;
        style.WindowBorderSize = 1.0f;
        style.WindowMinSize = ImVec2(32.0f, 32.0f);
        style.WindowTitleAlign = ImVec2(0.05f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_Left;
        style.ChildRounding = 6.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupRounding = 6.0f;
        style.PopupBorderSize = 1.0f;
        style.FramePadding = ImVec2(8.0f, 6.0f);
        style.FrameRounding = 6.0f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.CellPadding = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 18.0f;
        style.ColumnsMinSpacing = 6.0f;
        style.ScrollbarSize = 14.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabMinSize = 10.0f;
        style.GrabRounding = 6.0f;
        style.TabRounding = 6.0f;
        style.TabBorderSize = 1.0f;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.5f);

        ImVec4 bg0 = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
        ImVec4 bg1 = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
        ImVec4 bg2 = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
        ImVec4 ac1 = ImVec4(0.05f, 0.80f, 0.75f, 1.00f); // teal
        ImVec4 ac2 = ImVec4(0.90f, 0.25f, 0.60f, 1.00f); // magenta
        ImVec4 tx0 = ImVec4(0.92f, 0.96f, 1.00f, 1.00f);
        ImVec4 tx1 = ImVec4(0.60f, 0.70f, 0.80f, 1.00f);

        style.Colors[ImGuiCol_Text] = tx0;
        style.Colors[ImGuiCol_TextDisabled] = tx1;
        style.Colors[ImGuiCol_WindowBg] = bg0;
        style.Colors[ImGuiCol_ChildBg] = bg1;
        style.Colors[ImGuiCol_PopupBg] = bg1;
        style.Colors[ImGuiCol_Border] = ImVec4(0.20f, 0.25f, 0.30f, 1.0f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        style.Colors[ImGuiCol_FrameBg] = bg2;
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(ac1.x, ac1.y, ac1.z, 0.25f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(ac1.x, ac1.y, ac1.z, 0.45f);
        style.Colors[ImGuiCol_TitleBg] = bg1;
        style.Colors[ImGuiCol_TitleBgActive] = bg2;
        style.Colors[ImGuiCol_TitleBgCollapsed] = bg1;
        style.Colors[ImGuiCol_MenuBarBg] = bg1;
        style.Colors[ImGuiCol_ScrollbarBg] = bg1;
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.30f, 0.36f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.36f, 0.42f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.42f, 0.48f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ac1;
        style.Colors[ImGuiCol_SliderGrab] = ac1;
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(ac1.x, ac1.y, ac1.z, 0.9f);
        style.Colors[ImGuiCol_Button] = ImVec4(ac1.x, ac1.y, ac1.z, 0.20f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(ac1.x, ac1.y, ac1.z, 0.35f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(ac1.x, ac1.y, ac1.z, 0.55f);
        style.Colors[ImGuiCol_Header] = ImVec4(ac2.x, ac2.y, ac2.z, 0.20f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(ac2.x, ac2.y, ac2.z, 0.35f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(ac2.x, ac2.y, ac2.z, 0.55f);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.27f, 0.32f, 1.0f);
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(ac1.x, ac1.y, ac1.z, 0.60f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(ac1.x, ac1.y, ac1.z, 0.80f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(ac2.x, ac2.y, ac2.z, 0.40f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(ac2.x, ac2.y, ac2.z, 0.75f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(ac2.x, ac2.y, ac2.z, 0.45f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.20f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.12f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.18f, 0.22f, 1.0f);
        style.Colors[ImGuiCol_PlotLines] = ac1;
        style.Colors[ImGuiCol_PlotLinesHovered] = ac2;
        style.Colors[ImGuiCol_PlotHistogram] = ac2;
        style.Colors[ImGuiCol_PlotHistogramHovered] = ac1;
        style.Colors[ImGuiCol_TableHeaderBg] = bg2;
        style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.20f, 0.25f, 0.32f, 1.0f);
        style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.16f, 0.20f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
        style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(ac1.x, ac1.y, ac1.z, 0.35f);
        style.Colors[ImGuiCol_DragDropTarget] = ImVec4(ac2.x, ac2.y, ac2.z, 0.85f);
        style.Colors[ImGuiCol_NavHighlight] = ImVec4(ac1.x, ac1.y, ac1.z, 0.75f);
        style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.20f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.50f);

        // Slightly upscale default font for better readability
        ImGui::GetIO().FontGlobalScale = 1.0f; // keep neutral; caller may override
    }

} // namespace EditorTheme