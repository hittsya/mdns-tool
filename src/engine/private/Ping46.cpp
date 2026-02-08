#include "Ping46.h"
#include <algorithm>
#include <string>

#include <Logger.h>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static bool
extractTimeMs(const std::string& line, int& timeMs)
{
  auto pos = line.find("time");
  if (pos == std::string::npos) {
    return false;
  }

  pos += 4;

  if (pos < line.size() && line[pos] == '<') {
    timeMs = 0;
    return true;
  }

  if (pos >= line.size() || line[pos] != '=') {
    return false;
  }

  pos += 1;
  size_t end = line.find("ms", pos);
  if (end == std::string::npos) {
    return false;
  }

  timeMs = std::stoi(line.substr(pos, end - pos));
  return true;
}

static void
pushHistory(std::vector<float>& history, int value)
{
  if (history.size() >= 100) {
    history.erase(history.begin());
  }

  history.push_back(static_cast<float>(value));
}

void
mdns::engine::PingTool::pingIpAddress(const std::string& ipAddress)
{
  logger::net()->info("Starting ping tool for IP: " + ipAddress);
  std::function<void(std::string const&, std::stop_token const&)> pingFunction;

  if (ipAddress.find(':') != std::string::npos) {
    pingFunction = [this](std::string const& addr,
                          std::stop_token const& token) {
      pingIpv6(addr, token);
    };
  } else {
    pingFunction = [this](std::string const& addr,
                          std::stop_token const& token) {
      pingIpv4(addr, token);
    };
  }

  m_thread = std::jthread(
    [this, pingFunction, ipAddress](std::stop_token const& stop_token) -> void {
      logger::net()->info("Starting ping thread");
      resetStats();
      pingFunction(ipAddress, stop_token);
    });
}

void
mdns::engine::PingTool::stopPing()
{
  logger::net()->info("Stopping ping thread");
  m_thread.request_stop();

  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void
mdns::engine::PingTool::resetStats()
{
  m_stats.send = 0;
  m_stats.received = 0;
  m_stats.lost = 0;
  m_stats.average = 0;
  m_stats.min = INT_MAX;
  m_stats.max = 0;
  m_stats.history.clear();
}

void
mdns::engine::PingTool::ping(const std::string& command,
                             std::stop_token const& token)
{
  logger::net()->info("Will execute command: " + command);

  m_output.clear();
  m_output += "\n== Ping tool started == \n";

#ifdef _WIN32
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE readPipe = nullptr;
  HANDLE writePipe = nullptr;

  if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
    m_output = "CreatePipe failed";
    return;
  }

  SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = writePipe;
  si.hStdError = writePipe;

  PROCESS_INFORMATION pi{};

  std::string cmd = "cmd /c " + command;

  if (!CreateProcessA(nullptr,
                      cmd.data(),
                      nullptr,
                      nullptr,
                      TRUE,
                      CREATE_NO_WINDOW,
                      nullptr,
                      nullptr,
                      &si,
                      &pi)) {
    CloseHandle(readPipe);
    CloseHandle(writePipe);
    m_output = "CreateProcess failed";
    return;
  }

  CloseHandle(writePipe);

  char buffer[256];
  DWORD bytesRead = 0;
  int totalTime = 0;
  std::string pending;

  while (!token.stop_requested()) {
    if (!ReadFile(readPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) ||
        bytesRead == 0)
      break;

    buffer[bytesRead] = '\0';
    pending += buffer;

    size_t pos = 0;
    while ((pos = pending.find('\n')) != std::string::npos) {
      std::string line = pending.substr(0, pos);
      pending.erase(0, pos + 1);

      m_output += line + "\n";
      if (line.find("Reply from") != std::string::npos ||
          line.find("Request timed out") != std::string::npos) {
        m_stats.send++;
      }

      if (line.find("Reply from") != std::string::npos) {
        m_stats.received++;

        int timeMs = 0;
        if (extractTimeMs(line, timeMs)) {
          totalTime += timeMs;
          m_stats.min = (std::min)(m_stats.min, timeMs);
          m_stats.max = (std::max)(m_stats.max, timeMs);
          pushHistory(m_stats.history, timeMs);
        }
      }
    }
  }

  if (token.stop_requested()) {
    TerminateProcess(pi.hProcess, 0);
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(readPipe);
#endif

#ifndef _WIN32
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    logger::net()->error("pipe() failed");
    m_output = "pipe() failed";
    return;
  }

  pid_t pid = fork();
  if (pid == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
    _exit(1);
  }

  close(pipefd[1]);

  int flags = fcntl(pipefd[0], F_GETFL, 0);
  fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

  char buffer[256];
  int totalTime = 0;

  while (!token.stop_requested()) {
    if (ssize_t const n = read(pipefd[0], buffer, sizeof(buffer) - 1); n > 0) {
      buffer[n] = '\0';
      std::string line(buffer);
      m_output += line;

      if (line.find("bytes from") != std::string::npos ||
          line.find("Destination Host Unreachable") != std::string::npos) {
        m_stats.send++;
      }

      if (line.find("bytes from") != std::string::npos) {
        m_stats.received++;

        if (int timeMs = 0; extractTimeMs(line, timeMs)) {
          totalTime += timeMs;
          m_stats.min = std::min(m_stats.min, timeMs);
          m_stats.max = std::max(m_stats.max, timeMs);
          pushHistory(m_stats.history, timeMs);
        }
      }
    } else if (n == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        usleep(10 * 1000);
        continue;
      } else {
        break;
      }
    } else {
      break;
    }
  }

  if (token.stop_requested()) {
    kill(pid, SIGTERM);
  }

  waitpid(pid, nullptr, 0);
  close(pipefd[0]);
#endif
}

void
mdns::engine::PingTool::pingIpv4(const std::string& ipAddress,
                                 std::stop_token const& token)
{
#ifdef WIN32
  std::string command = "ping -4 -t " + ipAddress;
#else
  std::string command = "ping -4 " + ipAddress;
#endif

  ping(command, token);
}

void
mdns::engine::PingTool::pingIpv6(const std::string& ipAddress,
                                 std::stop_token const& token)
{
#ifdef WIN32
  std::string command = "ping -6 -t " + ipAddress;
#else
  std::string command = "ping -6 " + ipAddress;
#endif

  ping(command, token);
}