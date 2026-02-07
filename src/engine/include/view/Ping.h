#include <Types.h>
#include <Ping46.h>
#include <vector>
#include <functional>

namespace mdns::engine::ui
{
    void renderPingTool(PingTool::PingStats const& stats, std::string const& stdout, std::function<void()> onStop);
}