#ifndef SETTINGS_H
#define SETTINGS_H

#include <imgui.h>
#include <imgui_internal.h>

namespace mdns::meta
{

class Settings
{
public:
    struct AppSettings
    {
        float ui_scale_factor = 1.0f;
    };

    Settings();
    ~Settings();
    AppSettings& getSettings();
    void saveSettings();
private:
    ImGuiSettingsHandler m_handler;
    AppSettings          m_settings;
};

}

#endif
