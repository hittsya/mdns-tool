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
{}

mdns::MdnsHelper::~MdnsHelper() = default;

void
mdns::MdnsHelper::startBrowse()
{
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
  logger::mdns()->info("Opened " + std::to_string(connections.size()) +
                       " sockets");

  browsing_thread_ =
    std::jthread([this, connections = std::move(connections)](
                   std::stop_token const& stop_token) mutable -> void {
      logger::mdns()->info("Starting continuous mDNS discovery");
      on_browsing_state_changed_(true);
      runDiscovery(stop_token, std::move(connections));
    });
}

void
mdns::MdnsHelper::addResolveQuery(std::string const& query)
{
  if (auto const it = std::ranges::find(browsing_queries_, query);
      it == browsing_queries_.end()) {
    logger::mdns()->info("Adding question: " + query);
    browsing_queries_.push_back(query);
  }
}

std::vector<std::string> const&
mdns::MdnsHelper::getResolveQueries() const
{
  return browsing_queries_;
}

void
mdns::MdnsHelper::removeResolveQuery(std::string const& query)
{
  if (auto const it = std::ranges::find(browsing_queries_, query);
      it != browsing_queries_.end()) {
    logger::mdns()->info("Removing question: " + query);
    browsing_queries_.erase(it);
  }
}

void
mdns::MdnsHelper::stopBrowse()
{
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

std::vector<std::uint8_t>
mdns::MdnsHelper::buildQuery(std::vector<std::string> const& services) const
{
  if (services.empty()) {
    logger::mdns()->info("Service list is empty, baking generic query");

    return std::vector<std::uint8_t>(std::begin(proto::mdns_multi_query),
                                     std::end(proto::mdns_multi_query));
  }

  std::vector<uint8_t> pkt;
  pkt.push_back(0x00);
  pkt.push_back(0x00);
  pkt.push_back(0x00);
  pkt.push_back(0x00);

  uint16_t qdcount = services.size();
  pkt.push_back(qdcount >> 8);
  pkt.push_back(qdcount & 0xFF);

  pkt.insert(pkt.end(), { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }); // AN, NS, AR

  for (auto const& s : services) {
    encodeDnsName(pkt, s);

    pkt.push_back(0x00);
    pkt.push_back(proto::MDNS_RECORDTYPE_PTR);

    pkt.push_back(0x00);
    pkt.push_back(proto::MDNS_CLASS_IN);
  }

  return pkt;
}

void
mdns::MdnsHelper::encodeDnsName(std::vector<uint8_t>& out,
                                std::string const& name)
{
  std::stringstream ss(name);
  std::string label;

  while (std::getline(ss, label, '.')) {
    out.push_back(static_cast<uint8_t>(label.size()));
    out.insert(out.end(), label.begin(), label.end());
  }

  out.push_back(0x00);
}

void
mdns::MdnsHelper::scheduleDiscoveryNow()
{
  logger::mdns()->info(
    "Reseting last query timer. Will send query at the next iteration");
  last_query_time_ = std::chrono::steady_clock::now() - query_interval_;
}

void
mdns::MdnsHelper::runDiscovery(std::stop_token const& stop_token,
                               std::vector<sock_fd_t>&& sockets)
{
  scheduleDiscoveryNow();

  while (!stop_token.stop_requested()) {
    if (auto now = std::chrono::steady_clock::now();
        now - last_query_time_ >= query_interval_) {
      auto const query = buildQuery(browsing_queries_);

      for (auto const socket : sockets) {
        logger::mdns()->info("Sending discovery query: socket FD: " +
                             std::to_string(socket));
        impl_->send_multicast(socket, query.data(), query.size());
      }

      last_query_time_ = now;
    }

    std::vector<proto::mdns_response> result;

    for (auto const messages = impl_->receive_discovery(sockets);
         auto const& message : messages) {
      logger::mdns()->trace("Processing multicast (" +
                            std::to_string(message.blob.size()) + " bytes)");

      if (auto const parsed = parseDiscoveryResponse(message);
          parsed.has_value()) {
        result.push_back(parsed.value());
      } else {
        logger::mdns()->warn("Multicast processing failed");
      }
    }

    on_service_discovered_(std::move(result));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  logger::mdns()->info("Closing sockets");
  for (auto const socket : sockets) {
    impl_->close(socket);
  }

  logger::mdns()->info("Browsing thread stopped");
}

const std::uint8_t*
mdns::MdnsHelper::parseName(const std::uint8_t*& ptr,
                            const std::uint8_t* start,
                            const std::uint8_t* end,
                            std::string& out)
{
  out.clear();

  const std::uint8_t* cur = ptr;
  const std::uint8_t* next = nullptr;
  int jumpCount = 0;
  bool terminated = false;

  while (cur < end) {
    std::uint8_t len = *cur;

    if ((len & 0xC0) == 0xC0) {
      if (cur + 1 >= end) {
        logger::mdns()->error(
          "Malformed packet (truncated compression pointer)");
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
    logger::mdns()->error("Malformed packet (name not terminated) -- " + out);
    return nullptr;
  }

  if (!out.empty()) {
    out.pop_back();
  }

  ptr = next ? next : cur;
  return ptr;
}

mdns::proto::mdns_rr
mdns::MdnsHelper::parseRR(const std::uint8_t*& ptr,
                          const std::uint8_t* start,
                          const std::uint8_t* end)
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

  record.port = 0;
  record.type = readU16(ptr);
  record.clazz = readU16(ptr);
  record.ttl = readU32(ptr);

  std::uint16_t rdlen = readU16(ptr);
  if (ptr + rdlen > end) {
    logger::mdns()->error("Malformed RR (RDATA overrun)");
    return record;
  }

  const std::uint8_t* rdata_start = ptr;
  const std::uint8_t* rdata_end = ptr + rdlen;

  switch (record.type) {
    case mdns::proto::MDNS_RECORDTYPE_PTR: {
      const std::uint8_t* tmp = rdata_start;

      if (std::string target; parseName(tmp, start, end, target)) {
        record.name = "";
        record.rdata = mdns::proto::mdns_rr_ptr_ext{ target };
      }
    } break;

    case mdns::proto::MDNS_RECORDTYPE_TXT: {
      const std::uint8_t* tmp = rdata_start;
      mdns::proto::mdns_rr_txt_ext rr_txt{};

      while (tmp < rdata_end) {
        std::uint8_t len = *tmp++;
        if (tmp + len > rdata_end) {
          break;
        }

        std::string txt(reinterpret_cast<const char*>(tmp), len);
        rr_txt.entries.emplace_back(txt);
        record.name += txt;
        tmp += len;
      }

      record.rdata = rr_txt;
    } break;

    case mdns::proto::MDNS_RECORDTYPE_SRV: {
      if (rdlen < 6) {
        logger::mdns()->error("Malformed SRV record (too short)");
        break;
      }

      const std::uint8_t* tmp = rdata_start;
      mdns::proto::mdns_rr_srv_ext srv;

      srv.priority = readU16(tmp);
      srv.weight = readU16(tmp);
      srv.port = readU16(tmp);

      if (!parseName(tmp, start, end, srv.target)) {
        logger::mdns()->warn(
          "Failed parsing name(MDNS_RECORDTYPE_SRV), dropping packet");
        break;
      }

      logger::mdns()->trace(
        fmt::format("Discovered SRV record: {} -> {}:{} (prio={}, weight={})",
                    record.name,
                    srv.target,
                    srv.port,
                    srv.priority,
                    srv.weight));

      record.port = srv.port;
      record.rdata = std::move(srv);
    } break;

    case mdns::proto::MDNS_RECORDTYPE_A: {
      if (rdlen == 4) {
        std::string advertisedIP;
        advertisedIP.resize(INET_ADDRSTRLEN);

        if (!inet_ntop(
              AF_INET, rdata_start, advertisedIP.data(), advertisedIP.size())) {
          logger::mdns()->trace("Failed parsing MDNS_RECORDTYPE_A packet");
          break;
        }

        advertisedIP.resize(std::strlen(advertisedIP.c_str()));

        logger::mdns()->trace("Discovered A record: " + advertisedIP + " / " +
                              record.name);
        record.rdata = mdns::proto::mdns_rr_a_ext{ advertisedIP };
      }
    } break;

    case mdns::proto::MDNS_RECORDTYPE_AAAA: {
      if (rdlen == 16) {
        std::string advertisedIP;
        advertisedIP.resize(INET6_ADDRSTRLEN);

        if (!inet_ntop(AF_INET6,
                       rdata_start,
                       advertisedIP.data(),
                       advertisedIP.size())) {
          logger::mdns()->trace("Failed parsing MDNS_RECORDTYPE_AAAA packet");
          break;
        }

        advertisedIP.resize(std::strlen(advertisedIP.c_str()));
        logger::mdns()->trace("Discovered AAAA record: " + advertisedIP +
                              " / " + record.name);
        record.rdata = mdns::proto::mdns_rr_a_ext{ advertisedIP };
      }
    } break;

    case mdns::proto::MDNS_RECORDTYPE_NSEC: {
      const std::uint8_t* tmp = rdata_start;
      mdns::proto::mdns_rr_nsec_ext nsec;

      if (!parseName(tmp, start, end, nsec.next_domain)) {
        logger::mdns()->warn("Failed parsing NSEC next domain");
        break;
      }

      while (tmp < rdata_end) {
        if (tmp + 2 > rdata_end) {
          logger::mdns()->warn("Malformed NSEC bitmap");
          break;
        }

        uint8_t window = *tmp++;
        uint8_t len = *tmp++;

        if (tmp + len > rdata_end) {
          logger::mdns()->warn("NSEC bitmap overflow");
          break;
        }

        for (uint8_t i = 0; i < len; ++i) {
          uint8_t byte = tmp[i];
          for (int bit = 0; bit < 8; ++bit) {
            if (byte & (1 << (7 - bit))) {
              uint16_t rrtype = window * 256 + i * 8 + bit;
              nsec.types.push_back(rrtype);
            }
          }
        }

        tmp += len;
      }

      logger::mdns()->trace(fmt::format("NSEC record for {} indicates {} types",
                                        record.name,
                                        nsec.types.size()));

      record.rdata = std::move(nsec);
    } break;

    default: {
      logger::mdns()->error(fmt::format(
        "Failed parsing RR, setting type to unknown: {}", record.type));
      record.rdata = mdns::proto::mdns_rr_unknown_ext{
        std::vector<std::uint8_t>{ rdata_start, rdata_end }
      };
    } break;
  }

  logger::mdns()->trace(
    fmt::format("Parsed RR: name='{}' type={} class={} ttl={} rdlen={}",
                record.name,
                record.type,
                record.clazz,
                record.ttl,
                rdlen));

  ptr = rdata_end;
  return record;
}

std::uint16_t
mdns::MdnsHelper::readU16(const std::uint8_t*& ptr)
{
  auto const result = ntohs(*reinterpret_cast<const std::uint16_t*>(ptr));
  ptr += sizeof(std::uint16_t);
  return result;
}

std::uint32_t
mdns::MdnsHelper::readU32(const std::uint8_t*& ptr)
{
  auto const result = ntohl(*reinterpret_cast<const std::uint32_t*>(ptr));
  ptr += sizeof(std::uint32_t);
  return result;
}

std::optional<mdns::proto::mdns_response>
mdns::MdnsHelper::parseDiscoveryResponse(proto::mdns_recv_res const& message)
{
  auto const& buffer = message.blob;

  if (buffer.size() < sizeof(std::uint16_t) * 6) {
    logger::mdns()->warn(
      "mDNS packet too small: " + std::to_string(buffer.size()) + " bytes");
    return std::nullopt;
  }

  const auto* packet_start = reinterpret_cast<const uint8_t*>(buffer.data());
  const auto* packet_end = packet_start + buffer.size();
  const auto* data = packet_start;

  proto::mdns_response response{};
  response.packet.assign(packet_start, packet_end);

  if (data + 12 > packet_end) {
    logger::mdns()->error("Malformed packet (truncated header)");
    return std::nullopt;
  }

  response.time_of_arrival = std::chrono::steady_clock::now();
  response.query_id = readU16(data);
  response.flags = readU16(data);
  response.questions = readU16(data);

  std::uint16_t const answer_rrs = readU16(data);
  std::uint16_t const authority_rrs = readU16(data);
  std::uint16_t const additional_rrs = readU16(data);

  logger::mdns()->trace(
    fmt::format("Header: id={} flags=0x{:04X} qd={} an={} ns={} ar={}",
                response.query_id,
                response.flags,
                response.questions,
                answer_rrs,
                authority_rrs,
                additional_rrs));

  for (std::uint16_t i = 0; i < response.questions; ++i) {
    proto::mdns_question q{};

    const auto* name_end = parseName(data, packet_start, packet_end, q.name);
    if (!name_end || name_end + 4 > packet_end) {
      logger::mdns()->error("Malformed packet (bad question name)");
      return std::nullopt;
    }

    data = name_end;
    q.type = readU16(data);
    q.clazz = readU16(data);

    logger::mdns()->info("Pushed question query: " + q.name);
    response.questions_list.push_back(std::move(q));
  }

  auto parse_rr_block = [&](std::vector<proto::mdns_rr>& out,
                            std::uint16_t count,
                            std::string& advertizedIP) -> void {
    for (std::uint16_t i = 0; i < count; ++i) {
      const auto* before = data;

      auto rr = parseRR(data, packet_start, packet_end);
      if (data <= before || data > packet_end) {
        logger::mdns()->error("Malformed packet (bad RR parse)");
        continue;
      }

      // If its the MDNS_RECORDTYPE_A/MDNS_RECORDTYPE_AAAA then set the
      // advertised ip
      std::visit(
        [&]<typename T0>(T0&& entry) {
          using T = std::decay_t<T0>;

          if constexpr (std::is_same_v<T, proto::mdns_rr_a_ext> ||
                        std::is_same_v<T, proto::mdns_rr_aaaa_ext>) {
            advertizedIP = entry.address;
          }
        },
        rr.rdata);

      out.push_back(std::move(rr));
    }

    return;
  };

  parse_rr_block(
    response.answer_rrs, answer_rrs, response.advertized_ip_addr_str);
  parse_rr_block(
    response.authority_rrs, authority_rrs, response.advertized_ip_addr_str);
  parse_rr_block(
    response.additional_rrs, additional_rrs, response.advertized_ip_addr_str);

  response.ip_addr_str = message.ip_addr_str;
  response.port = message.port;

  return response;
}

void
mdns::MdnsHelper::connectOnServiceDiscovered(service_dicovered_cb cb)
{
  on_service_discovered_ = std::move(cb);
}

void
mdns::MdnsHelper::connectOnBrowsingStateChanged(browse_en_cb cb)
{
  on_browsing_state_changed_ = std::move(cb);
}
