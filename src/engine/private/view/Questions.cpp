#include <view/Questions.h>
#include <imgui.h>

void mdns::engine::ui::renderQuestionLayout(std::vector<QuestionCardEntry> const& intercepted_questions) 
{
    ImGui::BeginChild("QuestionsChild", ImVec2(0, 0), true);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 4));
    ImGui::BeginChild("QuestionsHeader", ImVec2(0, 54), false, ImGuiWindowFlags_NoScrollbar);

    ImVec2 qmin = ImGui::GetWindowPos();
    ImVec2 qmax = ImVec2(qmin.x + ImGui::GetWindowSize().x, qmin.y + ImGui::GetWindowSize().y);
    ImGui::GetWindowDrawList()->AddRectFilled(qmin, qmax, IM_COL32(60, 70, 85, 255));

    ImGui::Indent(15);
    ImGui::Dummy(ImVec2(0.0f, 0.35f));
    ImGui::TextUnformatted("Service Discovery Requests");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.9f, 1.0f, 1.0f));
    ImGui::TextUnformatted("Intercepted network queries from devices that are searching for services");
    ImGui::PopStyleColor();
    ImGui::Unindent(15);

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::BeginChild("QuestionsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    for (size_t i = 0; i < intercepted_questions.size(); ++i) {
        renderQuestionCard(
            (int)i,
            intercepted_questions[i].name,
            intercepted_questions[i].ip_addresses[0]
        );
    }

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::EndChild();
}

void mdns::engine::ui::renderQuestionCard(int index, std::string const &name, std::string ipAddrs) 
{
    if (name.empty()) {
        return;
    }

    ImGuiStyle const &style = ImGui::GetStyle();
    ImGuiIO const &io = ImGui::GetIO();
    float height = ImGui::GetTextLineHeight() + ImGui::GetFrameHeight() + style.FramePadding.y + 1.0f;

    ImGui::PushID(index);
    ImGui::BeginChild("QuestionCard", ImVec2(0, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiChildFlags_AutoResizeY);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    float windowWidth = ImGui::GetContentRegionAvail().x;
    float textWidth = ImGui::CalcTextSize(name.c_str()).x;

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0.0f, 0.5f));

    ImGui::Text("Who has");
    ImGui::SameLine();

    // TODO: Bold text
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);

    draw->AddText(pos, col, name.c_str());
    draw->AddText(ImVec2(pos.x + 1, pos.y), col, name.c_str());

    ImGui::Dummy(ImGui::CalcTextSize(name.c_str()));

    ImGui::SameLine();
    ImGui::Text("? -- via %s", ipAddrs.c_str());

    ImGui::Dummy(ImVec2(0.0f, 1.0f));

    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::PopID();
}