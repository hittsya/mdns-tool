#include <view/Ping.h>
#include <limits>
#include <imgui.h>

void mdns::engine::ui::renderPingTool(PingTool::PingStats const& stats, std::string const& stdout, std::function<void()> onStop) 
{
    ImGui::SameLine();
    ImGui::BeginGroup();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::Text("Ping Tool");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
    
    if (ImGui::Button("X##ClosePing")) {
        onStop();
    }

    ImGui::PopStyleVar();
    ImGui::Separator();
    ImGui::BeginChild("PingPanel", ImVec2(600, 0), true);

    ImGui::Text("Statistics:");
    ImGui::Columns(2, "stats", false);
    ImGui::Text("Sent: %d", stats.send);
    ImGui::Text("Received: %d", stats.received);
    ImGui::Text("Lost: %d", stats.lost);
    ImGui::NextColumn();
    ImGui::Text("Min: %d ms", stats.min == std::numeric_limits<int>::max() ? 0 : stats.min);
    ImGui::Text("Max: %d ms", stats.max);
    ImGui::Text("Avg: %d ms", stats.average);
    ImGui::Columns(1);

    ImGui::Separator();

    ImGui::Text("Response Time");
    static int historyOffset = 0;

    ImGui::PlotLines("##PingGraph", stats.history.data(), stats.history.size(), historyOffset, nullptr, 0.0f, 100.0f, ImVec2(-1, 80));
    ImGui::Separator();

    ImGui::Text("Output:");
    ImGui::BeginChild("PingOutput", ImVec2(0, 0), true);

    ImGui::Text(stdout.data());

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::EndGroup();   
}