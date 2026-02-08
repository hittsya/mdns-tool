#ifndef SETTINGS_H
#define SETTINGS_H

#include <imgui.h>
#include <imgui_internal.h>
#include <optional>

namespace mdns::meta {

class Settings
{
public:
  struct AppSettings
  {
    std::optional<float> ui_scale_factor;
    std::optional<int> window_width;
    std::optional<int> window_height;
  };

  Settings();
  ~Settings();
  AppSettings& getSettings();
  void saveSettings();

private:
  ImGuiSettingsHandler m_handler;
  AppSettings m_settings;
};

}

#endif
