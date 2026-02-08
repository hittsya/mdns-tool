#ifndef SERVICES_H
#define SERVICES_H

#include <Types.h>

#include <functional>
#include <vector>

namespace mdns::engine::ui {
void
renderServiceLayout(
  std::vector<ScanCardEntry> const& discovered_services,
  std::function<void(std::string const&)> onOpenPingTool,
  std::function<void()> onQuestionWindowOpen,
  std::function<void(ScanCardEntry entry)> onOpenDissectorMeta,
  unsigned int browser_texture,
  unsigned int info_texture,
  unsigned int terminal_texture,
  std::vector<std::string> const& questions);

void
renderServiceCard(int index,
                  ScanCardEntry const& entry,
                  float cardWidth,
                  std::function<void(std::string const&)> onOpenPingTool,
                  std::function<void(ScanCardEntry entry)> onOpenDissectorMeta,
                  unsigned int browser_texture,
                  unsigned int info_texture,
                  unsigned int terminal_texture);
}

#endif // SERVICES_H