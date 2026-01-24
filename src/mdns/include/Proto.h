#ifndef PROTO_H
#define PROTO_H

#include <vector>
#include <string>

namespace mdns::proto {

static constexpr int port             = 5353;
static constexpr int unicast_response = 0x8000U;
static constexpr int cache_flush      = 0x8000U;

struct mdns_recv_res {
    std::string       ip_addr_str;
    std::uint16_t     port;
    std::vector<char> blob;
};

struct mdns_question {
    std::string name;
    uint16_t    type;
    uint16_t    clazz;
};

struct mdns_rr {
    std::string           name;
    std::uint16_t         type;
    std::uint16_t         clazz;
    std::uint32_t         ttl;
    std::vector<uint8_t>  rdata;
    std::string           rdata_serialized;
};

struct mdns_response {
    std::uint16_t query_id;
    std::uint16_t flags;
    std::uint16_t questions;

    std::vector<mdns_rr>       answer_rrs;
    std::vector<mdns_rr>       additional_rrs;
    std::vector<mdns_rr>       authority_rrs;
    std::vector<mdns_question> questions_list;
    std::vector<uint8_t>       packet;

    std::string   ip_addr_str;
    std::uint16_t port;

    const uint8_t* packet_start() const {
        return packet.data();
    }

    const uint8_t* packet_end() const {
        return packet.data() + packet.size();
    }
};

enum mdns_record_type {
    MDNS_RECORDTYPE_IGNORE = 0,
    // Address
    MDNS_RECORDTYPE_A = 1,
    // Domain Name pointer
    MDNS_RECORDTYPE_PTR = 12,
    // Arbitrary text string
    MDNS_RECORDTYPE_TXT = 16,
    // IP6 Address [Thomson]
    MDNS_RECORDTYPE_AAAA = 28,
    // Server Selection [RFC2782]
    MDNS_RECORDTYPE_SRV = 33
};

enum mdns_entry_type {
    MDNS_ENTRYTYPE_QUESTION = 0,
    MDNS_ENTRYTYPE_ANSWER = 1,
    MDNS_ENTRYTYPE_AUTHORITY = 2,
    MDNS_ENTRYTYPE_ADDITIONAL = 3
};

enum mdns_class { MDNS_CLASS_IN = 1 };

static constexpr uint8_t mdns_services_query[] = {
    // Query ID
    0x00, 0x00,
    // Flags
    0x00, 0x00,
    // 1 question
    0x00, 0x01,
    // No answer, authority or additional RRs
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // _services._dns-sd._udp.local.
    0x09, '_', 's', 'e', 'r', 'v', 'i', 'c', 'e', 's', 0x07, '_', 'd', 'n', 's', '-', 's', 'd',
    0x04, '_', 'u', 'd', 'p', 0x05, 'l', 'o', 'c', 'a', 'l', 0x00,
    // PTR record
    0x00, MDNS_RECORDTYPE_PTR,
    // QU (unicast response) and class IN
    0x80, MDNS_CLASS_IN
};

}

#endif // PROTO_H
