#include <perfkit/configs.h>
#include <perfkit/detail/base.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include "doctest.h"

using namespace std::literals;
PERFKIT_CATEGORY(argparse0) {
  PERFKIT_CONFIGURE(opt0, false)
          .make_flag(false, "opt0", "o", "O")
          .confirm();

  PERFKIT_CONFIGURE(opt1, false)
          .make_flag(false, "1")
          .confirm();

  PERFKIT_CONFIGURE(opt2, false)
          .make_flag(true, "2")
          .confirm();

  PERFKIT_CONFIGURE(opt3, false)
          .make_flag(true, "3")
          .confirm();

  PERFKIT_CONFIGURE(defs, false)
          .make_flag(false, "defs", "invalid", "V")
          .confirm();

  PERFKIT_CONFIGURE(someint, 0)
          .make_flag(false, "S", "someint")
          .confirm();
}

static std::vector<std::string> argset0{
        "--opt0",
        "-NV23",
        "pos1",
        "pos2",
        "-1",
        "-S142",
        "pos3",
};

static std::vector<char*> to_charpp(std::vector<std::string>& s) {
  using namespace ranges;
  return s | views::transform([](auto& str) { return str.data(); }) | to<std::vector<char*>>();
}

TEST_SUITE("Argument Parser") {
  TEST_CASE("Single Flags") {
    auto charpp = to_charpp(argset0);
    int argc    = charpp.size();
    auto argv   = charpp.data();

    perfkit::configs::parse_args(&argc, &argv, true);
    CHECK(argc == 3);
    CHECK(argv[0] == "pos1"sv);
    CHECK(argv[1] == "pos2"sv);
    CHECK(argv[2] == "pos3"sv);
  }
}
