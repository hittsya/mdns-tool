#include <view/Help.h>
#include <ChangeLog.h>
#include <imgui.h>

void mdns::engine::ui::renderChangelogWindow(bool* show) {
    ImGui::Begin("Changelog", show, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text(mdns::meta::CHANGELOG);
    ImGui::End();
}

void mdns::engine::ui::renderHelpWindow(bool* show, const char* title) {
    ImGui::Begin("Help", show, ImGuiWindowFlags_AlwaysAutoResize);

    auto center_text = [](const char *text) {
        float window_width = ImGui::GetWindowSize().x;
        float text_width = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((window_width - text_width) * 0.5f);
        ImGui::TextUnformatted(text);
    };

    ImGui::Spacing();
    center_text(title);
    ImGui::Separator();
    ImGui::Spacing();

    const char *github_url = "https://github.com/hittsya/mdns-tool";
    center_text("Source code:");
    ImGui::Spacing();

    float link_width = ImGui::CalcTextSize(github_url).x;
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - link_width) * 0.5f);

    if (ImGui::Selectable(github_url, false)) {
        ImGui::SetClipboardText(github_url);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("(click to copy link)");
    ImGui::End();
}