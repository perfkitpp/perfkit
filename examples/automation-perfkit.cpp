#define DOCTEST_CONFIG_NO_EXCEPTIONS
#include "doctest.h"
#include "perfkit/perfkit.h"
#include "perfkit/ui.hpp"
#include "range/v3/view.hpp"

using namespace std::literals;
using namespace ranges;

using views::zip;

PERFKIT_OPTION_DISPATCHER(g_opts);
auto has_value   = perfkit::option_factory(g_opts, "Has Value", 100).make();
auto name_test_1 = perfkit::option_factory(g_opts, "-|   AA  | BB   |  +641    | CC", 100).make();
auto name_test_2 = perfkit::option_factory(g_opts, "+561|-|    A A   | + 31411 |   B    B |  +641   |  C C", 100).make();

PERFKIT_MESSAGE_BLOCK my_block{0, "hell, world"};
PERFKIT_MESSAGE_BLOCK another_block{0, "hell, world2"};

TEST_SUITE_BEGIN("Basic Compilation Test");
TEST_CASE("Create Options") {
  CHECK(*has_value == 100);
  g_opts.queue_update_value("Has Value", 200);
  CHECK(*has_value == 100);
  CHECK(g_opts.apply_update_and_check_if_dirty());
  CHECK(has_value.check_dirty_and_consume());
  CHECK(*has_value == 200);
  CHECK(!has_value.check_dirty_and_consume());

  CHECK(name_test_1.base().display_key() == "-|AA|BB|CC");
  CHECK(name_test_2.base().display_key() == "-|A A|B    B|C C");
};

TEST_CASE("Create Message Blocks") {
  for (int i = 0; i < 5; ++i) {
    auto fut  = my_block.async_fetch_request();
    auto TM_0 = my_block.fork("Foo");

    auto DT_0_0 = TM_0.branch("My");
    if (i > 0) { CHECK(14 == DT_0_0.data_as<int64_t>()); }
    DT_0_0 = 14;

    auto tm = TM_0.timer("TT");
    CHECK((i > 1) == !!tm);

    CHECK(fut.wait_for(0s) == std::future_status::ready);
    auto value      = fut.get();
    auto [lck, ptr] = value.acquire();

    for (auto& elem : *ptr) {
      elem.subscribe(true);
    }
  }
}

TEST_CASE("Tokenize") {
  std::vector<std::string> tokens;
  perfkit::cmdutils::tokenize_by_argv_rule(
      "a alpha veta \"and there will \\\"be\\\' light\"  mo\\'ve\\ space\\ over", tokens, nullptr);

  std::vector<std::string> compared(
      {"a", "alpha", "veta", "and there will \\\"be\\\' light", "mo\\'ve space over"});

  for (auto it : zip(tokens, compared)) {
    auto& gen = it.first;
    auto& src = it.second;
    CHECK(src == gen);
  }
}

TEST_CASE("Sort Messages") {
}

TEST_SUITE_END();
