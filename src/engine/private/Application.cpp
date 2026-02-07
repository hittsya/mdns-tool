#include <Application.h>
#include <Ping46.h>

#include <imgui.h>
#include <algorithm>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <memory>
#include <stdexcept>

#include <view/Dissector.h>
#include <view/Help.h>
#include <view/Questions.h>
#include <view/Services.h>
#include <view/Ping.h>

#include <Util.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <AppIcon.h>
#include <AppLogo.h>

#include <Logger.h>
#include <ChangeLog.h>

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

    if (!glfwInit())
    {
        logger::core()->error("Failed to initialize GLFW");
        throw std::runtime_error("Failed to initialize GLFW");
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

#if defined(WIN32)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
    logger::core()->info("GLFW initialized");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    m_settings = std::make_unique<meta::Settings>();
    logger::core()->info("Settings initialized");

    float scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    auto &settings = m_settings->getSettings();

    m_width = settings.window_width.value_or(static_cast<int>(m_width * scale));
    m_height = settings.window_height.value_or(static_cast<int>(m_height * scale));

    m_title = "mDNS Scanner " + buildInfo;
    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!m_window)
    {
        logger::core()->error("Failed to create window");
        throw std::runtime_error("Failed to create window");
    }

    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwSetWindowUserPointer(m_window, this);

    glfwSetWindowContentScaleCallback(
        m_window,
        [](GLFWwindow *, float xscale, float yscale)
        {
            ImGuiIO &io = ImGui::GetIO();
            io.FontGlobalScale = xscale;
        });

    glfwSetWindowSizeCallback(
        m_window,
        [](GLFWwindow *window, int width, int height)
        {
            auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
            app->onWindowResized(width, height);
        });

    glfwMakeContextCurrent(m_window);

    loadAppIcon();
    loadAppLogoTexture();

    glfwSwapInterval(1);
    logger::core()->info("Root window initialized");

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    m_base_style = ImGui::GetStyle();
    logger::core()->info("ImGUI initialized");

    setUIScalingFactor(m_settings->getSettings().ui_scale_factor.value_or(1.0f));

    m_mdns_helper.connectOnServiceDiscovered(
        [this](std::vector<proto::mdns_response> &&responses) -> void
        {
            onScanDataReady(std::move(responses));
        });

    m_mdns_helper.connectOnBrowsingStateChanged(
        [this](bool enabled) -> void
        {
            m_discovery_running = enabled;
        });
}

mdns::engine::Application::~Application()
{
    m_settings->saveSettings();

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

void mdns::engine::Application::onWindowResized(int width, int height)
{
    m_width = width;
    m_height = height;

    int fbw, fbh;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);

    auto &settings = m_settings->getSettings();
    settings.window_height = height;
    settings.window_width = width;
}

void mdns::engine::Application::setUIScalingFactor(float newFactor)
{
    newFactor = std::clamp(newFactor, 0.75f, 2.0f);

    ImGuiIO &io = ImGui::GetIO();
    ImGui::GetStyle() = m_base_style;
    ImGui::GetStyle().ScaleAllSizes(newFactor);
    ImGui::GetStyle().FontScaleDpi = newFactor;

    logger::ui()->info("Set UI scaling factor to " + std::to_string(newFactor));
    m_settings->getSettings().ui_scale_factor = newFactor;
}

void mdns::engine::Application::loadAppLogoTexture()
{
    int w, h, comp;
    unsigned char *data = stbi_load_from_memory(app_icon, app_icon_len, &w, &h, &comp, 4);

    glGenTextures(1, &m_logo_texture);
    glBindTexture(GL_TEXTURE_2D, m_logo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
}

void mdns::engine::Application::loadAppIcon()
{
    int w, h, comp;
    unsigned char *pixels = stbi_load_from_memory(app_icon, app_icon_len, &w, &h, &comp, 4);

    GLFWimage icon;
    icon.width = w;
    icon.height = h;
    icon.pixels = pixels;

    glfwSetWindowIcon(m_window, 1, &icon);
    stbi_image_free(pixels);
}

void mdns::engine::Application::handleShortcuts()
{
    ImGuiIO &io = ImGui::GetIO();

    meta::Settings::AppSettings &settings = m_settings->getSettings();
    if (io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Equal))
        {
            auto const scaleFactor = settings.ui_scale_factor.value_or(1.0f);
            setUIScalingFactor(scaleFactor + .25f);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Minus))
        {
            auto const scaleFactor = settings.ui_scale_factor.value_or(1.0f);
            setUIScalingFactor(scaleFactor - .25f);
        }
    }
}

void mdns::engine::Application::run()
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
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }
}

void mdns::engine::Application::renderUI()
{
    ImGuiViewport *viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##FullscreenRoot", nullptr, windowFlags);

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Zoom In", "Ctrl +")) {
                setUIScalingFactor(0.1f);
            }

            if (ImGui::MenuItem("Zoom Out", "Ctrl -")) {
                setUIScalingFactor(-0.1f);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Open Help")) {
                show_help_window = true;
            }

            if (ImGui::MenuItem("GitHub repo")) {
                mdns::engine::util::openInBrowser("https://github.com/hittsya/mdns-tool");
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Changelog")) {
            m_show_changelog_window = true;
        }

        ImGui::EndMainMenuBar();
    }

    if (m_show_changelog_window) {
        mdns::engine::ui::renderChangelogWindow(&m_show_changelog_window);
    }

    if (m_show_dissector_meta_window) {
        mdns::engine::ui::renderDissectorWindow(m_dissector_meta_entry, &m_show_dissector_meta_window);
    }

    if (show_help_window) {
        mdns::engine::ui::renderHelpWindow(&show_help_window, m_title.c_str());
    }

    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFrameHeight()));
    ImGui::Image((ImTextureID)(intptr_t)m_logo_texture, ImVec2(48, 48));
    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::TextUnformatted(m_title.c_str());
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted("This marvelous piece of shit is produced by Daniel M, Maxim S., 30 january 2026, Bratislava");
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    ImGui::Separator();
    ImGui::Spacing();

    renderDiscoveryLayout();

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void mdns::engine::Application::renderDiscoveryLayout()
{
    static auto onPingToolClick = [this](std::string const& ip) -> void {
        m_ping_tool.pingIpAddress(ip);
        m_open_ping_view = true;
    };

    static auto onDissectorClick = [this](ScanCardEntry entry) -> void {
        m_show_dissector_meta_window = true;
        m_dissector_meta_entry       = entry;
    };

    static auto onPingStop = [this]() -> void {
        m_ping_tool.stopPing();
        m_open_ping_view = false;
    };

    /*static char searchBuffer[128] = "";
        ImGui::SetNextItemWidth(250);
        ImGui::InputTextWithHint("##ServiceSearch", "Search services...", searchBuffer, IM_ARRAYSIZE(searchBuffer));*/
    // ImGui::SameLine

    ImGui::SetNextItemWidth(300);

    ImVec2 buttonSize(100, 30);
    ImGui::BeginDisabled(m_discovery_running);
    if (ImGui::Button("Browse", buttonSize)) {
        m_mdns_helper.startBrowse();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(300);

    ImGui::BeginDisabled(!m_discovery_running);
    if (ImGui::Button("Stop", buttonSize)) {
        m_mdns_helper.stopBrowse();
    }
    ImGui::EndDisabled();

    ImGui::Dummy(ImVec2(0.0f, 0.35f));

    ImGui::BeginGroup();
    ImGui::BeginChild("MainContent", ImVec2(m_open_ping_view ? -600 : 0, 0), false);

    mdns::engine::ui::renderServiceLayout (m_discovered_services, onPingToolClick, onDissectorClick);
    mdns::engine::ui::renderQuestionLayout(m_intercepted_questions);

    if (m_open_ping_view) {
        mdns::engine::ui::renderPingTool(m_ping_tool.getStats(), m_ping_tool.getOutput(), onPingStop);
    }

    ImGui::EndGroup();
}

void mdns::engine::Application::onScanDataReady(std::vector<proto::mdns_response> &&responses)
{
    for (auto &response : responses) {
        const bool advertised = !response.advertized_ip_addr_str.empty();
        const std::string &ip = advertised ? response.advertized_ip_addr_str : response.ip_addr_str;

        auto processEntry = [&](const std::string &name, uint16_t port, proto::mdns_rdata const &rdata) {
            ScanCardEntry entry{};
            entry.ip_addresses = {ip};
            entry.port = port ? port : response.port;
            entry.name = name;
            entry.dissector_meta = {rdata};

            tryAddService(entry, advertised);
        };

        for (auto const& rr : response.answer_rrs) {
            processEntry(rr.name, rr.port, rr.rdata);
        }

        for (auto const& rr : response.additional_rrs) {
            processEntry(rr.name, rr.port, rr.rdata);
        }

        for (auto const& rr : response.authority_rrs) {
            processEntry(rr.name, rr.port, rr.rdata);
        }

        for (auto const& q : response.questions_list) {
            QuestionCardEntry entry{};
            entry.ip_addresses = {ip};
            entry.name = q.name;

            auto serviceIt = std::find(m_intercepted_questions.begin(), m_intercepted_questions.end(), entry);
            if (serviceIt == m_intercepted_questions.end()) {
                m_intercepted_questions.insert(m_intercepted_questions.begin(), std::move(entry));
            }
        }
    }
}

void mdns::engine::Application::tryAddService(ScanCardEntry entry, bool isAdvertized)
{
    auto const& meta = entry.dissector_meta[0];
    if (std::holds_alternative<proto::mdns_rr_ptr_ext>(meta)) {
        m_mdns_helper.addResolveQuery(std::get<proto::mdns_rr_ptr_ext>(meta).target);
    }

    auto serviceIt = std::find(m_discovered_services.begin(), m_discovered_services.end(), entry);
    if (serviceIt == m_discovered_services.end()) {
        m_discovered_services.insert(m_discovered_services.begin(), std::move(entry));
        return;
    }

    bool exists = std::any_of(
        serviceIt->dissector_meta.begin(),
        serviceIt->dissector_meta.end(),
        [&](const proto::mdns_rdata &rr) -> bool {
            return rr == entry.dissector_meta.front();
        }
    );

    if (!exists) {
        serviceIt->dissector_meta.insert(serviceIt->dissector_meta.begin(), entry.dissector_meta.front());
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
            [](std::string const &a, std::string const &b) -> bool {
                const bool a4 = a.find(':') == std::string::npos;
                const bool b4 = b.find(':') == std::string::npos;

                if (a4 != b4) {
                    return a4;
                }

                return a < b;
            });
    }
}