#include <Types.h>

#include <vector>
#include <functional>

namespace mdns::engine::ui
{
    void renderServiceLayout(std::vector<ScanCardEntry> const&         discovered_services,
                             std::function<void(std::string const&)>   onOpenPingTool,
                             std::function<void(ScanCardEntry entry)>  onOpenDissectorMeta,
                             unsigned int browser_texture,
                             unsigned int info_texture,
                             unsigned int terminal_texture);

    void renderServiceCard(int                  index, 
                           ScanCardEntry const& entry,
                           float                cardWidth,
                           std::function<void(std::string const&)>     onOpenPingTool,
                           std::function<void(ScanCardEntry entry)>    onOpenDissectorMeta,
                           unsigned int browser_texture,
                           unsigned int info_texture,
                           unsigned int terminal_texture);
}