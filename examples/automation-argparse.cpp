#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include "doctest.h"
#include "perfkit/configs.h"
#include "perfkit/detail/base.hpp"

using namespace std::literals;
PERFKIT_CATEGORY(argparse0)
{
    PERFKIT_CONFIGURE(opt0, false).transient().flags("opt0", "o", "O").confirm();
    PERFKIT_CONFIGURE(opt1, false).transient().flags("1").confirm();
    PERFKIT_CONFIGURE(opt2, false).flags("2").confirm();
    PERFKIT_CONFIGURE(opt3, false).flags("3").confirm();
    PERFKIT_CONFIGURE(opt4, false).transient().flags("4").confirm();
    PERFKIT_CONFIGURE(defs, false).transient().flags("defs", "invalid", "V").confirm();
    PERFKIT_CONFIGURE(someint, 0).transient().flags("S", "someint").confirm();
    PERFKIT_CONFIGURE(somestr, "polobark").transient().flags("P").confirm();
    PERFKIT_CONFIGURE(somestr2, "polo").transient().flags("polo").confirm();
    PERFKIT_CONFIGURE(somestr3, "polo").transient().flags("vRuka").confirm();
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
        "--vRuka",
        "alexeeeee",
};

static std::vector<char*> to_charpp(std::vector<std::string>& s)
{
    using namespace ranges;
    return s | views::transform([](auto& str) { return str.data(); }) | to<std::vector<char*>>();
}

TEST_SUITE("Argument Parser")
{
    TEST_CASE("Single Flags")
    {
        auto charpp = to_charpp(argset0);
        int argc    = charpp.size();
        auto argv   = charpp.data();

        perfkit::configs::parse_args(&argc, &argv, true);
        argparse0::registry().update();

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

    TEST_CASE("On Invalid Single Flag")
    {
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
