#ifndef MDNSHELPER_H
#define MDNSHELPER_H

#include <thread>
#include <memory>
#include <vector>
#include <atomic>
#include <optional>
#include <functional>
#include <Proto.h>

namespace mdns
{

class MdnsHelper
{
public:
    using sock_fd_t            = int;
    using service_dicovered_cb = std::function<void(std::vector<proto::mdns_response>&&)>;
    using browse_en_cb         = std::function<void(bool)>;

    MdnsHelper();
    ~MdnsHelper();
    void startBrowse();
    void stopBrowse ();
    void connectOnServiceDiscovered(service_dicovered_cb cb);
    void connectOnBrowsingStateChanged(browse_en_cb cb);
private:
    void runDiscovery(std::stop_token stop_token, std::vector<sock_fd_t>&& sockets);
    std::optional<proto::mdns_response> parseDiscoveryResponse(proto::mdns_recv_res const& message);
    const std::uint8_t* parseName(const std::uint8_t*& ptr, const  std::uint8_t *start, const std::uint8_t *end, std::string& out);
    proto::mdns_rr parseRR(const std::uint8_t*& ptr, const std::uint8_t *start, const std::uint8_t *end);
    std::uint16_t readU16(const std::uint8_t*& ptr);
    std::uint32_t readU32(const std::uint8_t*& ptr);
private:
    struct BackendImpl;
    std::unique_ptr<BackendImpl> impl_;

    service_dicovered_cb on_service_discovered_     {[](std::vector<proto::mdns_response>&&){}};
    browse_en_cb         on_browsing_state_changed_ {[](bool){}};

    std::jthread         browsing_thread_;
    std::atomic<bool>    browsing_ {false};
};

}

#endif // MDNSHELPER_H
