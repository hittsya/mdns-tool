#ifndef PING_H
#define PING_H

#include <Ping46.h>
#include <Types.h>

#ifdef _WIN32
    #ifdef stdout
    #pragma push_macro("stdout")
    #undef stdout
    #define RESTORE_STDOUT_MACRO
    #endif
#endif

#include <vector>
#include <functional>

namespace mdns::engine::ui {
void
renderPingTool(PingTool::PingStats const& stats,
               std::string const& stdout,
               std::function<void()> onStop);
}

#ifdef RESTORE_STDOUT_MACRO
    #pragma pop_macro("stdout")
    #undef RESTORE_STDOUT_MACRO
#endif

#endif //PING_H