#include <imgui.h>

namespace mdns::engine::ui {
void
pushThemedButtonStyles(ImVec4 base);
void
popThemedButtonStyles();
void
renderPlayTriange(float h, ImVec2 const& center);
void
renderLoadingSpinner(float h, ImVec2 const& center);
}