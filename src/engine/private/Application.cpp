#include <Application.h>
#include <Ping46.h>

#include <imgui.h>
#include <algorithm>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stdexcept>
#include <Logger.h>

#if defined(WIN32)
    #include "windows.h"
#endif

#if defined(__APPLE__)
    #define GL_SILENCE_DEPRECATION
#endif

mdns::engine::Application::Application(int width, int height, std::string buildInfo)
    : m_width(width), m_height(height)
{
    logger::init();
    logger::core()->info("Logger initialized");
    logger::core()->info("Build " + buildInfo);


    if (!glfwInit()) {
        logger::core()->error("Failed to initialize GLFW");
        throw std::runtime_error("Failed to initialize GLFW");
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE       , GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

#if defined(WIN32)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

	m_title  = "mDNS Browser - " + buildInfo;
    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!m_window) {
        logger::core()->error("Failed to create window");
        throw std::runtime_error("Failed to create window");
    }

    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwSetWindowContentScaleCallback(
        m_window,
        [](GLFWwindow*, float xscale, float yscale) {
            ImGuiIO& io = ImGui::GetIO();
            io.FontGlobalScale = xscale;
        }
    );

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    logger::core()->info("GLFW initialized");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");


    m_ui_scaling_factor = getMonitorScalingFactor();
    logger::core()->info("Monitor scaling is " + std::to_string(m_ui_scaling_factor));

    setUIScalingFactor(m_ui_scaling_factor);
    logger::core()->info("ImGUI initialized");

    m_mdns_helper.connectOnServiceDiscovered(
        [this](std::vector<proto::mdns_response>&& responses) -> void {
            onScanDataReady(std::move(responses));
        }
	);

    m_mdns_helper.connectOnBrowsingStateChanged(
        [this](bool enabled) -> void {
			m_discovery_running = enabled;
        }
    );
}

mdns::engine::Application::~Application()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    logger::core()->info("ImGUI terminated");

    glfwDestroyWindow(m_window);
    glfwTerminate();

    logger::core()->info("GLFW terminated");
    logger::core()->info("Shutting down logger");

    logger::shutdown();
}

float 
mdns::engine::Application::getMonitorScalingFactor()
{
    float xscale, yscale;
    glfwGetWindowContentScale(m_window, &xscale, &yscale);
    return xscale;
}

void 
mdns::engine::Application::setUIScalingFactor(float scalingFactor)
{
    ImFontConfig cfg;
    cfg.SizePixels = 13.0f * scalingFactor;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetStyle().ScaleAllSizes(scalingFactor);

    io.Fonts->Clear();
    io.Fonts->AddFontDefault(&cfg);
    io.Fonts->Build();
}

void
mdns::engine::Application::run()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderUI();
        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(m_window, &w, &h);
        glViewport  (0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear     (GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }
}

void
mdns::engine::Application::renderUI()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos     (viewport->Pos);
    ImGui::SetNextWindowSize    (viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding  , 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin       ("##FullscreenRoot", nullptr, windowFlags);

    renderDiscoveryLayout();

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void 
mdns::engine::Application::renderServiceCard(int index, std::string const& name, std::vector<std::string> const& ipAddrs, std::uint16_t port)
{
    if(name.empty()) {
        return;
	}

	auto const height = calcServiceCardHeight(ipAddrs.size());

    ImGui::PushID(index);
    ImGui::BeginChild("ServiceCard", ImVec2(0, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiChildFlags_AutoResizeY);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::SetWindowFontScale(1.1f);

    float windowWidth = ImGui::GetContentRegionAvail().x;
    float textWidth = ImGui::CalcTextSize(name.c_str()).x;

    ImGui::SeparatorText(name.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::Text("Hostname:");
    ImGui::SameLine(200);
    ImGui::Text("%s", name.c_str());

    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    ImGui::Text("IP Address(es):");
    ImGui::SameLine(200);
    ImGui::Indent(190);

    for (auto const& ipAddr: ipAddrs) {
        ImGui::PushID(ipAddr.c_str());
        
        if(ImGui::Selectable(ipAddr.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
            ImGui::OpenPopup("ip_actions_popup");
        }

        if (ImGui::BeginPopup("ip_actions_popup")) {
            ImGui::Text("IP: %s", ipAddr.c_str());
            ImGui::Separator();

            if (ImGui::Button("Ping")) {
				m_ping_tool.pingIpAddress(ipAddr);
                m_open_ping_view = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Open in browser")) {
                std::string url = name.find("https") != std::string::npos ? "https" : "http";

                url += "://";
                url += ipAddr;

                if (port != mdns::proto::port) {
                    url += ":" + std::to_string(port);
                }

                openInBrowser(url);
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::Unindent(190);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));

    ImGui::Text("Port:");
    ImGui::SameLine(200);
    ImGui::Text("%d", port);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (ImGui::Button("Open in browser")) {
        std::string url = name.find("https") != std::string::npos ? "https": "http";
        
        url += "://";
        url += name;
        
        if (port != mdns::proto::port) {
            url += ":" + std::to_string(port);
        }
        
        openInBrowser(url);
    }

    ImGui::SameLine();

    /*if (ImGui::Button("Advanced data")) {

    }*/

    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::PopID();
}

void
mdns::engine::Application::renderFoundServices()
{
    for (auto const& service: m_discovered_services) {
        renderServiceCard(
            static_cast<int>(&service - &m_discovered_services[0]),
            service.name,
            service.ip_addresses,
            service.port
        );
	}
}

void 
mdns::engine::Application::openInBrowser(const std::string& url)
{
#ifdef _WIN32
    HINSTANCE res = ShellExecuteA(
        nullptr,
        nullptr,
        url.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );

    if ((INT_PTR)res <= 32) {
        logger::core()->error("Failed opening link: " + url);
    }
#else 
    std::string cmd = "xdg-open \"" + url + "\"";
    system(cmd.c_str());
#endif
}

void
mdns::engine::Application::renderRightSidebarLayout()
{
    if (m_open_ping_view)
    {
        ImGui::SameLine();
        ImGui::BeginGroup();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::Text("Ping Tool");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
        if (ImGui::Button("X##ClosePing")) {
            m_ping_tool.stopPing();
            m_open_ping_view = false;
        }
        
        ImGui::PopStyleVar();
        ImGui::Separator();
        ImGui::BeginChild("PingPanel", ImVec2(600, 0), true);
            
        ImGui::Text("Statistics:");
		
        auto const& stats = m_ping_tool.getStats();
        ImGui::Columns(2, "stats", false);
        ImGui::Text("Sent: %d", stats.send);
        ImGui::Text("Received: %d", stats.received);
        ImGui::Text("Lost: %d", stats.lost);
        ImGui::NextColumn();
        ImGui::Text("Min: %d ms", stats.min == INT_MAX? 0: stats.min);
        ImGui::Text("Max: %d ms", stats.max);
        ImGui::Text("Avg: %d ms", stats.average);
        ImGui::Columns(1);

        ImGui::Separator();

        ImGui::Text("Response Time");
        static int historyOffset = 0;

        ImGui::PlotLines("##PingGraph", stats.history.data(), stats.history.size(), historyOffset, nullptr, 0.0f, 100.0f, ImVec2(-1, 80));
        ImGui::Separator();

        ImGui::Text("Output:");
        ImGui::BeginChild("PingOutput", ImVec2(0, 0), true);

        ImGui::Text(m_ping_tool.getOutput().data());

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::EndGroup();
    }
}

void
mdns::engine::Application::renderDiscoveryLayout()
{
    if (ImGui::CollapsingHeader("Browse mDNS services", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginGroup();
        ImGui::BeginChild("MainContent", ImVec2(m_open_ping_view ? -600 : 0, 0), false);

        /*static char searchBuffer[128] = "";
        ImGui::SetNextItemWidth(250);
        ImGui::InputTextWithHint("##ServiceSearch", "Search services...", searchBuffer, IM_ARRAYSIZE(searchBuffer));*/

        //ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        
        ImGui::BeginDisabled(m_discovery_running);
        if (ImGui::Button("Browse")) {
            m_mdns_helper.startBrowse();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);

        ImGui::BeginDisabled(!m_discovery_running);
        if (ImGui::Button("Stop")) {
            m_mdns_helper.stopBrowse();
        }
        ImGui::EndDisabled();

        renderFoundServices();

        ImGui::EndChild();

        renderRightSidebarLayout();

        ImGui::EndGroup();
    }
}

void 
mdns::engine::Application::onScanDataReady(std::vector<proto::mdns_response>&& responses)
{
    ScanCardEntry entry;

    for (std::size_t entry_idx = 0; entry_idx < responses.size(); ++entry_idx) {
        auto const& target_service = responses[entry_idx];
        auto const  advertized     = target_service.advertized_ip_addr_str.size() > 0;
        
        entry.ip_addresses = { advertized ? target_service.advertized_ip_addr_str: target_service.ip_addr_str };
        entry.port         = target_service.port;

        for (std::size_t answer_idx = 0; answer_idx < target_service.answer_rrs.size(); ++answer_idx) {
			entry.name = target_service.answer_rrs[answer_idx].rdata_serialized;
            if (target_service.answer_rrs[answer_idx].port != 0) {
                entry.port = target_service.answer_rrs[answer_idx].port;
            }

            tryAddService(entry, advertized);
        }

        for (std::size_t answer_idx = 0; answer_idx < target_service.additional_rrs.size(); ++answer_idx) {
            entry.name = target_service.additional_rrs[answer_idx].rdata_serialized;
            if (target_service.additional_rrs[answer_idx].port != 0) {
                entry.port = target_service.additional_rrs[answer_idx].port;
            }

            tryAddService(entry, advertized);
        }

        for (std::size_t answer_idx = 0; answer_idx < target_service.authority_rrs.size(); ++answer_idx) {
            entry.name = target_service.authority_rrs[answer_idx].rdata_serialized;
            if (target_service.authority_rrs[answer_idx].port != 0) {
                entry.port = target_service.authority_rrs[answer_idx].port;
            }

            tryAddService(entry, advertized);
        }

        for (std::size_t answer_idx = 0; answer_idx < target_service.questions_list.size(); ++answer_idx) {
            entry.name = target_service.questions_list[answer_idx].name;
            tryAddService(entry, advertized);
        }
    }
}

void
mdns::engine::Application::tryAddService(ScanCardEntry entry, bool isAdvertized)
{
    auto serviceIt = std::find(m_discovered_services.begin(), m_discovered_services.end(), entry);
    if (serviceIt == m_discovered_services.end()) {
        m_discovered_services.push_back(std::move(entry));
        return;
	}

    if (serviceIt->port == mdns::proto::port && serviceIt->port != entry.port) {
        // Handle case where SRV record with port appeared after all records
        // TODO: Weight?
        serviceIt->port = entry.port;
    }

    if (isAdvertized) {
        // Handle case where anounced service is also advertizing an address.
		// We give priority to advertized IPs.
        serviceIt->ip_addresses = entry.ip_addresses;
        return;
    }

    auto ipIt = std::find(
        serviceIt->ip_addresses.begin(),
        serviceIt->ip_addresses.end(),
        entry.ip_addresses.front()
	);

	if (ipIt == serviceIt->ip_addresses.end()) {
        serviceIt->ip_addresses.push_back(entry.ip_addresses.front());

        std::sort(
            serviceIt->ip_addresses.begin(),
            serviceIt->ip_addresses.end(),
            [](std::string const& a, std::string const& b) -> bool {
                const bool a4 = a.find(':') == std::string::npos;
                const bool b4 = b.find(':') == std::string::npos;

                if (a4 != b4) {
                    return a4;
                }

                return a < b;
            }
        );
    }
}

float 
mdns::engine::Application::calcServiceCardHeight(std::size_t ipCount)
{
    ImGuiStyle const& style = ImGui::GetStyle();
    ImGuiIO const& io       = ImGui::GetIO();

    const float line    = ImGui::GetTextLineHeight();
    const float spacing = style.ItemSpacing.y;
    const float padding = style.WindowPadding.y * 2.0f;
    const float frame   = style.FramePadding.y * 2.0f;

    float height = 0.0f;
    height += line * 1.1f + spacing;
    height += 4.0f;
    height += line + spacing;
    height += 3.0f;
    height += line;
    height += ipCount * (line + spacing);
    height += 3.0f;
    height += line + spacing;
    height += 8.0f;
    height += ImGui::GetFrameHeight() + spacing;
    height += padding + frame;

    return height;
}
