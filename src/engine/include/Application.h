#ifndef APPLICATION_H
#define APPLICATION_H

#include <GLFW/glfw3.h>
#include <MdnsHelper.h>

namespace mdns::engine
{

class Application
{
public:
    Application(int width, int height, const char* title);
    ~Application();
    void run();

private:
    void renderUI();
    void renderDiscoveryLayout();
    void renderFoundServices();
    void renderServiceCard(int index, std::string const& name, std::string const ipAddr, std::uint16_t port);
    void setUIScalingFactor(float scalingFactor);
    float getMonitorScalingFactor();
private:
    int               m_width;
    int               m_height;
    const char*       m_title;
    GLFWwindow*       m_window = nullptr;

	float                             m_ui_scaling_factor = 1.0f;
    MdnsHelper                        m_mdns_helper;
    std::vector<proto::mdns_response> m_discovered_services;
};

}

#endif