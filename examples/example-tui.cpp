#include "perfkit/perfkit.h"

namespace {

PERFKIT_OPTION_DISPATCHER(optd);
auto opt_coeff_a  = perfkit::declare_option(optd, "Global|Amplification", 1.114);
auto opt_coeff_b  = perfkit::declare_option(optd, "Global|Iteration", 31);
auto opt_category = perfkit::declare_option(optd, "Global|Misc|Category", "Hello");

auto opt_coeff_d = perfkit::declare_option(
    optd, "Core|ImportantVar", 1.114,
    perfkit::attribute<double>{}
        .description("Some important")
        .one_of({1.31, 5.54, 3.82}));

}  // namespaceg

void some_function(double, int, std::string const&);

int main(void) {
  while (true) {
    // some intensive multi-threaded loop ...
    if (optd.apply_update_and_check_if_dirty()) {
      if (opt_coeff_a.check_dirty_and_consume()) {
        // ... DO SOME REFRESH ...
      }

      if (opt_coeff_b.check_dirty_and_consume()) {
        // ... DO SOME REFRESH ...
      }

      if (opt_category.check_dirty_and_consume()) {
        // ... DO SOME REFRESH ...
      }
    }

    // use options as-is.
    //
    // there is no overhead on reading options, as any change of option value is guaranteed
    // to be occurred only during invocation of option_dispatcher::apply_update_and_check_if_dirty
    //
    some_function(opt_coeff_a.get(), opt_coeff_b.get(), opt_category.get());

    // values can be reference with various method.
    some_function(*opt_coeff_a, *opt_coeff_b, *opt_category);
    puts(opt_category->c_str());
    some_function(*opt_coeff_a, *opt_coeff_b, (std::string const&)opt_category);
  }

  return 0;
}

void some_function(double, int, std::string const&) {}