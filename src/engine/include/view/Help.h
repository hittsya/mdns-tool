#ifndef HELP_H
#define HELP_H

#include <Types.h>
#include <optional>

namespace mdns::engine::ui {
void
renderChangelogWindow(bool* show);
void
renderHelpWindow(bool* show, const char* title);
}

#endif // HELP_H