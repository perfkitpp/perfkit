#include <perfkit/gui/imgui.hpp>
#include <perfkit/gui/window.hpp>

void foo() {
  perfkit::delegate<int> dd;
  dd.add([](int s) {});
  auto key = dd.add([]() {});

  dd.invoke(3);
  dd.priority(key, perfkit::delegate_priority::high);
  dd.remove(std::move(key));

  dd += [](int s) {};
}