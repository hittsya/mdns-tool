#include <Ping46.h>
#include <Types.h>
#include <functional>
#include <vector>

namespace mdns::engine::ui {
void
renderPingTool(PingTool::PingStats const& stats,
               std::string const& stdout,
               std::function<void()> onStop);
}