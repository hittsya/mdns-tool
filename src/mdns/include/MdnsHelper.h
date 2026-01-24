#ifndef MDNSHELPER_H
#define MDNSHELPER_H

#include <thread>
#include <memory>
#include <vector>
#include <atomic>
#include <optional>
#include <Proto.h>

namespace mdns
{

class MdnsHelper
{
public:
    using sock_fd_t = int;

    MdnsHelper();
    ~MdnsHelper();

    void startBrowse();
    void stopBrowse();
    bool browsing() { return browsing_.load(std::memory_order_relaxed); }
private:
    void runDiscovery(std::stop_token stop_token, std::vector<sock_fd_t>&& sockets);
    std::optional<proto::mdns_response> parseDiscoveryResponse(proto::mdns_recv_res const& message);
    const std::uint8_t* parseName(const std::uint8_t*& ptr, const  std::uint8_t *start, const std::uint8_t *end, std::string& out);
    proto::mdns_rr parseRR(const std::uint8_t*& ptr, const std::uint8_t *start, const std::uint8_t *end);
    std::uint16_t readU16(const std::uint8_t*& ptr);
    std::uint32_t readU32(const std::uint8_t*& ptr);
    bool serializeRR(proto::mdns_response& res, proto::mdns_rr& rr);
private:
    struct BackendImpl;
    std::unique_ptr<BackendImpl> impl_;

    std::jthread      browsing_thread_;
    std::atomic<bool> browsing_ {false};
};

}

#endif // MDNSHELPER_H
