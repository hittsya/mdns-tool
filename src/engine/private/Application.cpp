#ifdef WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "windows.h"
#include "dwmapi.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#endif

#include <Application.h>
#include <Ping46.h>

#include <algorithm>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <memory>
#include <stdexcept>

#include <view/Dissector.h>
#include <view/Help.h>
#include <view/Ping.h>
#include <view/Questions.h>
#include <view/Services.h>

#include <style/Button.h>
#include <style/Window.h>

#include <images/AppIcon.h>
#include <images/Browser.h>
#include <images/Info.h>
#include <images/Terminal.h>

#include <Util.h>
#include <style/Button.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <Logger.h>

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#endif

static std::string
toLower(std::string s)
{
  std::ranges::transform(
    s, s.begin(), [](unsigned char c) { return std::tolower(c); });

  return s;
}

mdns::engine::Application::Application(int const width,
                                       int const height,
                                       std::string const& buildInfo)
  : m_width(width)
  , m_height(height)
  , m_title("mDNS Scanner " + buildInfo)
{}

mdns::engine::Application::~Application()
{
  if (m_settings) {
    m_settings->saveSettings();
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  logger::core()->info("ImGUI terminated");

  if (m_window) {
    glfwDestroyWindow(m_window);
  }

  glfwTerminate();
  logger::core()->info("GLFW terminated");

  logger::core()->info("Shutting down logger");
  logger::shutdown();
}

bool
mdns::engine::Application::init()
{
  logger::init();
  logger::core()->info("Logger initialized");
  logger::core()->info("Application " + m_title + " is starting");

  if (!glfwInit()) {
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

  try {
    m_settings = std::make_unique<meta::Settings>();
    logger::core()->info("Settings initialized");

    m_ping_tool = std::make_unique<PingTool>();
    logger::core()->info("Ping tool initialized");

    m_mdns_helper = std::make_unique<MdnsHelper>();
    logger::core()->info("MDNS helper initialized");

    m_mdns_helper->connectOnServiceDiscovered(
      [this](std::vector<proto::mdns_response>&& responses) -> void {
        onScanDataReady(std::move(responses));
      });

    m_mdns_helper->connectOnBrowsingStateChanged(
      [this](bool enabled) -> void { m_discovery_running = enabled; });
  } catch (std::bad_alloc const& ex) {
    logger::core()->error("Failed to initialize backend helpers: " +
                          std::string(ex.what()));
    return false;
  }

  float const scale =
    ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
  auto const& settings = m_settings->getSettings();

  m_width = settings.window_width.value_or(static_cast<int>(m_width * scale));
  m_height =
    settings.window_height.value_or(static_cast<int>(m_height * scale));
  m_window =
    glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
  if (!m_window) {
    logger::core()->error("Failed to create window");
    throw std::runtime_error("Failed to create window");
  }

  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
  glfwSetWindowUserPointer(m_window, this);

  glfwSetWindowContentScaleCallback(
    m_window, [](GLFWwindow*, float xscale, float yscale) {
      ImGuiIO& io = ImGui::GetIO();
      io.FontGlobalScale = xscale;
    });

  glfwSetWindowSizeCallback(
    m_window, [](GLFWwindow* window, int width, int height) {
      auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
      app->onWindowResized(width, height);
    });

  glfwMakeContextCurrent(m_window);

#ifdef WIN32
  ImVec4 c = ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg];
  
  auto lift = [](float v) { return powf(v, 1.0f / 1.025f); };

  COLORREF captionColor = RGB((int)(lift(c.x) * 255.0f),
                              (int)(lift(c.y) * 255.0f),
                              (int)(lift(c.z) * 255.0f));

  HWND hwnd = glfwGetWin32Window(m_window);
  DwmSetWindowAttribute(
    hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
  DwmSetWindowAttribute(
    hwnd, DWMWA_BORDER_COLOR, &captionColor, sizeof(captionColor));
#endif

  loadAppIcon();

  loadTexture(&m_logo_texture, app_icon, app_icon_len);
  loadTexture(&m_browser_texture, browser_png, browser_png_len);
  loadTexture(&m_info_texture, info_png, info_png_len);
  loadTexture(&m_terminal_texture, terminal_png, terminal_png_len);

  glfwSwapInterval(1);
  logger::core()->info("Root window initialized");

  ImGui_ImplGlfw_InitForOpenGL(m_window, true);
  ImGui_ImplOpenGL3_Init("#version 330");
  m_base_style = ImGui::GetStyle();
  logger::core()->info("ImGUI initialized");

  setUIScalingFactor(m_settings->getSettings().ui_scale_factor.value_or(1.0f));
  return true;
}

void
mdns::engine::Application::onWindowResized(int width, int height)
{
  m_width = width;
  m_height = height;

  int fbw, fbh;
  glfwGetFramebufferSize(m_window, &fbw, &fbh);
  glViewport(0, 0, fbw, fbh);

  auto& settings = m_settings->getSettings();
  settings.window_height = height;
  settings.window_width = width;
}

void
mdns::engine::Application::setUIScalingFactor(float newFactor) const
{
  newFactor = std::clamp(newFactor, 0.75f, 2.0f);

  ImGui::GetStyle() = m_base_style;
  ImGui::GetStyle().ScaleAllSizes(newFactor);
  ImGui::GetStyle().FontScaleDpi = newFactor;

  logger::ui()->info("Set UI scaling factor to " + std::to_string(newFactor));
  m_settings->getSettings().ui_scale_factor = newFactor;
}

void
mdns::engine::Application::loadTexture(GLuint* dest,
                                       unsigned char data[],
                                       unsigned int length)
{
  logger::core()->info(fmt::format("Loading texture, length: {}", length));

  int w, h, comp;
  unsigned char* data_ = stbi_load_from_memory(data, length, &w, &h, &comp, 4);

  glGenTextures(1, dest);
  glBindTexture(GL_TEXTURE_2D, *dest);
  glTexImage2D(
    GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  stbi_image_free(data_);
}

void
mdns::engine::Application::loadAppIcon() const
{
  int w, h, comp;
  unsigned char* pixels =
    stbi_load_from_memory(app_icon, app_icon_len, &w, &h, &comp, 4);

  GLFWimage icon;
  icon.width = w;
  icon.height = h;
  icon.pixels = pixels;

  glfwSetWindowIcon(m_window, 1, &icon);
  stbi_image_free(pixels);
}

void
mdns::engine::Application::handleShortcuts() const
{
  ImGuiIO const& io = ImGui::GetIO();
  meta::Settings::AppSettings const& settings = m_settings->getSettings();

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
  while (!glfwWindowShouldClose(m_window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    handleShortcuts();
    sortEntries();
    renderUI();

    ImGui::Render();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
  }
}

void
mdns::engine::Application::sortEntries()
{
  std::lock_guard<std::mutex> lock(m_filtered_services_mutex);
  m_filtered_services.clear();

  auto const query = toLower(std::string(m_search_buffer.data()));
  if (query.empty()) {
    m_filtered_services = m_discovered_services;
    return;
  }

  for (auto const& s : m_discovered_services) {
    if (toLower(s.name).find(query) != std::string::npos) {
      m_filtered_services.push_back(s);
    }
  }

  // std::ranges::sort(
  //     m_filtered_services,
  //     [](auto const& a, auto const& b) {
  //         return a.time_of_arrival > b.time_of_arrival;
  //     }
  // );
}

void
mdns::engine::Application::renderUI()
{
  ImGuiViewport* viewport = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::SetNextWindowViewport(viewport->ID);

  constexpr ImGuiWindowFlags windowFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::SetNextWindowBgAlpha(0.0f);
  ImGui::Begin("##FullscreenRoot", nullptr, windowFlags);

  ImDrawList* draw = ImGui::GetBackgroundDrawList(viewport);

  ImU32 bg = IM_COL32(18, 19, 21, 255);
  draw->AddRectFilled(viewport->Pos,
                      ImVec2(viewport->Pos.x + viewport->Size.x,
                             viewport->Pos.y + viewport->Size.y),
                      bg);

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
        mdns::engine::util::openInBrowser(
          "https://github.com/hittsya/mdns-tool");
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
    mdns::engine::ui::renderDissectorWindow(m_dissector_meta_entry,
                                            &m_show_dissector_meta_window);
  }

  if (show_help_window) {
    mdns::engine::ui::renderHelpWindow(&show_help_window, m_title.c_str());
  }

  ImGui::Dummy(ImVec2(0.0f, 18.0f));
  float textH = ImGui::GetTextLineHeight();
  float imgH = textH * 1.25f;
  float offset = (imgH - textH) * 0.5f;

  ImGui::Dummy(ImVec2(0.0f, 2.4f));
  ImGui::BeginGroup();

  if (ImGui::GetWindowSize().x > 950.0f) {
    ImGui::Image(
      static_cast<ImTextureID>(static_cast<intptr_t>(m_logo_texture)),
      ImVec2(imgH, imgH));
    ImGui::SameLine();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
    ImGui::TextUnformatted(m_title.c_str());
  }

  if (ImGui::GetWindowSize().x > 1450.0f) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.5f);
    ImGui::TextUnformatted("| Browse for MDNS or Bonjour services via MDNS-SD");
    ImGui::PopStyleColor();
  }

  ImGui::EndGroup();

  ImGui::SameLine();

  renderDiscoveryLayout();

  ImGui::End();
  ImGui::PopStyleVar(2);
}

void
mdns::engine::Application::renderDiscoveryLayout()
{
  static auto onPingToolClick = [this](std::string const& ip) -> void {
    m_ping_tool->pingIpAddress(ip);
    m_open_ping_view = true;
  };

  static auto onQuestionWindowOpen = [this]() -> void {
    m_open_question_view = true;
  };

  static auto onDissectorClick = [this](ScanCardEntry entry) -> void {
    m_show_dissector_meta_window = true;
    m_dissector_meta_entry = entry;
  };

  static auto onPingStop = [this]() -> void {
    m_ping_tool->stopPing();
    m_open_ping_view = false;
  };

  ImGuiStyle const& style = ImGui::GetStyle();
  const char* label =
    m_discovery_running ? " Stop discovery" : " Browse services";
  float h = ImGui::GetFrameHeight() * 1.25f;
  float textW = ImGui::CalcTextSize(label).x;
  float iconSpace = h;
  float btnWidth = textW + iconSpace + style.FramePadding.x * 4.0f;
  float avail = ImGui::GetContentRegionAvail().x;
  float searchWidth = 350.0f - 16.0f;

  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
  ImGui::PushStyleVar(
    ImGuiStyleVar_FramePadding,
    ImVec2(style.FramePadding.x, style.FramePadding.y * 2.0f));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 1, 1, 0.06f));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1, 1, 1, 0.09f));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1, 1, 1, 0.12f));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.10f));

  ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.5f);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail -
                       (btnWidth + 21.0f + searchWidth));
  ImGui::SetNextItemWidth(searchWidth);

  ImGui::InputTextWithHint("##search",
                           "Filter services",
                           m_search_buffer.data(),
                           m_search_buffer.size());

  ImGui::PopStyleColor(4);
  ImGui::PopStyleVar(2);

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       ImGui::GetContentRegionAvail().x - (btnWidth + 8.0f));

  constexpr auto blue = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
  constexpr auto red = ImVec4(0.85f, 0.45f, 0.45f, 1.0f);
  mdns::engine::ui::pushThemedButtonStyles(m_discovery_running ? red : blue);

  ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);

  if (ImGui::Button(label, ImVec2(btnWidth, h))) {
    if (m_discovery_running) {
      m_mdns_helper->stopBrowse();
    } else {
      m_mdns_helper->startBrowse();
    }
  }

  auto const btnMin = ImGui::GetItemRectMin();
  auto const btnMax = ImGui::GetItemRectMax();
  auto const center = ImVec2(btnMin.x + h * 0.5f, (btnMin.y + btnMax.y) * 0.5f);

  if (!m_discovery_running) {
    mdns::engine::ui::renderPlayTriange(h, center);
  } else {
    mdns::engine::ui::renderLoadingSpinner(h, center);
  }

  mdns::engine::ui::popThemedButtonStyles();

  ImGui::Dummy(ImVec2(0.0f, 0.15f));

  ImGui::BeginGroup();
  ImGui::BeginChild(
    "MainContent", ImVec2(m_open_ping_view ? -810 : 0, 0), false);

  {
    std::lock_guard<std::mutex> lock(m_filtered_services_mutex);

    mdns::engine::ui::renderServiceLayout(m_filtered_services,
                                          onPingToolClick,
                                          onQuestionWindowOpen,
                                          onDissectorClick,
                                          m_browser_texture,
                                          m_info_texture,
                                          m_terminal_texture,
                                          m_mdns_helper->getResolveQueries());
  }

  ImGui::Dummy(ImVec2(0.0f, 3.0f));

  {
    std::lock_guard<std::mutex> lock(m_intercepted_questions_mutex);
    mdns::engine::ui::renderQuestionLayout(m_intercepted_questions);
  }

  if (m_open_question_view) {
    mdns::engine::ui::pushThemedWindowStyles();
    ImGui::Begin("Add custom question",
                 &m_open_question_view,
                 ImGuiWindowFlags_AlwaysAutoResize);
    mdns::engine::ui::popThemedWindowStyles();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(
      ImGuiStyleVar_FramePadding,
      ImVec2(style.FramePadding.x, style.FramePadding.y * 2.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 1, 1, 0.06f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1, 1, 1, 0.09f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1, 1, 1, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.10f));

    static char buffer[128] = { "\0" };
    std::string_view text(buffer);
    bool invalid =
      text.empty() || !text.ends_with(".local") || text.starts_with("_");

    if (invalid) {
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 80, 80, 255));
      ImGui::TextUnformatted("Service name must end with .local, and should "
                             "not start with \"_\" character");
      ImGui::PopStyleColor();
    }

    ImGui::SetNextItemWidth(350.0f);
    ImGui::InputTextWithHint("##serviceAdd", "testservice.local", buffer, 128);

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    ImGui::Dummy(ImVec2(0.0f, 2.5f));

    mdns::engine::ui::pushThemedButtonStyles(ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
    ImGui::BeginDisabled(invalid);

    if (ImGui::Button("Add custom question")) {
      m_mdns_helper->addResolveQuery(std::string(buffer));
      m_open_question_view = false;
      std::ranges::fill(buffer, 0);
    }

    ImGui::EndDisabled();
    mdns::engine::ui::popThemedButtonStyles();

    ImGui::End();
  }

  if (m_open_ping_view) {
    mdns::engine::ui::renderPingTool(
      m_ping_tool->getStats(), m_ping_tool->getOutput(), onPingStop);
  }

  ImGui::EndGroup();
}

void
mdns::engine::Application::onScanDataReady(
  std::vector<proto::mdns_response>&& responses)
{
  for (auto& response : responses) {
    const bool advertised = !response.advertized_ip_addr_str.empty();
    const std::string& ip =
      advertised ? response.advertized_ip_addr_str : response.ip_addr_str;

    auto processEntry =
      [&](const std::string& name,
          uint16_t port,
          proto::mdns_rdata const& rdata,
          std::chrono::steady_clock::time_point const& toa) -> void {
      ScanCardEntry entry{};
      entry.ip_addresses = { ip };
      entry.port = port ? port : response.port;
      entry.name = name;
      entry.time_of_arrival = toa;
      entry.dissector_meta = { rdata };

      tryAddService(entry, advertised);
    };

    for (auto const& rr : response.answer_rrs) {
      processEntry(rr.name, rr.port, rr.rdata, response.time_of_arrival);
    }

    for (auto const& rr : response.additional_rrs) {
      processEntry(rr.name, rr.port, rr.rdata, response.time_of_arrival);
    }

    for (auto const& rr : response.authority_rrs) {
      processEntry(rr.name, rr.port, rr.rdata, response.time_of_arrival);
    }

    std::lock_guard<std::mutex> lock(m_intercepted_questions_mutex);

    for (auto const& q : response.questions_list) {
      QuestionCardEntry entry{};
      entry.ip_addresses = { ip };
      entry.name = q.name;
      entry.time_of_arrival = response.time_of_arrival;

      if (auto it = std::ranges::find(m_intercepted_questions, entry);
          it == m_intercepted_questions.end()) {
        m_intercepted_questions.insert(m_intercepted_questions.begin(),
                                       std::move(entry));

        if (m_intercepted_questions.size() > 15) {
          m_intercepted_questions.pop_back();
        }
      }
    }
  }

  // Special case when at start we received only mDNS pointers and we
  // want to immediately resolve services
  if (!m_discovered_services.empty()) {
    auto const anyServiceResolved = std::ranges::any_of(
      m_discovered_services,
      [](ScanCardEntry const& entry) -> bool { return !entry.name.empty(); });

    if (!anyServiceResolved) {
      m_mdns_helper->scheduleDiscoveryNow();
    }
  }
}

void
mdns::engine::Application::tryAddService(ScanCardEntry entry, bool isAdvertized)
{
  if (auto const& meta = entry.dissector_meta[0];
      std::holds_alternative<proto::mdns_rr_ptr_ext>(meta)) {
    m_mdns_helper->addResolveQuery(
      std::get<proto::mdns_rr_ptr_ext>(meta).target);
  }

  auto const serviceIt = std::ranges::find(m_discovered_services, entry);
  if (serviceIt == m_discovered_services.end()) {
    m_discovered_services.insert(m_discovered_services.begin(),
                                 std::move(entry));
    return;
  }

  auto const exists = std::ranges::any_of(
    serviceIt->dissector_meta, [&](const proto::mdns_rdata& rr) -> bool {
      return rr == entry.dissector_meta.front();
    });

  if (!exists) {
    serviceIt->dissector_meta.insert(serviceIt->dissector_meta.begin(),
                                     entry.dissector_meta.front());
  }

  if (serviceIt->port == mdns::proto::port && serviceIt->port != entry.port) {
    // Handle case where SRV record with port appeared after all records
    serviceIt->port = entry.port;
  }

  if (serviceIt->time_of_arrival != entry.time_of_arrival) {
    serviceIt->time_of_arrival = entry.time_of_arrival;
  }

  if (isAdvertized) {
    // Handle case where anounced service is also advertizing an address.
    // We give priority to advertized IPs.
    serviceIt->ip_addresses = entry.ip_addresses;
    return;
  }

  if (auto ipIt =
        std::ranges::find(serviceIt->ip_addresses, entry.ip_addresses.front());
      ipIt == serviceIt->ip_addresses.end()) {
    serviceIt->ip_addresses.push_back(entry.ip_addresses.front());

    std::ranges::sort(serviceIt->ip_addresses,
                      [](std::string const& a, std::string const& b) -> bool {
                        const bool a4 = a.find(':') == std::string::npos;
                        const bool b4 = b.find(':') == std::string::npos;

                        if (a4 != b4) {
                          return a4;
                        }

                        return a < b;
                      });
  }
}