#include <Application.h>
#include <Ping46.h>

#include <imgui.h>
#include <algorithm>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <memory>
#include <stdexcept>

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
    logger::core()->info("GLFW initialized");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    m_settings = std::make_unique<meta::Settings>();
    logger::core()->info("Settings initialized");

    float scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    auto& settings = m_settings->getSettings();

    m_width  = settings.window_width.value_or(static_cast<int>(m_width * scale));
    m_height = settings.window_height.value_or(static_cast<int>(m_height * scale));

    m_title  = "mDNS Scanner " + buildInfo;
    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
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
    loadAppLogoTexture();

    glfwSwapInterval(1);
    logger::core()->info("Root window initialized");

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    m_base_style = ImGui::GetStyle();
    logger::core()->info("ImGUI initialized");

    setUIScalingFactor(m_settings->getSettings().ui_scale_factor.value_or(1.0f));

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

void 
mdns::engine::Application::onWindowResized(int width, int height)
{
    m_width  = width;
    m_height = height;

    int fbw, fbh;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);

    auto& settings = m_settings->getSettings();
    settings.window_height = height;
    settings.window_width  = width;
}

void 
mdns::engine::Application::setUIScalingFactor(float newFactor)
{
    newFactor = std::clamp(newFactor, 0.75f, 2.0f);

    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetStyle() = m_base_style;
    ImGui::GetStyle().ScaleAllSizes(newFactor);
    ImGui::GetStyle().FontScaleDpi = newFactor;

    logger::ui()->info("Set UI scaling factor to " + std::to_string(newFactor));
    m_settings->getSettings().ui_scale_factor = newFactor;
}

void
mdns::engine::Application::loadAppLogoTexture()
{
    int w, h, comp;
    unsigned char* data = stbi_load_from_memory(app_icon, app_icon_len, &w, &h, &comp, 4);

    glGenTextures  (1, &m_logo_texture);
    glBindTexture  (GL_TEXTURE_2D, m_logo_texture);
    glTexImage2D   (GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
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

    meta::Settings::AppSettings& settings = m_settings->getSettings();
    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Equal)) {
            auto const scaleFactor = settings.ui_scale_factor.value_or(1.0f);
            setUIScalingFactor(scaleFactor + .25f);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Minus)) {
            auto const scaleFactor = settings.ui_scale_factor.value_or(1.0f);
            setUIScalingFactor(scaleFactor - .25f);
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

        if (ImGui::MenuItem("Changelog")) {
            m_show_changelog_window = true;
        }

        ImGui::EndMainMenuBar();
    }

    if (m_show_changelog_window)
    {
        ImGui::Begin("Changelog", &m_show_changelog_window, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text(meta::CHANGELOG);
        ImGui::End();
    }

    if (m_show_dissector_meta_window)
    {
        auto const card = m_dissector_meta_entry.value_or(ScanCardEntry{"Unknown"});

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 size       = { vp->WorkSize.x * 0.5f, vp->WorkSize.y * 0.5f,};

        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::Begin(("Dissector metadata: " + card.name).c_str(), &m_show_dissector_meta_window, ImGuiWindowFlags_None);

        ImGui::TextDisabled("This card was build using this mDNS records");
        ImGui::Spacing();

        for (auto const& rr : card.dissector_meta) {
            std::visit([&](auto const& entry) {
                using T = std::decay_t<decltype(entry)>;

                ImGui::PushID(&rr);

                if constexpr(std::is_same_v<T, proto::mdns_rr_ptr_ext>) {
                    ImGui::TextColored({0.8f, 0.7f, 1.0f, 1.0f}, "- PTR record");
                    ImGui::Indent();
                    ImGui::Text("Target: %s", entry.target.c_str());
                    ImGui::Unindent();
                }
                else if constexpr(std::is_same_v<T, proto::mdns_rr_txt_ext>) {
                    ImGui::TextColored({0.8f, 0.7f, 1.0f, 1.0f}, "- TXT record");
                    ImGui::Indent();
                    
                    for (auto const& txt : entry.entries) {
                        ImGui::Text("%s", txt.c_str());
                    }

                    ImGui::Unindent();
                }
                else if constexpr(std::is_same_v<T, proto::mdns_rr_srv_ext>) {
                    ImGui::TextColored({0.8f, 0.7f, 1.0f, 1.0f}, "- SRV record");
                    ImGui::Indent();
                    ImGui::Text("Target:   %s", entry.target.c_str());
                    ImGui::Text("Port:     %u", entry.port);
                    ImGui::Text("Priority: %u", entry.priority);
                    ImGui::Text("Weight:   %u", entry.weight);
                    ImGui::Unindent();
                }
                else if constexpr (std::is_same_v<T, proto::mdns_rr_a_ext>) {
                    ImGui::TextColored({0.8f, 0.7f, 1.0f, 1.0f}, "- A record");
                    ImGui::Indent();
                    ImGui::Text("IpV4:     %s", entry.address.c_str());
                    ImGui::Unindent();
                }
                else if constexpr(std::is_same_v<T, proto::mdns_rr_aaaa_ext>) {
                    ImGui::TextColored({0.8f, 0.7f, 1.0f, 1.0f}, "- AAAA record");
                    ImGui::Indent();
                    ImGui::Text("IpV6:     %s", entry.address.c_str());
                    ImGui::Unindent();
                }
                else {
                    ImGui::TextColored({0.8f, 0.7f, 1.0f, 1.0f}, "- UNKNOWN record");
                }

                ImGui::PopID();
                ImGui::Spacing();
            }, rr);
        }

        ImGui::End();
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

void
mdns::engine::Application::renderQuestionCard(int index, std::string const& name, std::string ipAddrs)
{
    if(name.empty()) {
        return;
    }

    ImGuiStyle const& style = ImGui::GetStyle();
    ImGuiIO const& io       = ImGui::GetIO();
    float height = ImGui::GetTextLineHeight() + ImGui::GetFrameHeight() + style.FramePadding.y + 1.0f;

    ImGui::PushID(index);
    ImGui::BeginChild("QuestionCard", ImVec2(0, height ), true, ImGuiWindowFlags_NoScrollbar | ImGuiChildFlags_AutoResizeY);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    float windowWidth = ImGui::GetContentRegionAvail().x;
    float textWidth   = ImGui::CalcTextSize(name.c_str()).x;

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0.0f, 0.5f));

    ImGui::Text("Who has");
    ImGui::SameLine();

    // TODO: Bold text
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);

    draw->AddText(pos, col, name.c_str());
    draw->AddText(ImVec2(pos.x + 1, pos.y), col, name.c_str());

    ImGui::Dummy(ImGui::CalcTextSize(name.c_str()));

    ImGui::SameLine();
    ImGui::Text("? -- via %s", ipAddrs.c_str());

    ImGui::Dummy(ImVec2(0.0f, 1.0f));

    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::PopID();
}


void 
mdns::engine::Application::renderServiceCard(int index, ScanCardEntry const& entry)
{
    if(entry.name.empty()) {
        return;
	}

	auto const height = calcServiceCardHeight(entry.ip_addresses.size());

    ImGui::PushID(index);
    ImGui::BeginChild("ServiceCard", ImVec2(0, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiChildFlags_AutoResizeY);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::SetWindowFontScale(1.1f);

    float windowWidth = ImGui::GetContentRegionAvail().x;
    float textWidth = ImGui::CalcTextSize(entry.name.c_str()).x;

    ImGui::SeparatorText(entry.name.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::Text("Hostname:");
    ImGui::SameLine(250);
    ImGui::Text("%s", entry.name.c_str());

    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    ImGui::Text("IP Address(es):");
    ImGui::SameLine(250);
    ImGui::Indent(235);

    for (auto const& ipAddr: entry.ip_addresses) {
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
                std::string url = entry.name.find("https") != std::string::npos ? "https" : "http";

                url += "://";
                url += ipAddr;

                if (entry.port != mdns::proto::port) {
                    url += ":" + std::to_string(entry.port);
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
    ImGui::Text("%d", entry.port);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (ImGui::Button("Open in browser")) {
        std::string url = entry.name.find("https") != std::string::npos ? "https": "http";
        
        url += "://";
        url += entry.name;
        
        if (entry.port != mdns::proto::port) {
            url += ":" + std::to_string(entry.port);
        }
        
        openInBrowser(url);
    }

    ImGui::SameLine();
    ImVec4 bg        = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
    ImVec4 bg_hover  = ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
    ImVec4 bg_active = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button,        bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  bg_active);
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.05f, 0.05f, 0.05f, 1.0f));

    if (ImGui::Button("Dissector metadata")) {
        m_show_dissector_meta_window = true;
        m_dissector_meta_entry       = entry;
    }

    ImGui::PopStyleColor(4);
    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::PopID();
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
    if (auto result = system(cmd.c_str())) {
        logger::core()->error(fmt::format("Command {} failed with rc {}", cmd, result));
    }
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
    /*static char searchBuffer[128] = "";
        ImGui::SetNextItemWidth(250);
        ImGui::InputTextWithHint("##ServiceSearch", "Search services...", searchBuffer, IM_ARRAYSIZE(searchBuffer));*/
    //ImGui::SameLine
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

    if (ImGui::CollapsingHeader("Browse mDNS services")) {
        ImGui::Dummy(ImVec2(0.0f, 0.25f));
        ImGui::Indent(10);
        ImGui::TextUnformatted("Discovered services");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted("Services that anounced themselfs");
        ImGui::PopStyleColor();
        ImGui::Unindent(10);
        ImGui::Spacing();
        ImGui::Dummy(ImVec2(0.0f, 0.25f));

        for (auto const& service: m_discovered_services) {
            renderServiceCard(static_cast<int>(&service - &m_discovered_services[0]), service);
        }
    }

    if (ImGui::CollapsingHeader("Intercepted questions")) {
        ImGui::Dummy(ImVec2(0.0f, 0.25f));
        ImGui::Indent(10);
        ImGui::TextUnformatted("Intercepted mDNS questions");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted("This questions are used to trigger service anouncement");
        ImGui::PopStyleColor();
        ImGui::Unindent(10);
        ImGui::Spacing();
        ImGui::Dummy(ImVec2(0.0f, 0.25f));

        for (auto const& question: m_intercepted_questions) {
            renderQuestionCard(
                static_cast<int>(&question - &m_intercepted_questions[0]),
                question.name,
                question.ip_addresses[0]
            );
        }
    }

    ImGui::EndChild();
    renderRightSidebarLayout();
    ImGui::EndGroup();
}

void 
mdns::engine::Application::onScanDataReady(std::vector<proto::mdns_response>&& responses)
{
    for (auto& response : responses) {
        const bool advertised = !response.advertized_ip_addr_str.empty();
        const std::string& ip = advertised ? response.advertized_ip_addr_str : response.ip_addr_str;

        auto process_entry = [&](const std::string& name, uint16_t port, proto::mdns_rdata const& rdata) {
            if (name.empty()) {
                return;
            }

            ScanCardEntry entry{};
            entry.ip_addresses   = { ip };
            entry.port           = port ? port : response.port;
            entry.name           = name;
            entry.dissector_meta = { rdata };

            tryAddService(entry, advertised);
        };

        for (const auto& rr : response.answer_rrs) {
            process_entry(rr.rdata_serialized, rr.port, rr.rdata);
        }

        for (const auto& rr : response.additional_rrs) {
            process_entry(rr.rdata_serialized, rr.port, rr.rdata);
        }

        for (const auto& rr : response.authority_rrs) {
            process_entry(rr.rdata_serialized, rr.port, rr.rdata);
        }

        for (const auto& q : response.questions_list) {
            QuestionCardEntry entry{};
            entry.ip_addresses = { ip };
            entry.name         = q.name;

            auto serviceIt = std::find(m_intercepted_questions.begin(), m_intercepted_questions.end(), entry);
            if (serviceIt == m_intercepted_questions.end()) {
                m_intercepted_questions.insert(m_intercepted_questions.begin(), std::move(entry));
                return;
            }
        }
    }
}

void
mdns::engine::Application::tryAddService(ScanCardEntry entry, bool isAdvertized)
{
    auto serviceIt = std::find(m_discovered_services.begin(), m_discovered_services.end(), entry);
    if (serviceIt == m_discovered_services.end()) {
        m_discovered_services.insert(m_discovered_services.begin(), std::move(entry));
        return;
	}

    bool exists = std::any_of(
        serviceIt->dissector_meta.begin(),
        serviceIt->dissector_meta.end(),
        [&](const proto::mdns_rdata& rr) {
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
