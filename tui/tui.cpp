#include "perfkit/tui.hpp"

#include <unistd.h>

#include <chrono>

#include "imtui/imtui-impl-ncurses.h"
#include "imtui/imtui.h"
#include "spdlog/stopwatch.h"

using namespace perfkit;
using namespace std::literals;
using seconds_ty = std::chrono::duration<double>;

namespace {
ImTui::TScreen *g_screen;

}  // namespace

void tui::init(tui::init_options const &opt) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  g_screen = ImTui_ImplNcurses_Init(opt.support_mouse);
  ImTui_ImplText_Init();
}

void tui::poll_once(bool update_messages) {
  ImTui_ImplNcurses_ProcessEvent();
  ImTui_ImplText_NewFrame();
  auto &io = ImGui::GetIO();

  ImGui::NewFrame();

  ImGui::PushAllowKeyboardFocus(true);
  ImGui::Begin("MainWnd");
  {
    ImGui::Text("Hell, world!");

    float f = 0;
    ImGui::NewLine();
    ImGui::InputFloat("SomVal", &f);
    ImGui::NewLine();
    ImGui::InputFloat("SomVal2", &f);
    ImGui::NewLine();
    ImGui::InputFloat("SomVal3", &f);
    ImGui::NewLine();
    ImGui::InputFloat("SomVal4", &f);
  }
  ImGui::End();

  ImGui::Render();

  ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), g_screen);
  ImTui_ImplNcurses_DrawScreen();
}

static void demoloop() {
  static int nframes = 0;
  static float fval = 0;

  ImTui_ImplNcurses_NewFrame();
  ImTui_ImplText_NewFrame();

  ImGui::NewFrame();

  ImGui::SetNextWindowPos(ImVec2(4, 27), ImGuiCond_Once);
  ImGui::SetNextWindowSize(ImVec2(50.0, 10.0), ImGuiCond_Once);
  ImGui::Begin("Hello, world!");
  ImGui::Text("NFrames = %d", nframes++);
  ImGui::Text("Mouse Pos : x = %g, y = %g", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
  ImGui::Text("Time per frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
  ImGui::Text("Float:");
  ImGui::SameLine();
  ImGui::SliderFloat("##float", &fval, 0.0f, 10.0f);

  ImGui::End();

  ImGui::Render();

  ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), g_screen);
  ImTui_ImplNcurses_DrawScreen();
}

void tui::exec(int interval_ms, int *kill_switch, int message_update_cycle) {
  seconds_ty const interval{interval_ms * 1000.};

  for (int cycles = 0; !kill_switch || !*(volatile int *)kill_switch; ++cycles) {
    spdlog::stopwatch t_elapsed;
    // poll_once((cycles == message_update_cycle) && (cycles = 0, true));
    demoloop();

    auto time_to_wait = static_cast<int>((((interval - t_elapsed.elapsed()).count()) * 1e6));
    if (time_to_wait > 0) {
      usleep(time_to_wait);
    }
  }

  dispose();
}

void tui::register_command(const char *first_keyword, std::function<void(std::string_view)> exec_handler, std::function<void(std::string_view, std::vector<std::string_view> &)> autucomplete_handler) {
}

void tui::tokenize_by_rule(std::string_view, std::vector<std::string_view> &) {
}

void tui::dispose() {
  ImTui_ImplText_Shutdown();
  ImTui_ImplNcurses_Shutdown();
}
