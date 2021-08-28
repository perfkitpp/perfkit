#include "perfkit/perfkit.h"
#include "perfkit/ui.hpp"
#include "range/v3/view.hpp"

using namespace ranges;
using namespace std::literals;

int main(int argc, char** argv) {
  auto args = views::ints(0, argc)
              | views::transform([argv](size_t n) { return std::string_view{argv[n]}; })
              | to_vector;

  auto ui = perfkit::ui::create(perfkit::ui::basic_backend::tui_dashboard, args);

  using clock = std::chrono::steady_clock;
  while (true) {
    auto next_awake = clock ::now() + 16ms;

    ui->poll(true);

    std::this_thread::sleep_until(next_awake);
  }

  perfkit::ui::release(ui);
  return 0;
}
