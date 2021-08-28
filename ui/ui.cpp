#include "perfkit/ui.hpp"

#include <cassert>
#include <chrono>

#include "ui-tools.hpp"

namespace perfkit::ui {

namespace {
if_ui* g_ui = nullptr;

auto _factory_array() {
  static std::array<detail::cb_if_factory, (size_t)basic_backend::MAX> _factories;
  return &_factories;
}
}  // namespace

if_ui* create(basic_backend b, array_view<std::string_view> argv) {
  if (g_ui) { throw "ONLY SINGLE UI INSTANCE AT THE SAME TIME IS ALLOWED!!!"; }

  return _factory_array()->at(static_cast<size_t>(b))(argv);
}

void release(if_ui* p) {
  delete p;
}

nullptr_t register_function(ui::basic_backend b, ui::if_ui* (*factory)(array_view<std::string_view>)) {
  auto& dest = _factory_array()->at(static_cast<size_t>(b));
  assert(dest == nullptr && "REGISTERED SAME BACKEND!!!");

  dest = factory;
  return nullptr;
}

}  // namespace perfkit::ui