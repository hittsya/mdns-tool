#include "MdnsHelper.h"
#include "../include/Proto.h"
#include "Logger.h"
#include "MdnsImpl.hpp"

#if defined(_WIN32)
    #include <winsock.h>
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
        return;
    }
    logger::mdns()->info("Opened " + std::to_string(connections.size()) + " sockets");
    
    browsing_thread_ = std::jthread(
        [this, connections = std::move(connections)]
        (std::stop_token stop_token) mutable -> void {
            logger::mdns()->info("Starting continuous mDNS discovery");
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
}

void
mdns::MdnsHelper::runDiscovery(std::stop_token stop_token, std::vector<sock_fd_t>&& sockets)
{
    const auto query_interval = std::chrono::milliseconds(2500);
    auto last_query_time      = std::chrono::steady_clock::now() - query_interval;

    while(!stop_token.stop_requested()) {
        auto now = std::chrono::steady_clock::now();
        
        if (now - last_query_time >= query_interval) {
            logger::mdns()->info("Sending DNS-SD discovery");

            for (auto const socket: sockets) {
                // TODO: Remove from list?
                impl_->send_multicast(socket, proto::mdns_services_query, sizeof(proto::mdns_services_query));
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

        for (auto& entry: result) {
            for (auto& answer: entry.answer_rrs)
                serializeRR(entry, answer);

            for (auto& answer: entry.authority_rrs)
                serializeRR(entry, answer);

            for (auto& answer: entry.additional_rrs)
                serializeRR(entry, answer);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    logger::mdns()->info("Closing sockets");
    for (auto const socket: sockets) {
        impl_->close(socket);
    }

    logger::mdns()->info("Browsing thread stopped");
}

bool
mdns::MdnsHelper::serializeRR(proto::mdns_response& res, proto::mdns_rr& rr) {
    if (rr.type != proto::MDNS_RECORDTYPE_PTR) {
        return false;
    }

    if ((rr.clazz & 0x7FFF) != proto::MDNS_CLASS_IN) {
        return false;
    }

    const uint8_t* p = rr.rdata.data();
    std::string target;

    if (!parseName(p, res.packet_start(), res.packet_end(), target) || target.empty()) {
        target = "Unknown";
    }

    logger::mdns()->trace(fmt::format("PoinTeR: {} -> {}", rr.name, target));
    rr.rdata_serialized = target;

    return true;
}

const std::uint8_t*
mdns::MdnsHelper::parseName(const std::uint8_t*& ptr, const std::uint8_t* start, const std::uint8_t* end, std::string& out) {
    out.clear();

    const std::uint8_t* cur = ptr;
    const std::uint8_t* ret = nullptr;
    int jumpCount = 0;
    bool terminated = false;

    while (cur < end)
    {
        uint8_t len = *cur;

        if ((len & 0xC0) == 0xC0)
        {
            if (cur + 1 >= end)
            {
                logger::mdns()->error("Malformed packet (truncated compression)");
                return nullptr;
            }

            std::uint16_t offset = ((len & 0x3F) << 8) | *(cur + 1);
            if (start + offset >= end) {
                logger::mdns()->error("Malformed packet (bad compression offset)");
                return nullptr;
            }

            if (!ret) {
                ret = cur + 2;
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

    if (!terminated && !ret) {
        logger::mdns()->error("Malformed packet (name not terminated)");
        return nullptr;
    }

    if (!out.empty())
        out.pop_back();

    ptr = ret ? ret : cur;
    return ptr;
}

mdns::proto::mdns_rr
mdns::MdnsHelper::parseRR(const std::uint8_t*& ptr, const std::uint8_t *start, const std::uint8_t *end) {
    proto::mdns_rr record;
    ptr = parseName(ptr, start, end, record.name);

    record.type  = readU16(ptr);
    record.clazz = readU16(ptr);
    record.ttl   = readU32(ptr);

    std::uint16_t rdlen = readU16(ptr);
    record.rdata.assign(ptr, ptr + rdlen);
    ptr += rdlen;

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
        proto::mdns_question q;

        data = parseName(data, packet_start, packet_end, q.name);
        if (data + 4 > packet_end) {
			logger::mdns()->error("Malformed packet (truncated question)");
            return std::nullopt;
        }

        if (q.name == "_services._dns-sd._udp.local") {
            // We found ourselfs
            continue;
        }

        q.type  = readU16(data);
        q.clazz = readU16(data);

        logger::mdns()->info("Pushed anouncment: " + q.name);
        response.questions_list.push_back(std::move(q));
    }

    for (std::uint16_t i = 0; i < answer_rrs; ++i) {
        response.answer_rrs.push_back(parseRR(data, packet_start, packet_end));
    }

    for (std::uint16_t i = 0; i < authority_rrs; ++i) {
        response.authority_rrs.push_back(parseRR(data, packet_start, packet_end));
    }

    for (std::uint16_t i = 0; i < additional_rrs; ++i) {
        response.additional_rrs.push_back(parseRR(data, packet_start, packet_end));
    }

    response.ip_addr_str = message.ip_addr_str;
    response.port        = message.port;

    return response;
}
