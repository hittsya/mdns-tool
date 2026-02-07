#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <Logger.h>

#ifdef WIN32
    #include "windows.h"
#endif

namespace mdns::engine::util {
    void openInBrowser(const std::string &url);
}

#endif //UTIL_H