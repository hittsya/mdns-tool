#include "Settings.h"
#include <Logger.h>

mdns::meta::Settings::Settings()
{
    m_handler.TypeName = "AppSettings";
    m_handler.TypeHash = ImHashStr("AppSettings");
    m_handler.UserData = this;

    m_handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* h, const char*) -> void* {
         return &static_cast<Settings*>(h->UserData)->m_settings;
    };

    m_handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
        auto* s = static_cast<AppSettings*>(entry);
        std::sscanf(line, "UIScaleFactor=%f", &s->ui_scale_factor);
    };

    m_handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buf)
    {
        auto* self = static_cast<Settings*>(h->UserData);

        buf->appendf("[AppSettings][Main]\n");
        buf->appendf("UIScaleFactor=%f\n", self->m_settings.ui_scale_factor);
        buf->append ("\n");
    };

    ImGui::GetCurrentContext()->SettingsHandlers.push_back(m_handler);
    ImGui::LoadIniSettingsFromDisk(ImGui::GetIO().IniFilename);
}

mdns::meta::Settings::~Settings()
{
    saveSettings();
}

void
mdns::meta::Settings::saveSettings()
{
    logger::core()->info("Saving settings");
    ImGui::SaveIniSettingsToMemory();
    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
}


mdns::meta::Settings::AppSettings&
mdns::meta::Settings::getSettings()
{
    return m_settings;
}
