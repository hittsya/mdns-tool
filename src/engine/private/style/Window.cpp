#include <style/Window.h>

void mdns::engine::ui::pushThemedWindowStyles()
{
    ImGuiStyle const& style = ImGui::GetStyle();

    ImVec4 bg = ImVec4(0.15f, 0.16f, 0.18f, 0.96f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1,1,1,0.08f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, bg);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, bg);
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, bg);
}

void mdns::engine::ui::popThemedWindowStyles()
{
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
}

void mdns::engine::ui::pushThemedPopupStyles()
{
    ImVec4 popupBg  = ImVec4(0.15f, 0.16f, 0.18f, 0.98f);
    ImVec4 border   = ImVec4(1, 1, 1, 0.08f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 6));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, popupBg);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1,1,1,0.06f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1,1,1,0.10f));
}

void mdns::engine::ui::popThemedPopupStyles()
{
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(4);
}