#include <Types.h>
#include <optional>

namespace mdns::engine::ui
{
    void renderDissectorWindow(std::optional<ScanCardEntry> const& dissector_meta_entry, bool* show);
}