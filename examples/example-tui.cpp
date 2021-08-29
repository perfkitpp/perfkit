#include "perfkit/perfkit.h"
#include "perfkit/ui.hpp"
#include "range/v3/algorithm.hpp"
#include "range/v3/range.hpp"
#include "range/v3/view.hpp"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"

using namespace ranges;
using namespace std::literals;

static perfkit::tracer rootm{0, "RootMsg"};

PERFKIT_CONFIG_REGISTRY(OPTS);
static auto gg1 = perfkit::configure(OPTS, "MyOpt1", "hell, world!"s).confirm();

int main(int argc, char** argv) {
  spdlog::info("startup");
  printf("press enter to start");
  std::this_thread::sleep_for(0.5s);

  auto args = views::ints(0, argc)
              | views::transform([argv](size_t n) { return std::string_view{argv[n]}; })
              | to_vector;

  auto ui = perfkit::ui::create(perfkit::ui::basic_backend::tui_dashboard, args);

  ui->invoke_command("w somefile.txt");

  using clock = std::chrono::steady_clock;
  while (true) {
    OPTS.apply_update_and_check_if_dirty();
    gg1.async_modify("mvoae");

    auto next_awake = clock ::now() + 16ms;
    if (auto tm_single_loop = rootm.fork("Iteration"); true) {
      tm_single_loop.branch("sample data")   = 100;
      tm_single_loop.branch("sample data 2") = 10.2;

      if (auto tm_inter = tm_single_loop.timer("Iteration"); true) {
      }

      if (auto tm_inter = tm_single_loop.timer("Iteration 2"); true) {
      }

      tm_single_loop.branch("sample data 3") = "sample str";
      tm_single_loop.branch("sample data 4") = false;

      ui->poll(true);
    }
    std::this_thread::sleep_until(next_awake);
  }

  perfkit::ui::release();
  return 0;
}
