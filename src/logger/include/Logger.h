#pragma once

#include <memory>
#include <spdlog/logger.h>

namespace logger {
    void init();
    void shutdown();

    std::shared_ptr<spdlog::logger> core();
    std::shared_ptr<spdlog::logger> net();
    std::shared_ptr<spdlog::logger> mdns();
    std::shared_ptr<spdlog::logger> ui();

}
