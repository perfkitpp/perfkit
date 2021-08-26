#include "perfkit/perfkit.h"
#include "perfkit/ui.hpp"
using namespace std::literals;

namespace options {
PERFKIT_OPTION_DISPATCHER(gob);
PERFKIT_OPTION test_opt{gob, "+AA|Cat0|Cat1|MyOb", true, "Hello, World"};
PERFKIT_OPTION test_opt2{gob, "+AA|Cat0|Cat2|MyOb", "hell, world"s, "Hello, World 2"};
PERFKIT_OPTION test_opt3{gob, "+AA|Cat0|Cat3|MyOb", 4, "Hello, World 3"};
PERFKIT_OPTION test_opt4{gob, "+AA|Cat0|Cat3|MyOb2", 2.131, "Hello, World 4"};
PERFKIT_OPTION test_opt5{gob, "Cat3", true, "Hello, World 5"};

}  // namespace options

int main(void) {
  perfkit::tui::init({true});

  perfkit::tui::exec(66, nullptr, 1);
  return 0;
}