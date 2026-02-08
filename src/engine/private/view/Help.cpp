#include <view/Help.h>
#include <style/Window.h>

#include <MetaData.h>
#include <imgui.h>

void mdns::engine::ui::renderChangelogWindow(bool* show)
{
    mdns::engine::ui::pushThemedWindowStyles();
    ImGui::Begin("Changelog", show, ImGuiWindowFlags_AlwaysAutoResize);
    mdns::engine::ui::popThemedWindowStyles();

    ImGui::Text(mdns::meta::CHANGELOG);
    ImGui::End();
}

void mdns::engine::ui::renderHelpWindow(bool* show, const char* title)
{
    mdns::engine::ui::pushThemedWindowStyles();
    ImGui::Begin("Help", show, ImGuiWindowFlags_AlwaysAutoResize);
    mdns::engine::ui::popThemedWindowStyles();

    auto center_text = [](const char *text) {
        float window_width = ImGui::GetWindowSize().x;
        float text_width   = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((window_width - text_width) * 0.5f);
        ImGui::TextUnformatted(text);
    };

    ImGui::Spacing();
    center_text(title);
    ImGui::Separator();
    ImGui::Spacing();

    center_text("Source code:");
    ImGui::Spacing();

    float link_width = ImGui::CalcTextSize(mdns::meta::GITHUB_URL).x;
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - link_width) * 0.5f);

    if (ImGui::Selectable(mdns::meta::GITHUB_URL, false)) {
        ImGui::SetClipboardText(mdns::meta::GITHUB_URL);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("(click to copy link)");
    ImGui::End();
}