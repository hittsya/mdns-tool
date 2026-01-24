#include <Application.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stdexcept>
#include <Logger.h>

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

    m_window = glfwCreateWindow(m_width, m_height, m_title, nullptr, nullptr);
    if (!m_window) {
        logger::core()->error("Failed to create window");
        throw std::runtime_error("Failed to create window");
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    logger::core()->info("GLFW initialized");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    logger::core()->info("ImGUI initialized");
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
mdns::engine::Application::renderDiscoveryLayout()
{
    if (ImGui::CollapsingHeader("Browse mDNS services", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static char searchBuffer[128] = "";
        ImGui::SetNextItemWidth(250);
        ImGui::InputTextWithHint("##ServiceSearch", "Search services...", searchBuffer, IM_ARRAYSIZE(searchBuffer));

        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        
        // auto const browsing = m_mdns_helper.browsing();
        if (ImGui::Button("Browse")) {
           m_mdns_helper.startBrowse();
        //    if (result.has_value()) {
        //        m_discovered_services = std::move(result.value());
        //    }
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);

        if (ImGui::Button("Stop")) {
           m_mdns_helper.stopBrowse();
        //    if (result.has_value()) {
        //        m_discovered_services = std::move(result.value());
        //    }
        }

        for (std::size_t entry_idx = 0; entry_idx < m_discovered_services.size(); ++entry_idx) {
            auto const& target_service = m_discovered_services[entry_idx];

            for (std::size_t answer_idx = 0; answer_idx < target_service.answer_rrs.size(); ++answer_idx) {
                auto const target_answer = target_service.answer_rrs[answer_idx];

                ImGui::PushID(entry_idx + answer_idx);
                ImGui::BeginChild("ServiceCard", ImVec2(0, 130), true, ImGuiWindowFlags_NoScrollbar);

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::SetWindowFontScale(1.1f);

                float windowWidth = ImGui::GetContentRegionAvail().x;
                float textWidth   = ImGui::CalcTextSize(target_answer.rdata_serialized.c_str()).x;

                ImGui::SetCursorPosX  ((windowWidth - textWidth) * 0.5f);
                ImGui::TextUnformatted(target_answer.rdata_serialized.c_str());

                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleVar();
                ImGui::Dummy(ImVec2(0.0f, 4.0f));

                ImGui::Text("Hostname:");
                ImGui::SameLine(160);
                ImGui::Text("%s", target_answer.rdata_serialized.c_str());

                ImGui::Text("IP Address:");
                ImGui::SameLine(160);
                ImGui::Text("%s", target_service.ip_addr_str.c_str());

                ImGui::Text("Port:");
                ImGui::SameLine(160);
                ImGui::Text("%d", target_service.port);

                if (ImGui::Button("Open in browser")) {

                }

                ImGui::SameLine();

                if (ImGui::Button("Advanced data")) {

                }

                ImGui::EndChild();
                ImGui::Spacing();
                ImGui::PopID();
            }

        }
    }
}
