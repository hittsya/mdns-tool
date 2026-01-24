#include <Application.h>

#include <imgui.h>
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

mdns::engine::Application::Application(int width, int height, const char* title)
    : m_width(width), m_height(height), m_title(title)
{
    logger::init();
    logger::core()->info("Logger initialized");

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

    m_window = glfwCreateWindow(m_width, m_height, m_title, nullptr, nullptr);
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
        ImGui::Selectable(ipAddr.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
        ImGui::PopID();
    }

    ImGui::Unindent(190);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));

    ImGui::Text("Port:");
    ImGui::SameLine(200);
    ImGui::Text("%d", port);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (ImGui::Button("Open in browser")) {

    }

    ImGui::SameLine();

    if (ImGui::Button("Advanced data")) {

    }

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
mdns::engine::Application::renderDiscoveryLayout()
{
    if (ImGui::CollapsingHeader("Browse mDNS services", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static char searchBuffer[128] = "";
        ImGui::SetNextItemWidth(250);
        ImGui::InputTextWithHint("##ServiceSearch", "Search services...", searchBuffer, IM_ARRAYSIZE(searchBuffer));

        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        
        if (ImGui::Button("Browse")) {
           m_mdns_helper.startBrowse();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);

        if (ImGui::Button("Stop")) {
           m_mdns_helper.stopBrowse();
        }

        renderFoundServices();
    }
}

void 
mdns::engine::Application::onScanDataReady(std::vector<proto::mdns_response>&& responses)
{
    ScanCardEntry entry;

    for (std::size_t entry_idx = 0; entry_idx < responses.size(); ++entry_idx) {
        auto const& target_service = responses[entry_idx];
        entry.ip_addresses = { target_service.ip_addr_str };
        entry.port         = target_service.port;

        for (std::size_t answer_idx = 0; answer_idx < target_service.answer_rrs.size(); ++answer_idx) {
			entry.name = target_service.answer_rrs[answer_idx].rdata_serialized;
            tryAddService(entry);
        }

        for (std::size_t answer_idx = 0; answer_idx < target_service.additional_rrs.size(); ++answer_idx) {
            entry.name = target_service.additional_rrs[answer_idx].rdata_serialized;
            tryAddService(entry);
        }

        for (std::size_t answer_idx = 0; answer_idx < target_service.authority_rrs.size(); ++answer_idx) {
            entry.name = target_service.authority_rrs[answer_idx].rdata_serialized;
            tryAddService(entry);
        }

        for (std::size_t answer_idx = 0; answer_idx < target_service.questions_list.size(); ++answer_idx) {
            entry.name = target_service.questions_list[answer_idx].name;
            tryAddService(entry);
        }
    }
}

void
mdns::engine::Application::tryAddService(ScanCardEntry entry)
{
    auto serviceIt = std::find(m_discovered_services.begin(), m_discovered_services.end(), entry);
    if (serviceIt == m_discovered_services.end()) {
        m_discovered_services.push_back(std::move(entry));
        return;
	}

    auto ipIt = std::find(
        serviceIt->ip_addresses.begin(),
        serviceIt->ip_addresses.end(),
        entry.ip_addresses.front()
	);

	if (ipIt == serviceIt->ip_addresses.end()) {
        serviceIt->ip_addresses.push_back(entry.ip_addresses.front());
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
