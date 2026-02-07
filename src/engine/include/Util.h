#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <Logger.h>

#ifdef WIN32
    #include "windows.h"
#endif

namespace mdns::engine::util {
    void openInBrowser(const std::string &url);
    void openShellAndSSH(const std::string& host, const std::string& user, int port);
}

#endif //UTIL_H