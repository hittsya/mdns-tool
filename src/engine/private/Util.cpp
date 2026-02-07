#include "Util.h"
#include "Logger.h"

void mdns::engine::util::openInBrowser(const std::string &url)
{
    logger::core()->info("Opening link: " + url);

#ifdef _WIN32
    HINSTANCE res = ShellExecuteA(
        nullptr,
        nullptr,
        url.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL);

    if ((INT_PTR)res <= 32) {
        logger::core()->error("Failed opening link: " + url);
    }
#else
    std::string cmd = "xdg-open \"" + url + "\"";
    if (auto result = system(cmd.c_str())) {
        logger::core()->error(fmt::format("Command {} failed with rc {}", cmd, result));
    }
#endif
}