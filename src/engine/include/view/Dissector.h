#ifndef DISSECTOR_H
#define DISSECTOR_H

#include <Types.h>
#include <optional>

namespace mdns::engine::ui {
void
renderDissectorWindow(std::optional<ScanCardEntry> const& dissector_meta_entry,
                      bool* show);
}

#endif //DISSECTOR_H