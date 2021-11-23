#include <perfkit/graphic/gui.hpp>
#include <perfkit/graphic/window.hpp>

void foo()
{
    perfkit::event<int> dd;
    dd.add([](int s) {});
    auto key = dd.add([]() {});

    dd.invoke(3);
    dd.priority(key, perfkit::event_priority::high);
    dd.remove(std::move(key));

    dd += [](int s) {};
}