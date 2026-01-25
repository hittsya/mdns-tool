#ifndef APPLICATION_H
#define APPLICATION_H

#include <GLFW/glfw3.h>
#include <MdnsHelper.h>
#include <Ping46.h>

namespace mdns::engine
{

class Application
{   
public:
    struct ScanCardEntry
    {
        std::string              name;
        std::vector<std::string> ip_addresses;
		std::uint16_t            port;

        bool operator==(const ScanCardEntry& other) const noexcept
        {   
            return name == other.name && port == other.port;
        }
    };

    Application(int width, int height, std::string buildInfo);
    ~Application();
    void run();
private:
    void tryAddService(ScanCardEntry entry);
    void onScanDataReady(std::vector<proto::mdns_response>&& responses);
    float calcServiceCardHeight(std::size_t ipCount);
	void openInBrowser(std::string const& url);
    void renderUI();
    void renderDiscoveryLayout();
    void renderRightSidebarLayout();
    void renderFoundServices();
    void renderServiceCard(int index, std::string const& name, std::vector<std::string> const& ipAddrs, std::uint16_t port);
    void setUIScalingFactor(float scalingFactor);
    float getMonitorScalingFactor();
private:
    int                               m_width;
    int                               m_height;
    std::string                       m_title;
    GLFWwindow*                       m_window = nullptr;
	float                             m_ui_scaling_factor = 1.0f;
    
    MdnsHelper                        m_mdns_helper;
	PingTool                          m_ping_tool;
    bool                              m_open_ping_view = false;

    std::vector<ScanCardEntry>        m_discovered_services;
	bool                              m_discovery_running = false;
};

}

#endif