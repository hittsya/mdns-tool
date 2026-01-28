#include "MdnsHelper.h"
#include "../include/Proto.h"
#include "Logger.h"
#include "MdnsImpl.hpp"

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
#endif

mdns::MdnsHelper::MdnsHelper()
    : impl_(std::make_unique<BackendImpl>())
{
}

mdns::MdnsHelper::~MdnsHelper() = default;

void mdns::MdnsHelper::startBrowse() {
    if (browsing_.exchange(true)) {
        logger::mdns()->warn("Discovery already running");
        return;
    }
    
    auto connections = impl_->open_client_sockets_foreach_iface(32, 5353);
    if (connections.empty()) {
        logger::mdns()->error("No sockets opened");
        browsing_.store(false, std::memory_order_relaxed);
        on_browsing_state_changed_(false);
        return;
    }
    logger::mdns()->info("Opened " + std::to_string(connections.size()) + " sockets");
    
    browsing_thread_ = std::jthread(
        [this, connections = std::move(connections)]
        (std::stop_token stop_token) mutable -> void {
            logger::mdns()->info("Starting continuous mDNS discovery");
            on_browsing_state_changed_(true);
            runDiscovery(stop_token, std::move(connections));
    });
}

void mdns::MdnsHelper::stopBrowse() {
    if (!browsing_.exchange(false)) {
        logger::mdns()->warn("Discovery not running");
        return;
    }
    
    logger::mdns()->info("Stopping mDNS discovery");
    
    browsing_thread_.request_stop();
    if (browsing_thread_.joinable()) {
        browsing_thread_.join();
    }
    
    logger::mdns()->info("mDNS discovery stopped");
    on_browsing_state_changed_(false);
}

void
mdns::MdnsHelper::runDiscovery(std::stop_token stop_token, std::vector<sock_fd_t>&& sockets)
{
    const auto query_interval = std::chrono::milliseconds(2500);
    auto last_query_time      = std::chrono::steady_clock::now() - query_interval;

    while(!stop_token.stop_requested()) {
        auto now = std::chrono::steady_clock::now();
        
        if (now - last_query_time >= query_interval) {
            for (auto const socket: sockets) {
                logger::mdns()->info("Sending discovery query: socket FD: " + std::to_string(socket));
                impl_->send_multicast(socket, proto::mdns_multi_query, sizeof(proto::mdns_multi_query));
            }

            last_query_time = now;
        }

        std::vector<proto::mdns_response> result;

        auto const messages = impl_->receive_discovery(sockets);
        for (auto const& message: messages) {
            logger::mdns()->trace("Processing multicast (" + std::to_string(message.blob.size()) + " bytes)");

            auto const parsed = parseDiscoveryResponse(message);
            if (parsed.has_value()) {
                result.push_back(parsed.value());
            } else {
                logger::mdns()->warn("Multicast processing failed");
            }
        }

        on_service_discovered_(std::move(result));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    logger::mdns()->info("Closing sockets");
    for (auto const socket: sockets) {
        impl_->close(socket);
    }

    logger::mdns()->info("Browsing thread stopped");
}

const std::uint8_t*
mdns::MdnsHelper::parseName(const std::uint8_t*& ptr, const std::uint8_t* start, const std::uint8_t* end, std::string& out)
{
    out.clear();

    const std::uint8_t* cur = ptr;
    const std::uint8_t* next = nullptr;
    int jumpCount = 0;
    bool terminated = false;

    while (cur < end)
    {
        std::uint8_t len = *cur;

        if ((len & 0xC0) == 0xC0)
        {
            if (cur + 1 >= end) {
                logger::mdns()->error("Malformed packet (truncated compression pointer)");
                return nullptr;
            }

            std::uint16_t offset = ((len & 0x3F) << 8) | *(cur + 1);
            if (start + offset >= end) {
                logger::mdns()->error("Malformed packet (bad compression offset)");
                return nullptr;
            }

            if (!next) {
                next = cur + 2;
            }

            cur = start + offset;
            terminated = true;

            if (++jumpCount > 10) {
                logger::mdns()->error("Compression pointer loop detected");
                return nullptr;
            }

            continue;
        }

        if (len == 0) {
            cur++;
            terminated = true;
            break;
        }

        cur++;

        if (cur + len > end) {
            logger::mdns()->error("Malformed packet (label overruns buffer)");
            return nullptr;
        }

        out.append(reinterpret_cast<const char*>(cur), len);
        out.push_back('.');
        cur += len;
    }

    if (!terminated) {
        logger::mdns()->error("Malformed packet (name not terminated)");
        return nullptr;
    }
    
    if (!out.empty()) {
        out.pop_back();
    }

    ptr = next ? next : cur;
    return ptr;
}

mdns::proto::mdns_rr
mdns::MdnsHelper::parseRR(const std::uint8_t*& ptr, const std::uint8_t* start, const std::uint8_t* end, std::string& advertizedIP)
{
    proto::mdns_rr record{};

    const auto* name_end = parseName(ptr, start, end, record.name);
    if (!name_end) {
        logger::mdns()->error("Malformed RR (bad owner name)");
        return record;
    }
    ptr = name_end;

    if (ptr + 10 > end) {
        logger::mdns()->error("Malformed RR (truncated header)");
        return record;
    }

    record.port  = 0;
    record.type  = readU16(ptr);
    record.clazz = readU16(ptr);
    record.ttl   = readU32(ptr);

    std::uint16_t rdlen = readU16(ptr);
    if (ptr + rdlen > end) {
        logger::mdns()->error("Malformed RR (RDATA overrun)");
        return record;
    }

    const std::uint8_t* rdata_start = ptr;
    const std::uint8_t* rdata_end   = ptr + rdlen;

    record.rdata.assign(rdata_start, rdata_end);

    switch (record.type) {
        case mdns::proto::MDNS_RECORDTYPE_PTR:
        {
            const std::uint8_t* tmp = rdata_start;
            std::string target;

            if (parseName(tmp, start, end, target)) {
                record.rdata_serialized = target;
            }
        } break;

        case mdns::proto::MDNS_RECORDTYPE_TXT:
        {
            const std::uint8_t* tmp = rdata_start;

            while (tmp < rdata_end) {
                std::uint8_t len = *tmp++;
                if (tmp + len > rdata_end) {
                    break;
                }

                std::string txt(reinterpret_cast<const char*>(tmp), len);
				logger::mdns()->trace("TXT record entry: " + txt);
                record.rdata_serialized += txt;
                tmp += len;
            }
        } break;

        case mdns::proto::MDNS_RECORDTYPE_SRV:
        {
            if (rdlen < 6) {
                logger::mdns()->error("Malformed SRV record (too short)");
                break;
            }

            const std::uint8_t* tmp = rdata_start;
            std::uint16_t priority  = readU16(tmp);
            std::uint16_t weight    = readU16(tmp);
            std::uint16_t port      = readU16(tmp);

            std::string target;
            if (!parseName(tmp, start, end, target)) {
                logger::mdns()->error("Malformed SRV record (bad target name)");
                break;
            }

            logger::mdns()->trace(fmt::format(
                "Discovered SRV record: {} -> {}:{} (prio={}, weight={})",
                record.name, target, port, priority, weight
            ));

            record.port             = port;
            record.rdata_serialized = record.name;
        } break;

        case mdns::proto::MDNS_RECORDTYPE_A:
        {
            if (rdlen == 4) {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, rdata_start, ip, sizeof(ip));
                advertizedIP = ip;
                logger::core()->trace("Discovered A record: " + advertizedIP + " / " + record.name);
                record.rdata_serialized = record.name;
            }
        } break;

        case mdns::proto::MDNS_RECORDTYPE_AAAA:
        {
            if (rdlen == 16) {
                char ip[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, rdata_start, ip, sizeof(ip));
                advertizedIP = ip;
                logger::core()->trace("Discovered AAAA record: " + advertizedIP + " / " + record.name);
                record.rdata_serialized = record.name;
            }
        } break;

        default: 
        {
        } break;
    }

    logger::mdns()->trace(fmt::format(
        "Parsed RR: name='{}' type={} class={} ttl={} rdlen={}",
            record.name,
            record.type,
            record.clazz,
            record.ttl,
            rdlen
	));

    ptr = rdata_end;
    return record;
}

std::uint16_t
mdns::MdnsHelper::readU16(const std::uint8_t*& ptr) {
    auto const result = ntohs(*reinterpret_cast<const std::uint16_t *>(ptr));
    ptr += sizeof(std::uint16_t);
    return result;
}

std::uint32_t
mdns::MdnsHelper::readU32(const std::uint8_t*& ptr) {
    auto const result = ntohl(*reinterpret_cast<const std::uint32_t *>(ptr));
    ptr += sizeof(std::uint32_t);
    return result;
}

std::optional<mdns::proto::mdns_response>
mdns::MdnsHelper::parseDiscoveryResponse(proto::mdns_recv_res const& message) {
    auto const& buffer = message.blob;

    if (buffer.size() < sizeof(std::uint16_t)*6) {
        logger::mdns()->warn("mDNS packet too small: " + std::to_string(buffer.size()) + " bytes");
        return std::nullopt;
    }

    const auto* packet_start = reinterpret_cast<const uint8_t*>(buffer.data());
    const auto* packet_end   = packet_start + buffer.size();
    const auto* data         = packet_start;

    proto::mdns_response response{};
    response.packet.assign(packet_start, packet_end);

    if (data + 12 > packet_end) {
        logger::mdns()->error("Malformed packet (truncated header)");
        return std::nullopt;
    }

    response.query_id    = readU16(data);
    response.flags       = readU16(data);
    response.questions   = readU16(data);

    std::uint16_t const answer_rrs     = readU16(data);
    std::uint16_t const authority_rrs  = readU16(data);
    std::uint16_t const additional_rrs = readU16(data);

    logger::mdns()->trace(fmt::format(
       "Header: id={} flags=0x{:04X} qd={} an={} ns={} ar={}",
           response.query_id,
           response.flags,
           response.questions,
           answer_rrs,
           authority_rrs,
           additional_rrs
    ));

    for (std::uint16_t i = 0; i < response.questions; ++i) {
        proto::mdns_question q{};

        const auto* name_end = parseName(data, packet_start, packet_end, q.name);
        if (!name_end || name_end + 4 > packet_end) {
            logger::mdns()->error("Malformed packet (bad question name)");
            return std::nullopt;
        }

        data    = name_end;
        q.type  = readU16(data);
        q.clazz = readU16(data);

        logger::mdns()->info("Pushed question query: " + q.name);
        response.questions_list.push_back(std::move(q));
    }

    auto is_noisy_mdns_ptr = [&](const proto::mdns_rr& rr) -> bool {
        if (rr.type != proto::MDNS_RECORDTYPE_PTR) {
            return false;
        }

        if (rr.name == "_services._dns-sd._udp.local") {
            return true;
        }

        if (rr.name == "_http._tcp.local") {
            return true;
        }

        if (rr.name.size() > 6 && rr.name[0] == '_' && rr.name.find("._udp.local") != std::string::npos) {
            return true;
        }

        return false;
    };

    auto parse_rr_block = [&](std::vector<proto::mdns_rr>& out, std::uint16_t count, std::string &advertizedIP) -> void {
        for (std::uint16_t i = 0; i < count; ++i) {
            const auto* before = data;

            auto rr = parseRR(data, packet_start, packet_end, advertizedIP);
            if (is_noisy_mdns_ptr(rr)) {
                // TODO: Cache this RR name to db to later query by it
                logger::mdns()->info("Skipping noisy PTR: " + rr.name);
                continue;
            }

            if (data <= before || data > packet_end) {
                logger::mdns()->error("Malformed packet (bad RR parse)");
                continue;
            }

            out.push_back(std::move(rr));
        }

        return;
    };

    parse_rr_block(response.answer_rrs    , answer_rrs    , response.advertized_ip_addr_str);
    parse_rr_block(response.authority_rrs , authority_rrs , response.advertized_ip_addr_str);
    parse_rr_block(response.additional_rrs, additional_rrs, response.advertized_ip_addr_str);

    response.ip_addr_str = message.ip_addr_str;
    response.port        = message.port;

    return response;
}

void 
mdns::MdnsHelper::connectOnServiceDiscovered(service_dicovered_cb cb) {
    on_service_discovered_ = std::move(cb);
}

void 
mdns::MdnsHelper::connectOnBrowsingStateChanged(browse_en_cb cb) {
    on_browsing_state_changed_ = std::move(cb);
}
