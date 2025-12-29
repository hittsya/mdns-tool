#ifndef MDNSLINUXIMPL_HPP
#define MDNSLINUXIMPL_HPP

#include "MdnsHelper.h"
#include <vector>

struct mdns::MdnsHelper::BackendImpl
{
    std::vector<sock_fd_t> open_client_sockets_foreach_iface(std::size_t max, int port);
    int send_multicast(sock_fd_t sock, void const* buffer, std::size_t size);
    std::vector<proto::mdns_recv_res> receive_discovery(std::vector<sock_fd_t> const& sockets);
    void close(sock_fd_t sock);
};

#endif // MDNSLINUXIMPL_HPP
