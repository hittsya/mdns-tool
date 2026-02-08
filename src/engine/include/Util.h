#ifndef UTIL_H
#define UTIL_H

#include <Logger.h>
#include <string>

#ifdef WIN32
#include "windows.h"
#endif

namespace mdns::engine::util {
void
openInBrowser(const std::string& url);
void
openShellAndSSH(const std::string& host, const std::string& user, int port);
std::string
stripMdnsServicePostfix(const std::string& name);
}

#endif // UTIL_H