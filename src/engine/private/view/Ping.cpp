#include <view/Ping.h>
#include <imgui.h>
#include <limits>

#ifdef _WIN32
    #ifdef stdout
    #pragma push_macro("stdout")
    #undef stdout
    #define RESTORE_STDOUT_MACRO
    #endif
#endif

void
mdns::engine::ui::renderPingTool(PingTool::PingStats const& stats,
                                 std::string const& stdout,
                                 std::function<void()> onStop)
{
  ImGuiStyle const& style = ImGui::GetStyle();

  ImGui::SameLine();
  ImGui::BeginGroup();

  float childWidth = 800.0f;
  float childHeight = ImGui::GetContentRegionAvail().y;

  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.14f, 0.16f, 1.0f));

  ImGui::BeginChild("PingToolCard", ImVec2(childWidth, childHeight), true);
  ImGui::Indent(18);

  ImGui::Dummy(ImVec2(0.0f, style.FramePadding.y * 3.0f));

  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(2);

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
  ImGui::SetWindowFontScale(1.1f);
  ImGui::TextUnformatted("Ping Tool");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24);

  ImVec4 cardBg = ImVec4(0.13f, 0.14f, 0.16f, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Button, cardBg);
  ImGui::PushStyleColor(
  ImGuiCol_ButtonHovered,
    ImVec4(cardBg.x + 0.05f, cardBg.y + 0.05f, cardBg.z + 0.05f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, cardBg);
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
  if (ImGui::Button("X##ClosePing")) {
    onStop();
  }

  ImGui::PopStyleColor(4);
  ImGui::PopStyleVar();
  ImGui::Dummy(ImVec2(0.0f, style.FramePadding.y * 0.5f));

  ImGui::TextDisabled("Statistics");
  ImGui::Columns(2, "stats", false);
  ImGui::SetColumnWidth(0, 120);

  ImGui::Text("Sent: %d", stats.send);
  ImGui::Text("Received: %d", stats.received);
  ImGui::Text("Lost: %d", stats.lost);

  ImGui::NextColumn();

  ImGui::Text("Min: %d ms", stats.min == std::numeric_limits<int>::max() ? 0 : stats.min);
  ImGui::Text("Max: %d ms", stats.max);
  ImGui::Text("Avg: %d ms", stats.average);

  ImGui::Columns(1);
  ImGui::Spacing();
  ImGui::Dummy(ImVec2(0.0f, style.FramePadding.y * 1.0f));

  ImGui::TextDisabled("Response Time");

  static int historyOffset = 0;
  ImGui::PlotLines("##PingGraph",
                   stats.history.data(),
                   stats.history.size(),
                   historyOffset,
                   nullptr,
                   0.0f,
                   100.0f,
                   ImVec2(ImGui::GetContentRegionAvail().x - 16.0f, 120));

  ImGui::Spacing();
  ImGui::Dummy(ImVec2(0.0f, style.FramePadding.y * 1.0f));
  ImGui::TextDisabled("Standard output");

  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.19f, 0.22f, 1.0f));
  ImGui::BeginChild("PingOutput",
                    ImVec2(ImGui::GetContentRegionAvail().x - 16.0f,
                           ImGui::GetContentRegionAvail().y - 16.0f),
                    false,
                    ImGuiWindowFlags_NoScrollbar);
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(2);

  ImGui::SetWindowFontScale(0.875f);
  ImGui::Indent(12);
  ImGui::TextUnformatted(stdout.c_str());
  ImGui::Unindent(12);
  ImGui::SetWindowFontScale(1.0f);
  ImGui::Dummy(ImVec2(0.0f, 0.3f));

  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::Unindent(18);
  ImGui::EndChild();
  ImGui::EndChild();
  ImGui::EndGroup();
}

#ifdef RESTORE_STDOUT_MACRO
    #pragma pop_macro("stdout")
    #undef RESTORE_STDOUT_MACRO
#endif