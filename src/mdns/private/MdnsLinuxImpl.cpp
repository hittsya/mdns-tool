#include "MdnsImpl.hpp"
#include <Logger.h>
#include "../include/Proto.h"
#include <type_traits>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>

namespace {

constexpr unsigned char localhost[]        = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
constexpr unsigned char localhost_mapped[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};

bool isLoopback(struct sockaddr_in *sockaddr) {
    return (ntohl(sockaddr->sin_addr.s_addr) & 0xFF000000) == 0x7F000000;
}

template <typename T>
std::string inet2str(char* buffer, std::size_t capacity, T const* addr, std::size_t addrlen) {
    static_assert(
        std::is_same_v<T, sockaddr_in> || std::is_same_v<T, sockaddr_in6>,
        "inet2str only supports sockaddr_in and sockaddr_in6"
    );

    char host[NI_MAXHOST]    = {};
    char service[NI_MAXSERV] = {};

    auto const ret = getnameinfo(
        reinterpret_cast<sockaddr const*>(addr),
        static_cast<socklen_t>(addrlen),
        host,
        NI_MAXHOST,
        service,
        NI_MAXSERV,
        NI_NUMERICSERV | NI_NUMERICHOST
    );

    std::size_t len = 0;

    if (ret == 0) {
        in_port_t port = 0;

        if constexpr (std::is_same_v<T, sockaddr_in>) {
            port = addr->sin_port;
        } else {
            port = addr->sin6_port;
        }

        if (port != 0) {
            if constexpr (std::is_same_v<T, sockaddr_in6>) {
                len = std::snprintf(buffer, capacity, "[%s]:%s", host, service);
            } else {
                len = std::snprintf(buffer, capacity, "%s:%s", host, service);
            }
        } else {
            len = std::snprintf(buffer, capacity, "%s", host);
        }
    }

    if (len >= capacity) {
        len = capacity - 1;
    }

    return {buffer, len};
}

std::string getErrnoString() {
    auto *err = strerror(errno);
    return err ? err : "";
}

int initalizeIpv4Socket(ifaddrs *ifa, sockaddr_in *sockaddr, int port) {
    sockaddr->sin_port = htons(port);

    auto sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        logger::mdns()->error("Error creating socket: " + getErrnoString());
        return -1;
    }

    unsigned char ttl       = 1;
    unsigned char loopback  = 1;
    unsigned int  reuseaddr = 1;

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
        logger::mdns()->error("Error setting SO_REUSEADDR socket option: " + getErrnoString());
        ::close(sock);
        return -1;
    }

#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuseaddr, sizeof(reuseaddr)) < 0) {
        logger::mdns()->error("Error setting SO_REUSEPORT socket option: " + getErrnoString());
        ::close(sock);
        return -1;
    }
#endif

    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        logger::mdns()->error("Error setting IP_MULTICAST_TTL socket option: " + getErrnoString());
        ::close(sock);
        return -1;
    }

    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback)) < 0) {
        logger::mdns()->error("Error setting IP_MULTICAST_LOOP socket option: " + getErrnoString());
        ::close(sock);
        return -1;
    }

    sockaddr_in bind_addr = {};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(port);
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sock, reinterpret_cast<::sockaddr *>(&bind_addr), sizeof(bind_addr)) < 0) {
        logger::mdns()->error("Error binding socket: " + getErrnoString());
        ::close(sock);
        return -1;
    }
    
    ip_mreq req = {};
    req.imr_multiaddr.s_addr = htonl((((uint32_t)224U) << 24U) | ((uint32_t)251U));
    req.imr_interface = sockaddr->sin_addr;
    
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req)) < 0) {
        logger::mdns()->error("Error setting IP_ADD_MEMBERSHIP socket options: " + getErrnoString());
        ::close(sock);
        return -1;
    }
    
    auto const flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    sockaddr->sin_port = htons(port);
    return sock;
}

int initalizeIpv6Socket(ifaddrs *ifa, sockaddr_in6 *sockaddr, int port) {
    sockaddr->sin6_port = htons(port);
    
    auto sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        logger::mdns()->error("Error creating IPv6 socket: " + getErrnoString());
        return -1;
    }
    
    unsigned int ttl       = 1;
    unsigned int loopback  = 1;
    unsigned int reuseaddr = 1;
    
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
        logger::mdns()->error("Error setting SO_REUSEADDR socket option: " + getErrnoString());
        ::close(sock);
        return -1;
    }
    
#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuseaddr, sizeof(reuseaddr)) < 0) {
        logger::mdns()->error("Error setting SO_REUSEPORT socket option: " + getErrnoString());
        ::close(sock);
        return -1;
    }
#endif
    
    int ipv6only = 1;
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6only, sizeof(ipv6only)) < 0) {
        logger::mdns()->error("Error setting IPV6_V6ONLY socket option: " + getErrnoString());
        ::close(sock);
        return -1;
    }
    
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl)) < 0) {
        logger::mdns()->error("Error setting IPV6_MULTICAST_HOPS socket option: " + getErrnoString());
        ::close(sock);
        return -1;
    }
    
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loopback, sizeof(loopback)) < 0) {
        logger::mdns()->error("Error setting IPV6_MULTICAST_LOOP socket option: " + getErrnoString());
        ::close(sock);
        return -1;
    }
    
    ipv6_mreq req = {};
    req.ipv6mr_multiaddr.s6_addr[0]  = 0xff;
    req.ipv6mr_multiaddr.s6_addr[1]  = 0x02;
    req.ipv6mr_multiaddr.s6_addr[15] = 0xfb;
    
    req.ipv6mr_interface = if_nametoindex(ifa->ifa_name);
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &req, sizeof(req)) < 0) {
        logger::mdns()->error("Error setting IPV6_JOIN_GROUP socket option: " + getErrnoString());
        ::close(sock);
        return -1;
    }
    
    if (bind(sock, reinterpret_cast<::sockaddr*>(sockaddr), sizeof(sockaddr_in6)) < 0) {
        logger::mdns()->error("Error binding IPv6 socket: " + getErrnoString());
        ::close(sock);
        return -1;
    }
    
    auto const flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    return sock;
}

}

std::vector<mdns::MdnsHelper::sock_fd_t>
mdns::MdnsHelper::BackendImpl::open_client_sockets_foreach_iface(std::size_t max, int port) {
    std::vector<sock_fd_t> result;
    result.reserve(max);

    ifaddrs *ifaddr { nullptr };
    char conv[128];

    if (getifaddrs(&ifaddr) < 0) {
        logger::mdns()->error("Error getting if list");
        return result;
    }

    for (ifaddrs *curr_if = ifaddr; curr_if; curr_if = curr_if->ifa_next) {
        if (!curr_if->ifa_addr) {
            continue;
        }

        if (result.size() >= max) {
            logger::mdns()->warn("Reached max sockets count. Unable to create socket");
            break;
        }

        if (curr_if->ifa_addr->sa_family == AF_INET) {
            auto *sockaddr = reinterpret_cast<sockaddr_in *>(curr_if->ifa_addr);

            if (isLoopback(sockaddr)) {
                continue;
            }

            auto const sock = initalizeIpv4Socket(curr_if, sockaddr, port);
            if (sock < 0) {
                logger::mdns()->warn("Skipping IPv4 socket: " + inet2str(conv, sizeof(conv), sockaddr, sizeof(sockaddr_in)));
                continue;
            }

            logger::mdns()->trace("Init IPv4 socket: " + inet2str(conv, sizeof(conv), sockaddr, sizeof(sockaddr_in)));
            result.push_back(sock);
        }

        if (curr_if->ifa_addr->sa_family == AF_INET6) {
            auto *sockaddr = reinterpret_cast<sockaddr_in6 *>(curr_if->ifa_addr);

            if (IN6_IS_ADDR_LOOPBACK(&sockaddr->sin6_addr) || IN6_IS_ADDR_V4MAPPED(&sockaddr->sin6_addr)) {
                continue;
            }

            auto const sock = initalizeIpv6Socket(curr_if, sockaddr, port);
            if (sock < 0) {
                logger::mdns()->warn("Skipping IPv6 socket: " + inet2str(conv, sizeof(conv), sockaddr, sizeof(sockaddr_in)));
                continue;
            }

            logger::mdns()->trace("Init IPv6 socket: " + inet2str(conv, sizeof(conv), sockaddr, sizeof(sockaddr_in6)));
            result.push_back(sock);
        }
    }

    freeifaddrs(ifaddr);
    return result;
}

int
mdns::MdnsHelper::BackendImpl::send_multicast(sock_fd_t sock, void const* buffer, std::size_t size) {
    sockaddr_storage addr_storage;
    sockaddr_in      addr;
    sockaddr_in6     addr6;
    sockaddr        *saddr = reinterpret_cast<struct sockaddr *>(&addr_storage);

    socklen_t saddrlen = sizeof(struct sockaddr_storage);
    if (getsockname(sock, saddr, &saddrlen)) {
        logger::mdns()->error("getsockname() failed: " + getErrnoString());
        return -1;
    }

    if (saddr->sa_family == AF_INET6) {
        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;

#ifdef __APPLE__
        addr6.sin6_len = sizeof(addr6);
#endif

        addr6.sin6_addr.s6_addr[0]  = 0xFF;
        addr6.sin6_addr.s6_addr[1]  = 0x02;
        addr6.sin6_addr.s6_addr[15] = 0xFB;
        addr6.sin6_port = htons(static_cast<unsigned short>(proto::port));

        saddr    = reinterpret_cast<sockaddr *>(&addr6);
        saddrlen = sizeof(addr6);
    }
    else {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;

#ifdef __APPLE__
        addr.sin_len = sizeof(addr);
#endif

        addr.sin_addr.s_addr = htonl((((uint32_t)224U) << 24U) | ((uint32_t)251U));
        addr.sin_port        = htons(static_cast<unsigned short>(proto::port));
        saddr    = reinterpret_cast<struct sockaddr *>(&addr);
        saddrlen = sizeof(addr);
    }

    if (sendto(sock, buffer, static_cast<int>(size), 0, saddr, saddrlen) < 0) {
        logger::mdns()->error("sendto() failed: " + getErrnoString());
        return -1;
    }

    return 0;
}

void
mdns::MdnsHelper::BackendImpl::close(sock_fd_t sock) {
    ::close(sock);
}

std::vector<mdns::proto::mdns_recv_res>
mdns::MdnsHelper::BackendImpl::receive_discovery(std::vector<sock_fd_t> const& sockets)
{
    static constexpr std::size_t RECV_BUFF_SIZE = 2048;

    std::vector<proto::mdns_recv_res> result;
    result.reserve(sockets.size());

    timeval timeout{};
    timeout.tv_sec  = 0;
    timeout.tv_usec = 100000;

    fd_set readfs;
    FD_ZERO(&readfs);

    int nfds = 0;
    for (auto socket : sockets) {
        FD_SET(socket, &readfs);
        nfds = std::max(nfds, socket + 1);
    }

    int res = select(nfds, &readfs, nullptr, nullptr, &timeout);
    if (res < 0) {
        logger::mdns()->error("select() failed: " + std::string(strerror(errno)));
        return result;
    }
    else if(res == 0) {
        // logger::mdns()->warn("select() timeout: no responses received");
        return result;
    }

    char ipStr[INET6_ADDRSTRLEN] = {};
    uint16_t port = 0;

    for (auto socket : sockets) {
        if (!FD_ISSET(socket, &readfs)) {
            continue;
        }

        while (true) {
            char buffer[RECV_BUFF_SIZE];

            sockaddr_storage addr{};
            socklen_t addrlen = sizeof(addr);

            auto ret = recvfrom(socket, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&addr), &addrlen);
            if (ret > 0) {                
                if (addr.ss_family == AF_INET) {
                    auto* a = reinterpret_cast<sockaddr_in*>(&addr);
                    inet_ntop(AF_INET, &a->sin_addr, ipStr, sizeof(ipStr));
                    port = ntohs(a->sin_port);
                }
                else if (addr.ss_family == AF_INET6) {
                    auto* a = reinterpret_cast<sockaddr_in6*>(&addr);
                    inet_ntop(AF_INET6, &a->sin6_addr, ipStr, sizeof(ipStr));
                    port = ntohs(a->sin6_port);
                }

                proto::mdns_recv_res recv_res;
                recv_res.ip_addr_str  = ipStr;
                recv_res.port         = port;
                recv_res.blob         = std::vector<char>(buffer, buffer + ret);
                result.push_back(recv_res);

                continue;
            }

            if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }

            if (ret == 0) {
                logger::mdns()->warn("Zero-length UDP datagram ignored");
                break;
            }

            logger::mdns()->error("recvfrom() failed: " + getErrnoString());
            break;
        }
    }

    return result;
}
