#include <view/Questions.h>
#include <chrono>
#include <imgui.h>

void mdns::engine::ui::renderQuestionLayout(std::vector<QuestionCardEntry> const& intercepted_questions) 
{
    
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.14f, 0.16f, 1.0f));
    ImGui::BeginChild("QuestionsChild", ImVec2(0, 0), true);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGuiStyle const& style = ImGui::GetStyle();    
    ImGuiIO const& io = ImGui::GetIO();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.ItemSpacing.y));

    float headerHeight = ImGui::GetTextLineHeight() + style.ItemSpacing.y + style.FramePadding.y * 8.0f;
    ImGui::BeginChild("QuestionsHeader", ImVec2(0, headerHeight), false, ImGuiWindowFlags_NoScrollbar);

    // ImDrawList* draw = ImGui::GetWindowDrawList();
    // ImVec2 p0        = ImGui::GetWindowPos();
    // ImVec2 p1        = ImVec2(p0.x + ImGui::GetWindowWidth(), p0.y + ImGui::GetWindowHeight());
    // draw->AddRectFilled(p0, p1, IM_COL32(60, 70, 85, 255));

    ImGui::Dummy          (ImVec2(0.0f, style.FramePadding.y * 3.0f));
    ImGui::Indent         (style.FramePadding.x * 4.0f);
    ImGui::SetWindowFontScale(1.1f);
    ImGui::TextUnformatted("Intercepted mDNS questions");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Unindent       (style.FramePadding.x * 4.0f);
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::BeginChild("QuestionsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    
    ImGui::Indent(18);
    for (size_t i = 0; i < intercepted_questions.size(); ++i) {
        renderQuestionCard(
            (int)i,
            intercepted_questions[i].name,
            intercepted_questions[i].ip_addresses[0],
            intercepted_questions[i].time_of_arrival
        );

        ImGui::Dummy(ImVec2(0.0f, 2.5f));
    }
    ImGui::Unindent(18);

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::EndChild();
}

void mdns::engine::ui::renderQuestionCard(int index, std::string const &name, std::string ipAddrs, std::chrono::steady_clock::time_point const& toa) 
{
    if (name.empty()) {
        return;
    }

    ImGuiStyle const &style = ImGui::GetStyle();
    ImGuiIO const &io = ImGui::GetIO();
    float height = ImGui::GetTextLineHeight() + ImGui::GetFrameHeight() + style.FramePadding.y + 1.0f;

    ImGui::PushID(index);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, ImGui::GetStyle().WindowPadding.y));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.19f, 0.22f, 1.0f));
    ImGui::BeginChild("QuestionCard", ImVec2(ImGui::GetContentRegionAvail().x - 18.0f, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    ImGui::Indent(21);

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0.0f, 0.5f));

    ImGui::Text("Who has");
    ImGui::SameLine();

    // TODO: Bold text
    ImVec2 pos       = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImU32 col        = ImGui::GetColorU32(ImGuiCol_Text);

    draw->AddText(pos, col, name.c_str());
    draw->AddText(ImVec2(pos.x + 1, pos.y), col, name.c_str());

    ImGui::Dummy(ImGui::CalcTextSize(name.c_str()));

    ImGui::SameLine();
    ImGui::Text("? -- via %s", ipAddrs.c_str());

    auto const now = std::chrono::steady_clock::now();
    auto const age = duration_cast<std::chrono::seconds>(now - toa).count();

    char buf[64];
    if (age < 60) {
        std::snprintf(buf, sizeof(buf), "%ld seconds ago", age);
    }
    else if (age < 3600) {
        std::snprintf(buf, sizeof(buf), "%ld minutes ago", age / 60);
    }
    else {
        std::snprintf(buf, sizeof(buf), "%ld hours ago", age / 3600);
    }

    float tsWidth = ImGui::CalcTextSize(buf).x;
    float rightX  = ImGui::GetWindowContentRegionMax().x - (tsWidth + 25.0f);
    ImGui::SameLine();
    ImGui::SetCursorPosX(rightX);
    ImGui::TextDisabled("%s", buf);

    ImGui::Unindent(21);
    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::PopID();
}