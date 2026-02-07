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
    ImGuiStyle const& style = ImGui::GetStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.14f, 0.16f, 1.0f));
    ImGui::BeginChild("ServicesChild", ImVec2(0, availHeight * 0.75f), true);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGuiIO const& io = ImGui::GetIO();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.ItemSpacing.y));

    float headerHeight = ImGui::GetTextLineHeight() + style.ItemSpacing.y + style.FramePadding.y * 8.0f;
    ImGui::BeginChild("ServicesHeader", ImVec2(0, headerHeight), false, ImGuiWindowFlags_NoScrollbar);

    // ImDrawList* draw = ImGui::GetWindowDrawList();
    // ImVec2 p0        = ImGui::GetWindowPos();
    // ImVec2 p1        = ImVec2(p0.x + ImGui::GetWindowWidth(), p0.y + ImGui::GetWindowHeight());
    // draw->AddRectFilled(p0, p1, IM_COL32(60, 70, 85, 255));

    ImGui::Dummy          (ImVec2(0.0f, style.FramePadding.y * 3.0f));
    ImGui::Indent         (style.FramePadding.x * 4.0f);
    ImGui::SetWindowFontScale(1.1f);
    ImGui::TextUnformatted("Discovered Services");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Unindent       (style.FramePadding.x * 4.0f);
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::BeginChild("ServicesScroll", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    
    float regionWidth  = ImGui::GetContentRegionAvail().x;
    float minCardWidth = 875.0f;
    float spacing      = ImGui::GetStyle().ItemSpacing.x * 4;

    int cardsPerRow = (int)(regionWidth / (minCardWidth + spacing));
    cardsPerRow     = std::max(1, cardsPerRow);
    float cardWidth = (regionWidth - (cardsPerRow - 1) * spacing) / cardsPerRow;

    ImGui::Indent(18);
    for (size_t i = 0; i < discovered_services.size(); ++i) {
        renderServiceCard((int)i, discovered_services[i], cardWidth, onOpenPingTool, onOpenDissectorMeta);

        if ((i + 1) % cardsPerRow != 0) {
            ImGui::SameLine();
        } else {
            ImGui::Dummy(ImVec2(0.0f, 3.5f));
        }
    }
    ImGui::Unindent(18);

    ImGui::EndChild();
    ImGui::EndChild();
}

void mdns::engine::ui::renderServiceCard(int index, ScanCardEntry const& entry, float cardWidth, std::function<void(std::string const&)> onOpenPingTool, std::function<void(ScanCardEntry entry)> onOpenDissectorMeta)
{
    if (entry.name.empty()) {
        return;
    }

    auto const height = calcServiceCardHeight(entry.ip_addresses.size());

    ImGui::PushID(index);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, ImGui::GetStyle().WindowPadding.y));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.19f, 0.22f, 1.0f));
    ImGui::BeginChild("ServiceCard", ImVec2(cardWidth, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiChildFlags_AutoResizeY);    
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::SetWindowFontScale(1.1f);

    float textWidth         = ImGui::CalcTextSize(entry.name.c_str()).x;
    auto const nameStripped = mdns::engine::util::stripMdnsServicePostfix(entry.name);
    ImVec2 textSize         = ImGui::CalcTextSize(nameStripped.c_str());

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    ImGui::Indent(21);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - textSize.x) * 0.5f);
    ImGui::TextUnformatted(nameStripped.c_str());

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::Text("Hostname:");
    ImGui::SameLine(250);
    ImGui::Text("%s", entry.name.c_str());

    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    ImGui::Text("IP Address(es):");
    ImGui::SameLine(250);
    ImGui::Indent(228);

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

    ImGui::Unindent(228);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));

    ImGui::Text("Port:");
    ImGui::SameLine(250);
    ImGui::Text("%d", entry.port);

    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    ImGui::Text("Last update:");
    ImGui::SameLine(250);
    
    auto now = std::chrono::steady_clock::now();
    auto age = duration_cast<std::chrono::seconds>(now - entry.time_of_arrival).count();

    ImVec4 color;
    if (age <= 10)      color = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
    else if (age <= 15) color = ImVec4(0.95f, 0.8f, 0.2f, 1.0f);
    else                color = ImVec4(0.95f, 0.2f, 0.2f, 1.0f);

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

    ImGui::TextColored(color, "%s", buf);
    ImGui::Unindent(21);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    auto const ssh  = entry.name.find("_ssh") != std::string::npos;
    auto const port = ssh ? entry.port: 22;
    auto const name = mdns::engine::util::stripMdnsServicePostfix(entry.name);

    if (ImGui::Button("Open in browser")) {
        std::string url = entry.name.find("https") != std::string::npos ? "https" : "http";

        url += "://";
        url += name;

        if (entry.port != mdns::proto::port) {
            url += ":" + std::to_string(entry.port);
        }

        mdns::engine::util::openInBrowser(url);
    }

    ImGui::SameLine();
    if (ImGui::Button(fmt::format("SSH root@{}:{}", name, port).c_str())) {
        mdns::engine::util::openShellAndSSH(name, "root", port);
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
    ImGui::PopID();
}