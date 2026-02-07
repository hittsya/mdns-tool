#include <view/Services.h>
#include <Util.h>
#include <imgui.h>

static float calcServiceCardHeight(std::size_t ipCount)
{
    ImGuiStyle const &style = ImGui::GetStyle();
    ImGuiIO const &io = ImGui::GetIO();

    const float line = ImGui::GetTextLineHeight();
    const float spacing = style.ItemSpacing.y;
    const float padding = style.WindowPadding.y * 2.0f;
    const float frame = style.FramePadding.y * 2.0f;

    float height = 0.0f;
    height += line * 1.1f + spacing;
    height += 4.0f;
    height += line + spacing;
    height += 3.0f;
    height += line;
    height += ipCount * (line + spacing);
    height += 3.0f;
    height += line + spacing;
    height += 8.0f;
    height += ImGui::GetFrameHeight() + spacing;
    height += padding + frame;

    return height;
}

void mdns::engine::ui::renderServiceLayout(std::vector<ScanCardEntry> const& discovered_services, std::function<void(std::string const&)> onOpenPingTool, std::function<void(ScanCardEntry entry)> onOpenDissectorMeta) 
{
    float availHeight = ImGui::GetContentRegionAvail().y;
    float halfHeight = availHeight * 0.75f;

    ImGui::BeginChild("ServicesChild", ImVec2(0, halfHeight), true);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 4));
    ImGui::BeginChild("ServicesHeader", ImVec2(0, 54), false, ImGuiWindowFlags_NoScrollbar);

    ImVec2 hmin = ImGui::GetWindowPos();
    ImVec2 hmax = ImVec2(hmin.x + ImGui::GetWindowSize().x, hmin.y + ImGui::GetWindowSize().y);
    ImGui::GetWindowDrawList()->AddRectFilled(hmin, hmax, IM_COL32(60, 70, 85, 255));

    ImGui::Indent(15);
    ImGui::Dummy(ImVec2(0.0f, 0.35f));
    ImGui::TextUnformatted("Discovered Services");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.9f, 1.0f, 1.0f));
    ImGui::TextUnformatted("Devices/applications currently advertising services on the network");
    ImGui::PopStyleColor();
    ImGui::Unindent(15);

    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::BeginChild("ServicesScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    for (size_t i = 0; i < discovered_services.size(); ++i) {
        renderServiceCard((int)i, discovered_services[i], onOpenPingTool, onOpenDissectorMeta);
    }

    ImGui::EndChild();
    ImGui::EndChild();

}

void mdns::engine::ui::renderServiceCard(int index, ScanCardEntry const& entry, std::function<void(std::string const&)> onOpenPingTool, std::function<void(ScanCardEntry entry)> onOpenDissectorMeta)
{
    if (entry.name.empty()) {
        return;
    }

    auto const height = calcServiceCardHeight(entry.ip_addresses.size());

    ImGui::PushID(index);
    ImGui::BeginChild("ServiceCard", ImVec2(0, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiChildFlags_AutoResizeY);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::SetWindowFontScale(1.1f);

    float windowWidth = ImGui::GetContentRegionAvail().x;
    float textWidth = ImGui::CalcTextSize(entry.name.c_str()).x;

    ImGui::SeparatorText(entry.name.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::Text("Hostname:");
    ImGui::SameLine(250);
    ImGui::Text("%s", entry.name.c_str());

    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    ImGui::Text("IP Address(es):");
    ImGui::SameLine(250);
    ImGui::Indent(235);

    for (auto const &ipAddr : entry.ip_addresses)
    {
        ImGui::PushID(ipAddr.c_str());

        if (ImGui::Selectable(ipAddr.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
            ImGui::OpenPopup("ip_actions_popup");
        }

        if (ImGui::BeginPopup("ip_actions_popup"))
        {
            ImGui::Text     ("IP: %s", ipAddr.c_str());
            ImGui::Separator();

            if (ImGui::Button("Ping")) {   
                onOpenPingTool(ipAddr);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();

            
            if (ImGui::Button("Open in browser")) {
                std::string url = entry.name.find("https") != std::string::npos ? "https" : "http";

                url += "://";
                url += ipAddr;

                if (entry.port != mdns::proto::port) {
                    url += ":" + std::to_string(entry.port);
                }

                mdns::engine::util::openInBrowser(url);
            }
            
            ImGui::SameLine();
            
            auto const port = entry.name.find("_ssh") != std::string::npos ? entry.port: 22;
            if (ImGui::Button(fmt::format("SSH root@{}:{}", ipAddr, port).c_str())) {
                mdns::engine::util::openShellAndSSH(ipAddr, "root", port);
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::Unindent(235);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));

    ImGui::Text("Port:");
    ImGui::SameLine(250);
    ImGui::Text("%d", entry.port);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (ImGui::Button("Open in browser")) {
        std::string url = entry.name.find("https") != std::string::npos ? "https" : "http";

        url += "://";
        url += entry.name;

        if (entry.port != mdns::proto::port) {
            url += ":" + std::to_string(entry.port);
        }

        mdns::engine::util::openInBrowser(url);
    }

    ImGui::SameLine();
    
    auto const port = entry.name.find("_ssh") != std::string::npos ? entry.port: 22;
    if (ImGui::Button(fmt::format("SSH root@{}:{}", entry.name, port).c_str())) {
        mdns::engine::util::openShellAndSSH(entry.name, "root", port);
    }

    ImGui::SameLine();
    ImVec4 bg = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
    ImVec4 bg_hover = ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
    ImVec4 bg_active = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg_active);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));

    if (ImGui::Button("Dissector metadata")) {
        onOpenDissectorMeta(entry);
    }

    ImGui::PopStyleColor(4);
    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::PopID();
}