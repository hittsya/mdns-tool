#include "Util.h"
#include "Logger.h"

void
mdns::engine::util::openInBrowser(const std::string& url)
{
  logger::core()->info("Opening link: " + url);

#ifdef _WIN32
  HINSTANCE res = ShellExecuteA(
    nullptr, nullptr, url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

  if ((INT_PTR)res <= 32) {
    logger::core()->error("Failed opening link: " + url);
  }
#else
  std::string cmd = "xdg-open \"" + url + "\"";
  if (auto result = system(cmd.c_str())) {
    logger::core()->error(
      fmt::format("Command {} failed with rc {}", cmd, result));
  }
#endif
}

void
mdns::engine::util::openShellAndSSH(const std::string& host,
                                    const std::string& user,
                                    int port)
{
  std::string target = user.empty() ? host : user + "@" + host;
  logger::core()->info("Opening SSH session to: " + target);

#ifdef _WIN32
  std::string sshCmd = "ssh " + target;
  if (port != 22) {
    sshCmd += " -p " + std::to_string(port);
  }

  std::string fullCmd = "wt.exe new-tab cmd /k \""
                        "echo Connecting to " +
                        target + " on port " + std::to_string(port) + " && " +
                        sshCmd + "\"";

  HINSTANCE res = ShellExecuteA(nullptr,
                                "open",
                                "cmd.exe",
                                ("/c " + fullCmd).c_str(),
                                nullptr,
                                SW_SHOWNORMAL);

  if ((INT_PTR)res <= 32) {
    logger::core()->error("Failed opening SSH shell: " + target);
  }

#else
  std::string sshCmd = "ssh " + target;
  if (port != 22) {
    sshCmd += " -p " + std::to_string(port);
  }

  std::vector<std::string> terminals = { "x-terminal-emulator",
                                         "gnome-terminal",
                                         "konsole",
                                         "xfce4-terminal",
                                         "xterm" };

  bool launched = false;
  for (const auto& term : terminals) {
    if (system(("command -v " + term + " > /dev/null 2>&1").c_str()) != 0)
      continue;

    std::string wrapped = "bash -c 'echo \">" + sshCmd + "\";echo;" +
                          sshCmd +
                          "; echo; echo \"Press ENTER to close...\"; read'";
    std::string cmd;

    if (term == "gnome-terminal")
      cmd = term + " -- " + wrapped;
    else if (term == "konsole")
      cmd = term + " -e " + wrapped;
    else if (term == "xfce4-terminal")
      cmd = term + " -e " + wrapped;
    else if (term == "xterm")
      cmd = term + " -hold -e " + sshCmd;
    else
      cmd = term + " -e " + wrapped;

    if (system(cmd.c_str()) == 0) {
      launched = true;
      break;
    }
  }

  if (!launched) {
    logger::core()->error("No terminal emulator found to launch SSH.");
  }
#endif
}

std::string
mdns::engine::util::stripMdnsServicePostfix(std::string const& name)
{
  size_t first = name.find("._");
  if (first == std::string::npos) {
    return name;
  }

  size_t second = name.find("._", first + 2);
  if (second == std::string::npos) {
    return name.substr(0, first);
  }

  size_t domain = name.find('.', second + 2);
  if (domain == std::string::npos) {
    return name.substr(0, first);
  }

  return name.substr(0, first) + name.substr(domain);
}

std::string
mdns::engine::util::stripMdnsServicePrefix(const std::string& name)
{
  size_t pos = name.find("._");
  if (pos == std::string::npos)
    return name;

  return name.substr(pos + 1);
}
