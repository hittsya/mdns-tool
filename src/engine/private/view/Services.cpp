#include <Util.h>
#include <imgui.h>
#include <style/Button.h>
#include <style/Window.h>
#include <view/Services.h>

#include "imgui_internal.h"

#ifdef _WIN32
#undef max
#undef min
#endif

static float
calcServiceCardHeight(std::size_t ipCount)
{
  ImGuiStyle const& style = ImGui::GetStyle();
  ImGuiIO const& io = ImGui::GetIO();

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
  height += 5.0f;
  height += line + spacing;
  height += 8.0f;
  height += ImGui::GetFrameHeight() + spacing;
  height += padding + frame;

  return height;
}

void
mdns::engine::ui::renderServiceLayout(
  std::vector<ScanCardEntry> const& discovered_services,
  std::function<void(std::string const&)> onOpenPingTool,
  std::function<void()> onQuestionWindowOpen,
  std::function<void(ScanCardEntry entry)> onOpenDissectorMeta,
  unsigned int browser_texture,
  unsigned int info_texture,
  unsigned int terminal_texture,
  std::vector<std::string> const& questions)
{
  float availHeight = ImGui::GetContentRegionAvail().y;
  ImGuiStyle const& style = ImGui::GetStyle();

  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.14f, 0.16f, 1.0f));
  ImGui::BeginChild("ServicesChild", ImVec2(0, availHeight * 0.75f), true);
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(2);

  ImGuiIO const& io = ImGui::GetIO();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(style.ItemSpacing.x, style.ItemSpacing.y));

  float headerHeight = ImGui::GetTextLineHeight() + style.ItemSpacing.y +
                       style.FramePadding.y * 8.0f;
  ImGui::BeginChild("ServicesHeader",
                    ImVec2(0, headerHeight),
                    false,
                    ImGuiWindowFlags_NoScrollbar);

  ImGui::Dummy(ImVec2(0.0f, style.FramePadding.y * 3.0f));
  ImGui::Indent(style.FramePadding.x * 4.0f);
  ImGui::SetWindowFontScale(1.1f);
  ImGui::TextUnformatted("Discovered Services");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::Unindent(style.FramePadding.x * 4.0f);
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::Indent(18);

  mdns::engine::ui::pushThemedButtonStyles(ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
  float avail = ImGui::GetContentRegionAvail().x;
  float x = 0.0f;

  for (auto const& question : questions) {
    if (question.starts_with("_")) {
      continue;
    }

    ImGui::PushID(&question);

    ImVec2 textSize = ImGui::CalcTextSize(question.data());
    ImVec2 buttonSize = ImGui::CalcItemSize(
      ImVec2(textSize.x + ImGui::GetStyle().FramePadding.x * 2,
             textSize.y + ImGui::GetStyle().FramePadding.y * 2),
      0.0f,
      0.0f);

    if (x + buttonSize.x > avail) {
      ImGui::NewLine();
      x = 0.0f;
    }

    if (ImGui::Button(question.data(), buttonSize)) {
    }

    x += buttonSize.x + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
    ImGui::PopID();
  }
  mdns::engine::ui::popThemedButtonStyles();

  mdns::engine::ui::pushThemedButtonStyles(ImVec4(0.55f, 0.57f, 0.60f, 1.0f));
  if (ImGui::Button("Add question")) {
    onQuestionWindowOpen();
  }
  mdns::engine::ui::popThemedButtonStyles();

  ImGui::Dummy(ImVec2(0.0f, 2.5f));

  ImGui::BeginChild(
    "ServicesScroll", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

  float regionWidth = ImGui::GetContentRegionAvail().x;
  float minCardWidth = 875.0f;
  float spacing = ImGui::GetStyle().ItemSpacing.x * 4;

  int cardsPerRow = (int)(regionWidth / (minCardWidth + spacing));
  cardsPerRow = std::max(1, cardsPerRow);
  float cardWidth = (regionWidth - (cardsPerRow - 1) * spacing) / cardsPerRow;

  for (size_t skipped = 0, i = 0; i < discovered_services.size(); ++i) {
    if (discovered_services[i].name.empty()) {
      ++skipped;
      continue;
    }

    auto const index = i - skipped;
    renderServiceCard(static_cast<int>(index),
                      discovered_services[i],
                      cardWidth,
                      onOpenPingTool,
                      onOpenDissectorMeta,
                      browser_texture,
                      info_texture,
                      terminal_texture);

    if ((index + 1) % cardsPerRow != 0) {
      ImGui::SameLine();
    } else {
      ImGui::Dummy(ImVec2(0.0f, 3.5f));
    }
  }
  ImGui::Unindent(18);

  ImGui::EndChild();
  ImGui::EndChild();
}

void
mdns::engine::ui::renderServiceCard(
  int index,
  ScanCardEntry const& entry,
  float cardWidth,
  std::function<void(std::string const&)> onOpenPingTool,
  std::function<void(ScanCardEntry entry)> onOpenDissectorMeta,
  unsigned int browser_texture,
  unsigned int info_texture,
  unsigned int terminal_texture)
{
  auto const height = calcServiceCardHeight(entry.ip_addresses.size());

  ImGui::PushID(index);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(16.0f, ImGui::GetStyle().WindowPadding.y));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.19f, 0.22f, 1.0f));
  ImGui::BeginChild("ServiceCard",
                    ImVec2(cardWidth, height),
                    true,
                    ImGuiWindowFlags_NoScrollbar | ImGuiChildFlags_AutoResizeY);
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(3);

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
  ImGui::SetWindowFontScale(1.1f);

  float textWidth = ImGui::CalcTextSize(entry.name.c_str()).x;

  ImGui::Dummy(ImVec2(0.0f, 5.0f));
  ImGui::Indent(21);

  std::string nameSliced = entry.name;
  if (nameSliced.size() > 45) {
    nameSliced = nameSliced.substr(0, 45) + "...";
  }

  auto const nameStripped = util::stripMdnsServicePostfix(nameSliced);
  ImVec2 textSize = ImGui::CalcTextSize(nameStripped.c_str());

  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       (ImGui::GetContentRegionAvail().x - textSize.x) * 0.5f);
  ImGui::TextUnformatted(nameStripped.c_str());

  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleVar();
  ImGui::Dummy(ImVec2(0.0f, 4.0f));

  ImGui::Text("Hostname:");
  ImGui::SameLine(250);
  ImGui::Text("%s", nameSliced.c_str());

  ImGui::Dummy(ImVec2(0.0f, 3.0f));

  ImGui::Text("Type:");
  ImGui::SameLine(250);
  ImGui::Text("%s", util::stripMdnsServicePrefix(entry.name).c_str());

  ImGui::Dummy(ImVec2(0.0f, 3.0f));

  ImGui::Text("IP Address(es):");
  ImGui::SameLine(250);
  ImGui::Indent(228);

  mdns::engine::ui::pushThemedPopupStyles();
  for (auto const& ipAddr : entry.ip_addresses) {
    ImGui::PushID(ipAddr.c_str());

    if (ImGui::Selectable(
          ipAddr.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
      ImGui::OpenPopup("ip_actions_popup");
    }

    mdns::engine::ui::pushThemedButtonStyles(ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
    if (ImGui::BeginPopup("ip_actions_popup")) {
      ImGui::Text("IP: %s", ipAddr.c_str());
      ImGui::Separator();

      if (ImGui::Button("Ping")) {
        onOpenPingTool(ipAddr);
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();

      if (ImGui::Button("Open in browser")) {
        std::string url =
          entry.name.find("https") != std::string::npos ? "https" : "http";

        url += "://";
        url += ipAddr;

        if (entry.port != mdns::proto::port) {
          url += ":" + std::to_string(entry.port);
        }

        mdns::engine::util::openInBrowser(url);
      }

      ImGui::SameLine();

      auto const port =
        entry.name.find("_ssh") != std::string::npos ? entry.port : 22;
      if (ImGui::Button(fmt::format("SSH root@{}:{}", ipAddr, port).c_str())) {
        mdns::engine::util::openShellAndSSH(ipAddr, "root", port);
      }

      ImGui::EndPopup();
    }
    mdns::engine::ui::popThemedButtonStyles();

    ImGui::PopID();
  }
  mdns::engine::ui::popThemedPopupStyles();

  ImGui::Unindent(228);
  ImGui::Dummy(ImVec2(0.0f, 3.0f));

  ImGui::Text("Port:");
  ImGui::SameLine(250);
  ImGui::Text("%d", entry.port);

  ImGui::Dummy(ImVec2(0.0f, 3.0f));
  ImGui::Text("Last update:");
  ImGui::SameLine(250);

  auto now = std::chrono::steady_clock::now();
  auto age =
    duration_cast<std::chrono::seconds>(now - entry.time_of_arrival).count();

  ImVec4 color;
  if (age <= 10)
    color = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
  else if (age <= 15)
    color = ImVec4(0.95f, 0.8f, 0.2f, 1.0f);
  else
    color = ImVec4(0.95f, 0.2f, 0.2f, 1.0f);

  char buf[64];
  if (age < 60) {
    std::snprintf(buf, sizeof(buf), "%ld seconds ago", age);
  } else if (age < 3600) {
    std::snprintf(buf, sizeof(buf), "%ld minutes ago", age / 60);
  } else {
    std::snprintf(buf, sizeof(buf), "%ld hours ago", age / 3600);
  }

  ImGui::TextColored(color, "%s", buf);
  ImGui::Dummy(ImVec2(0.0f, 8.0f));

  auto const ssh = entry.name.find("_ssh") != std::string::npos;
  auto const port = ssh ? entry.port : 22;
  auto const name = mdns::engine::util::stripMdnsServicePostfix(entry.name);

  ImGuiStyle const& style = ImGui::GetStyle();
  ImGui::PushStyleVar(
    ImGuiStyleVar_FramePadding,
    ImVec2(style.FramePadding.x * 0.6f, style.FramePadding.y));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

  mdns::engine::ui::pushThemedButtonStyles(ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
  {
    const char* label = "  Open in browser";
    ImVec2 textSize2 = ImGui::CalcTextSize(label);
    float iconSize = ImGui::GetTextLineHeight();
    float pad = ImGui::GetStyle().FramePadding.x;
    ImVec2 btnSize(iconSize + pad + textSize2.x + pad * 2,
                   iconSize + ImGui::GetStyle().FramePadding.y * 4);

    if (ImGui::Button(label, btnSize)) {
      std::string url =
        entry.name.find("https") != std::string::npos ? "https" : "http";

      url += "://";
      url += name;

      if (entry.port != mdns::proto::port) {
        url += ":" + std::to_string(entry.port);
      }

      mdns::engine::util::openInBrowser(url);
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    ImVec2 iconPos =
      ImVec2(min.x + pad + 3.0f, min.y + (btnSize.y - iconSize) * 0.5f);
    draw->AddImage((ImTextureID)(intptr_t)browser_texture,
                   iconPos,
                   ImVec2(iconPos.x + iconSize, iconPos.y + iconSize));
  }

  ImGui::SameLine();
  {
    std::string labelStr = "  SSH";
    ImVec2 textSize2 = ImGui::CalcTextSize(labelStr.c_str());
    float iconSize = ImGui::GetTextLineHeight();
    float pad = ImGui::GetStyle().FramePadding.x;
    ImVec2 btnSize(iconSize + pad + textSize2.x + pad * 2,
                   iconSize + ImGui::GetStyle().FramePadding.y * 4);

    if (ImGui::Button(labelStr.c_str(), btnSize)) {
      mdns::engine::util::openShellAndSSH(name, "root", port);
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    ImVec2 iconPos =
      ImVec2(min.x + pad + 3.0f, min.y + (btnSize.y - iconSize) * 0.5f);
    draw->AddImage((ImTextureID)(intptr_t)terminal_texture,
                   iconPos,
                   ImVec2(iconPos.x + iconSize, iconPos.y + iconSize));
  }

  ImGui::SameLine();

  mdns::engine::ui::popThemedButtonStyles();
  mdns::engine::ui::pushThemedButtonStyles(ImVec4(0.55f, 0.57f, 0.60f, 1.0f));

  {
    const char* label = "Metadata";
    ImVec2 textSize2 = ImGui::CalcTextSize(label);
    float iconSize = ImGui::GetTextLineHeight();
    float pad = ImGui::GetStyle().FramePadding.x;
    ImVec2 btnSize(iconSize + pad + textSize2.x + pad * 2,
                   iconSize + ImGui::GetStyle().FramePadding.y * 4);

    if (ImGui::Button(label, btnSize)) {
      onOpenDissectorMeta(entry);
    }
    //
    // ImDrawList* draw = ImGui::GetWindowDrawList();
    // ImVec2 min = ImGui::GetItemRectMin();
    // ImVec2 max = ImGui::GetItemRectMax();
    //
    // ImVec2 iconPos = ImVec2(
    //     min.x + pad,
    //     min.y + (btnSize.y - iconSize) * 0.5f
    // );
    // draw->AddImage((ImTextureID)(intptr_t)info_texture, iconPos,
    // ImVec2(iconPos.x + iconSize, iconPos.y + iconSize));
  }

  mdns::engine::ui::popThemedButtonStyles();

  ImGui::PopStyleVar(3);
  ImGui::Unindent(21);
  ImGui::EndChild();
  ImGui::PopID();
}