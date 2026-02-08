#include <cmath>
#include <style/Button.h>

void
mdns::engine::ui::pushThemedButtonStyles(ImVec4 base)
{
  ImGuiStyle const& style = ImGui::GetStyle();

  ImVec4 btn = ImVec4(base.x, base.y, base.z, 0.85f);
  ImVec4 btnHover =
    ImVec4(base.x + 0.05f, base.y + 0.05f, base.z + 0.05f, 0.95f);
  ImVec4 btnActive =
    ImVec4(base.x - 0.05f, base.y - 0.05f, base.z - 0.05f, 1.0f);
  ImVec4 border = ImVec4(1, 1, 1, 0.10f);

  ImGui::PushStyleVar(
    ImGuiStyleVar_FramePadding,
    ImVec2(style.FramePadding.x * 0.6f, style.FramePadding.y));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

  ImGui::PushStyleColor(ImGuiCol_Button, btn);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHover);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, btnActive);
  ImGui::PushStyleColor(ImGuiCol_Border, border);
}

void
mdns::engine::ui::popThemedButtonStyles()
{
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(4);
}

void
mdns::engine::ui::renderPlayTriange(float h, ImVec2 const& center)
{
  ImDrawList* draw = ImGui::GetWindowDrawList();

  float r = h * 0.22f;

  draw->AddTriangleFilled(ImVec2(center.x - r * 0.6f, center.y - r),
                          ImVec2(center.x - r * 0.6f, center.y + r),
                          ImVec2(center.x + r, center.y),
                          IM_COL32(255, 255, 255, 255));
}

void
mdns::engine::ui::renderLoadingSpinner(float h, ImVec2 const& center)
{
  ImDrawList* draw = ImGui::GetWindowDrawList();

  float radius = h * 0.22f;
  float thickness = radius * 0.35f;

  float time = ImGui::GetTime();
  float a_min = time * 6.0f;
  float a_max = a_min + 3.1415f * 1.5f;

  draw->PathClear();

  for (int i = 0; i <= 24; i++) {
    float a = a_min + (i / 24.0f) * (a_max - a_min);
    draw->PathLineTo(
      ImVec2(center.x + cosf(a) * radius, center.y + sinf(a) * radius));
  }

  draw->PathStroke(IM_COL32(255, 255, 255, 255), false, thickness);
}