#ifdef WIN32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <MdnsHelper.h>
#include <MdnsImpl.hpp>
#include <Logger.h>
#include <Proto.h>

namespace {

    std::string winError()
    {
        return std::to_string(WSAGetLastError());
    }

    void setNonBlocking(SOCKET s)
    {
        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);
    }

    SOCKET initIpv4Socket(sockaddr_in* ifaceAddr, int port)
    {
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            logger::mdns()->error("IPv4 socket() failed: " + winError());
            return INVALID_SOCKET;
        }

        BOOL reuse = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

        sockaddr_in bindAddr{};
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_port   = htons(port);
        bindAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR) {
            logger::mdns()->error("IPv4 bind() failed: " + winError());
            closesocket(sock);
            return INVALID_SOCKET;
        }

        ip_mreq req{};
        inet_pton(AF_INET, "224.0.0.251", &req.imr_multiaddr);
        req.imr_interface = ifaceAddr->sin_addr;

        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
            (char*)&req, sizeof(req)) == SOCKET_ERROR) {
            logger::mdns()->error("IPv4 IP_ADD_MEMBERSHIP failed: " + winError());
            closesocket(sock);
            return INVALID_SOCKET;
        }

        setNonBlocking(sock);
        return sock;
    }

    SOCKET initIpv6Socket(sockaddr_in6* ifaceAddr, ULONG ifIndex, int port)
    {
        SOCKET sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            logger::mdns()->error("IPv6 socket() failed: " + winError());
            return INVALID_SOCKET;
        }

        BOOL reuse = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

        int v6only = 1;
        setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&v6only, sizeof(v6only));

        sockaddr_in6 bindAddr{};
        bindAddr.sin6_family = AF_INET6;
        bindAddr.sin6_port = htons(port);
        bindAddr.sin6_addr = in6addr_any;

        if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR) {
            logger::mdns()->error("IPv6 bind() failed: " + winError());
            closesocket(sock);
            return INVALID_SOCKET;
        }

        ipv6_mreq req{};
        inet_pton(AF_INET6, "ff02::fb", &req.ipv6mr_multiaddr);
        req.ipv6mr_interface = ifIndex;

        if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
            (char*)&req, sizeof(req)) == SOCKET_ERROR) {
            logger::mdns()->error("IPv6 IPV6_JOIN_GROUP failed: " + winError());
            closesocket(sock);
            return INVALID_SOCKET;
        }

        setNonBlocking(sock);
        return sock;
    }

}

mdns::MdnsHelper::BackendImpl::BackendImpl()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

mdns::MdnsHelper::BackendImpl::~BackendImpl()
{
    WSACleanup();
}

std::vector<mdns::MdnsHelper::sock_fd_t>
mdns::MdnsHelper::BackendImpl::open_client_sockets_foreach_iface(std::size_t max, int port)
{
    std::vector<sock_fd_t> result;
    result.reserve(max);

    ULONG size = 0;
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, nullptr, &size);

    std::vector<char> buffer(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapters, &size) != NO_ERROR) {
        logger::mdns()->error("GetAdaptersAddresses failed");
        return result;
    }

    for (auto* a = adapters; a && result.size() < max; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) {
            continue;
        }

        for (auto* ua = a->FirstUnicastAddress; ua && result.size() < max; ua = ua->Next) {
            auto* sa = ua->Address.lpSockaddr;

            if (sa->sa_family == AF_INET) {
                auto* addr = (sockaddr_in*)sa;
                auto sock  = initIpv4Socket(addr, port);
                
                if (sock != INVALID_SOCKET) {
                    result.push_back(sock);
                }
            }
            else if (sa->sa_family == AF_INET6) {
                auto* addr = (sockaddr_in6*)sa;
                auto sock  = initIpv6Socket(addr, a->Ipv6IfIndex, port);

                if (sock != INVALID_SOCKET) {
                    result.push_back(sock);
                }
            }
        }
    }

    return result;
}

int
mdns::MdnsHelper::BackendImpl::send_multicast(sock_fd_t sock, void const* buffer, std::size_t size)
{
    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);
    getsockname(sock, (sockaddr*)&addr, &len);

    if (addr.ss_family == AF_INET6) {
        sockaddr_in6 dst{};
        dst.sin6_family = AF_INET6;
        inet_pton(AF_INET6, "ff02::fb", &dst.sin6_addr);
        dst.sin6_port = htons(proto::port);

        if (sendto(sock, (char*)buffer, (int)size, 0, (sockaddr*)&dst, sizeof(dst)) == SOCKET_ERROR) {
            return -1;
        }
    }
    else {
        sockaddr_in dst{};
        dst.sin_family = AF_INET;
        inet_pton(AF_INET, "224.0.0.251", &dst.sin_addr);
        dst.sin_port = htons(proto::port);

        if (sendto(sock, (char*)buffer, (int)size, 0,
            (sockaddr*)&dst, sizeof(dst)) == SOCKET_ERROR)
            return -1;
    }

    return 0;
}

std::vector<mdns::proto::mdns_recv_res>
mdns::MdnsHelper::BackendImpl::receive_discovery(std::vector<sock_fd_t> const& sockets)
{
    std::vector<proto::mdns_recv_res> result;

    timeval timeout{};
    timeout.tv_sec  = 0;
    timeout.tv_usec = 100000;

    fd_set readfs;
    FD_ZERO(&readfs);

    for (auto s : sockets) {
        FD_SET(s, &readfs);
    }

    int res = select(0, &readfs, nullptr, nullptr, &timeout);
    if (res <= 0) {
        return result;
    }

    char ip[INET6_ADDRSTRLEN];
    for (auto s : sockets) {
        if (!FD_ISSET(s, &readfs)) {
            continue;
        }

        while (true) {
            char buf[2048];
            sockaddr_storage addr{};
            socklen_t len = sizeof(addr);

            int ret = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&addr, &len);
            if (ret <= 0) {
                break;
            }

            uint16_t port = 0;
            if (addr.ss_family == AF_INET) {
                auto* a = (sockaddr_in*)&addr;
                inet_ntop(AF_INET, &a->sin_addr, ip, sizeof(ip));
                port = ntohs(a->sin_port);
            }
            else {
                auto* a = (sockaddr_in6*)&addr;
                inet_ntop(AF_INET6, &a->sin6_addr, ip, sizeof(ip));
                port = ntohs(a->sin6_port);
            }

            proto::mdns_recv_res r;
            r.ip_addr_str = ip;
            r.port        = port;
            r.blob.assign(buf, buf + ret);
            result.push_back(std::move(r));
        }
    }

    return result;
}

void
mdns::MdnsHelper::BackendImpl::close(sock_fd_t sock)
{
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
}

#endif // WIN32
