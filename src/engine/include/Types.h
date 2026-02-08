#ifndef TYPES_H
#define TYPES_H

#include <Proto.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace mdns::engine {

struct CardEntry
{
  std::string name;
  std::vector<std::string> ip_addresses;
  std::uint16_t port;
  std::vector<proto::mdns_rdata> dissector_meta;
  std::chrono::steady_clock::time_point time_of_arrival;
};

struct ScanCardEntry : public CardEntry
{
  // Services are unique only by their name
  bool operator==(const ScanCardEntry& other) const noexcept
  {
    return name == other.name;
  }
};

struct QuestionCardEntry : public CardEntry
{
  // Questions are unqiue by their source and name
  bool operator==(const QuestionCardEntry& other) const noexcept
  {
    return name == other.name && ip_addresses[0] == other.ip_addresses[0];
  }
};

}

#endif // TYPES_H