#include <Logger.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace logger {

    namespace {
        std::shared_ptr<spdlog::logger> core_logger;
        std::shared_ptr<spdlog::logger> net_logger;
        std::shared_ptr<spdlog::logger> ui_logger;
        std::shared_ptr<spdlog::logger> mdns_logger;
    }

    void init() {
        if (spdlog::get("CORE")) {
            return;
        }

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
        std::vector<spdlog::sink_ptr> sinks { console_sink };

        core_logger = std::make_shared<spdlog::logger>("CORE", sinks.begin(), sinks.end());
        net_logger  = std::make_shared<spdlog::logger>("NET",  sinks.begin(), sinks.end());
        ui_logger   = std::make_shared<spdlog::logger>("UI",   sinks.begin(), sinks.end());
        mdns_logger = std::make_shared<spdlog::logger>("MDNS", sinks.begin(), sinks.end());

        spdlog::register_logger(core_logger);
        spdlog::register_logger(net_logger);
        spdlog::register_logger(ui_logger);
        spdlog::register_logger(mdns_logger);

        spdlog::set_level(spdlog::level::trace);
    }

    void shutdown() {
        spdlog::shutdown();
    }

    std::shared_ptr<spdlog::logger> core() { return core_logger; }
    std::shared_ptr<spdlog::logger> net()  { return net_logger; }
    std::shared_ptr<spdlog::logger> ui()   { return ui_logger; }
    std::shared_ptr<spdlog::logger> mdns()   { return mdns_logger; }

}
