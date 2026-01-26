#include <Application.h>
#include <Ping46.h>

#include <imgui.h>
#include <algorithm>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <AppIcon.h>
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

    m_ui_scaling_factor = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
	m_title             = "mDNS Scanner - " + buildInfo;
    m_window            = glfwCreateWindow(m_width * m_ui_scaling_factor, m_height * m_ui_scaling_factor, m_title.c_str(), nullptr, nullptr);
    if (!m_window) {
        logger::core()->error("Failed to create window");
        throw std::runtime_error("Failed to create window");
    }

    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwSetWindowUserPointer(m_window, this);

    glfwSetWindowContentScaleCallback(
        m_window,
        [](GLFWwindow*, float xscale, float yscale) {
            ImGuiIO& io = ImGui::GetIO();
            io.FontGlobalScale = xscale;
        }
    );

    glfwSetWindowSizeCallback(
        m_window,
        [](GLFWwindow* window, int width, int height)
        {
            auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
            app->onWindowResized(width, height);
        }
    );

    glfwMakeContextCurrent(m_window);
    loadAppIcon();
    glfwSwapInterval(1);

    logger::core()->info("GLFW initialized");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();


    ImGuiStyle& style = ImGui::GetStyle();
    // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.ScaleAllSizes(m_ui_scaling_factor);
    style.FontScaleDpi = m_ui_scaling_factor;

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    logger::core()->info("ImGUI initialized");
    m_base_style = ImGui::GetStyle();

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

void 
mdns::engine::Application::onWindowResized(int width, int height)
{
    m_width  = width;
    m_height = height;

    int fbw, fbh;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
}

void 
mdns::engine::Application::setUIScalingFactor(float newFactor)
{
    newFactor = std::clamp(newFactor, 0.75f, 2.0f);
    if (std::abs(newFactor - m_ui_scaling_factor) < 0.001f) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetStyle() = m_base_style;
    ImGui::GetStyle().ScaleAllSizes(newFactor);
    ImGui::GetStyle().FontScaleDpi = newFactor;

    /*ImFontConfig cfg{};
    cfg.SizePixels = 13.0f * newFactor;
    io.Fonts->Clear();
    io.Fonts->AddFontDefault(&cfg);
    io.Fonts->Build();*/

    m_ui_scaling_factor = newFactor;
    logger::ui()->info("Set UI scaling factor to " + std::to_string(newFactor));
}

void
mdns::engine::Application::loadAppIcon()
{
    int w, h, comp;
    unsigned char* pixels = stbi_load_from_memory(app_icon, app_icon_len, &w, &h, &comp, 4);

    GLFWimage icon;
    icon.width  = w;
    icon.height = h;
    icon.pixels = pixels;

    glfwSetWindowIcon(m_window, 1, &icon);
    stbi_image_free(pixels);
}

void
mdns::engine::Application::handleShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();

    if (io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Equal)) {
            setUIScalingFactor(m_ui_scaling_factor + .25f);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Minus)) {
            setUIScalingFactor(m_ui_scaling_factor - .25f);
        }
    }
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

        handleShortcuts();
        renderUI();

        ImGui::Render();

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

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Zoom In", "Ctrl +")) {
                setUIScalingFactor(0.1f);
            }

            if (ImGui::MenuItem("Zoom Out", "Ctrl -")) {
                setUIScalingFactor(-0.1f);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Open Help")) {
                show_help_window = true;
            }

            if (ImGui::MenuItem("GitHub repo")) {
                openInBrowser("https://github.com/hittsya/mdns-tool");
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (show_help_window)
    {
        ImGui::Begin("Help", &show_help_window, ImGuiWindowFlags_AlwaysAutoResize);

        auto center_text = [](const char* text) {
            float window_width = ImGui::GetWindowSize().x;
            float text_width   = ImGui::CalcTextSize(text).x;
            ImGui::SetCursorPosX((window_width - text_width) * 0.5f);
            ImGui::TextUnformatted(text);
        };

        ImGui::Spacing();
        center_text(m_title.c_str());
        ImGui::Separator();

        ImGui::Spacing();
        center_text("Author: Daniel Manoylo");

        ImGui::Spacing();
        center_text("Special thanks: Maxim Samborsky");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const char* github_url = "https://github.com/hittsya/mdns-tool";
        center_text("Source code:");
        ImGui::Spacing();

        float link_width = ImGui::CalcTextSize(github_url).x;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - link_width) * 0.5f);

        if (ImGui::Selectable(github_url, false)) {
            ImGui::SetClipboardText(github_url);
        }

        ImGui::Spacing();
        ImGui::TextDisabled("(click to copy link)");

        ImGui::End();
    }

    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFrameHeight()));

    ImGui::Indent(10);
    ImGui::TextUnformatted("Welcome to mDNS Scanner BETA");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted("Scan your local network for mDNS / Bonjour services");
    ImGui::PopStyleColor();
    ImGui::Unindent(10);
    ImGui::Separator();
    ImGui::Spacing();


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
    ImGui::SameLine(250);
    ImGui::Text("%s", name.c_str());

    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    ImGui::Text("IP Address(es):");
    ImGui::SameLine(250);
    ImGui::Indent(235);

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

    ImGui::Unindent(235);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));

    ImGui::Text("Port:");
    ImGui::SameLine(250);
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
    for (auto& response : responses) {
        const bool advertised = !response.advertized_ip_addr_str.empty();
        const std::string& ip = advertised ? response.advertized_ip_addr_str : response.ip_addr_str;

        auto process_entry = [&](const std::string& name, uint16_t port) {
            if (name.empty()) {
                return;
            }

            ScanCardEntry entry{};
            entry.ip_addresses = { ip };
            entry.port         = port ? port : response.port;
            entry.name         = name;

            tryAddService(entry, advertised);
        };

        for (const auto& rr : response.answer_rrs) {
            process_entry(rr.rdata_serialized, rr.port);
        }

        for (const auto& rr : response.additional_rrs) {
            process_entry(rr.rdata_serialized, rr.port);
        }

        for (const auto& rr : response.authority_rrs) {
            process_entry(rr.rdata_serialized, rr.port);
        }

        for (const auto& q : response.questions_list) {
            process_entry(q.name, 0);
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
