#include <perfkit/configs.h>
#include <perfkit/detail/base.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include "doctest.h"

using namespace std::literals;
PERFKIT_CATEGORY(argparse0) {
  PERFKIT_CONFIGURE(opt0, false).make_flag(false, "opt0", "o", "O").confirm();
  PERFKIT_CONFIGURE(opt1, false).make_flag(false, "1").confirm();
  PERFKIT_CONFIGURE(opt2, false).make_flag(true, "2").confirm();
  PERFKIT_CONFIGURE(opt3, false).make_flag(true, "3").confirm();
  PERFKIT_CONFIGURE(opt4, false).make_flag(false, "4").confirm();
  PERFKIT_CONFIGURE(defs, false).make_flag(false, "defs", "invalid", "V").confirm();
  PERFKIT_CONFIGURE(someint, 0).make_flag(false, "S", "someint").confirm();
  PERFKIT_CONFIGURE(somestr, "polobark").make_flag(false, "P").confirm();
  PERFKIT_CONFIGURE(somestr2, "polo").make_flag(false, "polo").confirm();
  PERFKIT_CONFIGURE(somestr3, "polo").make_flag(false, "vRuka").confirm();
}

static std::vector<std::string> argset0{
        "--opt0",
        "-NV23",
        "pos1",
        "pos2",
        "-14",
        "-S142",
        "pos3",
        "-P polu polu aa",
        "--polo=movdla kkwq nkkso",
        "--vRuka", "alexeeeee"};

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
    argparse0::registry().apply_update_and_check_if_dirty();

    CHECK(argc == 3);
    CHECK(argv[0] == "pos1"sv);
    CHECK(argv[1] == "pos2"sv);
    CHECK(argv[2] == "pos3"sv);

    CHECK(*argparse0::opt0 == true);
    CHECK(*argparse0::opt1 == true);
    CHECK(*argparse0::opt2 == false);
    CHECK(*argparse0::opt3 == false);
    CHECK(*argparse0::opt4 == true);
    CHECK(*argparse0::defs == false);
    CHECK(*argparse0::someint == 142);
    CHECK(*argparse0::somestr == " polu polu aa");
    CHECK(*argparse0::somestr2 == "movdla kkwq nkkso");
    CHECK(*argparse0::somestr3 == "alexeeeee");
  }

  TEST_CASE("On Invalid Single Flag") {
    std::vector<std::string> argset{
            "--opt0",
            "-NV23S",
            "pos1",
            "pos2",
            "-14",
            "-S142",
            "pos3",
    };

    auto charpp = to_charpp(argset);
    int argc    = charpp.size();
    auto argv   = charpp.data();

    REQUIRE_THROWS_AS(perfkit::configs::parse_args(&argc, &argv, true),
                      perfkit::configs::parse_error&);
  }
}
