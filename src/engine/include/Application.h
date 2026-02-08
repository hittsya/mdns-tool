#ifndef APPLICATION_H
#define APPLICATION_H

#include <GLFW/glfw3.h>
#include <MdnsHelper.h>
#include <Ping46.h>
#include <Settings.h>
#include <Types.h>
#include <imgui.h>
#include <array>
#include <mutex>

namespace mdns::engine {

class Application
{
public:
  Application(int width, int height, const std::string& buildInfo);
  ~Application();
  void run();
  void onWindowResized(int width, int height);
  bool init();

private:
  void handleShortcuts() const;
  void loadAppIcon() const;
  void tryAddService(ScanCardEntry entry, bool isAdvertised);
  void onScanDataReady(std::vector<proto::mdns_response>&& responses);
  void renderUI();
  void sortEntries();
  static void loadTexture(GLuint* dest,
                          unsigned char data[],
                          unsigned int length);
  void renderDiscoveryLayout();
  void setUIScalingFactor(float scalingFactor) const;

private:
  int m_width;
  int m_height;
  std::string m_title;

  GLuint m_logo_texture;
  GLuint m_browser_texture;
  GLuint m_info_texture;
  GLuint m_terminal_texture;

  GLFWwindow* m_window = nullptr;

  std::unique_ptr<MdnsHelper> m_mdns_helper;
  std::unique_ptr<PingTool> m_ping_tool;
  ImGuiStyle m_base_style;

  bool show_help_window = false;
  bool m_show_changelog_window = false;

  bool m_show_dissector_meta_window = false;
  std::optional<ScanCardEntry> m_dissector_meta_entry;

  bool m_open_ping_view = false;
  bool m_open_question_view = false;
  bool m_discovery_running = false;

  std::array<char, 128> m_search_buffer = { '\0' };
  std::vector<ScanCardEntry> m_discovered_services;

  std::mutex m_filtered_services_mutex;
  std::vector<ScanCardEntry> m_filtered_services;

  std::mutex m_intercepted_questions_mutex;
  std::vector<QuestionCardEntry> m_intercepted_questions;

  std::unique_ptr<meta::Settings> m_settings;
};

}

#endif
