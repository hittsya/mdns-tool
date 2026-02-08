#ifndef PING46_H
#define PING46_H

#include <functional>
#include <string>
#include <thread>

namespace mdns::engine {

class PingTool
{
public:
  struct PingStats
  {
    int send;
    int received;
    int lost;
    int average;
    int min;
    int max;
    std::vector<float> history;
  };

  PingTool() = default;
  ~PingTool() { stopPing(); }

  void pingIpAddress(const std::string& ipAddress);
  void stopPing();
  std::string const& getOutput() const { return m_output; }
  PingStats const& getStats() const { return m_stats; }

private:
  void resetStats();
  void pingIpv4(std::string const& ipAddress, std::stop_token const& token);
  void pingIpv6(std::string const& ipAddress, std::stop_token const& token);
  void ping(std::string const& command, std::stop_token const& token);

private:
  std::string m_output;
  std::jthread m_thread;
  PingStats m_stats;
};

}

#endif // PING46_H