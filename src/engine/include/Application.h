#ifndef APPLICATION_H
#define APPLICATION_H

#include <GLFW/glfw3.h>
#include <MdnsHelper.h>
#include <Ping46.h>
#include <Types.h>
#include <Settings.h>
#include <imgui.h>

namespace mdns::engine
{

class Application
{   
public:
    Application(int width, int height, std::string buildInfo);
    ~Application();
    void run();
    void onWindowResized(int width, int height);
private:
    void handleShortcuts();
    void loadAppIcon();
    void loadAppLogoTexture();
    void tryAddService(ScanCardEntry entry, bool isAdvertized);
    void onScanDataReady(std::vector<proto::mdns_response>&& responses);
    void renderUI();
    void renderDiscoveryLayout();
    void setUIScalingFactor(float scalingFactor);
private:
    int                               m_width;
    int                               m_height;
    std::string                       m_title;
    GLuint                            m_logo_texture;
    
    GLFWwindow*                       m_window = nullptr;
    MdnsHelper                        m_mdns_helper;
	PingTool                          m_ping_tool;
    ImGuiStyle                        m_base_style;

    bool 							  show_help_window = false;
    bool 							  m_show_changelog_window = false;
    
    bool 							  m_show_dissector_meta_window = false;
    std::optional<ScanCardEntry>      m_dissector_meta_entry;

    bool                              m_open_ping_view = false;
    bool                              m_discovery_running = false;

    std::vector<ScanCardEntry>        m_discovered_services;
    std::vector<QuestionCardEntry>    m_intercepted_questions;

    std::unique_ptr<meta::Settings>   m_settings;
};

}

#endif
