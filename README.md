# C++17 PERFORMANCE BENCHMARK & CONFIGURATION KIT

This library introduces comportable way to manage program config / real time performance analysis
for any kind of c++ applications.

# Usage

## Options

```c++
#include "perfkit/perfkit.h"

namespace {

PERFKIT_CONFIG_REGISTRY(optd);
auto opt_coeff_a = perfkit::configure(optd, "Global|Amplification", 1.114);
auto opt_coeff_b = perfkit::configure(optd, "Global|Iteration", 31);
auto opt_category = perfkit::configure(optd, "Global|Misc|Category", "Hello");


auto opt_coeff_d = perfkit::configure(
    optd, "Core|ImportantVar", 1.114,
    perfkit::_config_factory<double>{}
        .description("Some important")
        .one_of({1.31, 5.54, 3.82}));

}  // namespace

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
    // there is no overhead on reading options, as any change of config value is guaranteed
    // to be occurred only during invocation of config_registry::apply_update_and_check_if_dirty
    //
    some_function(opt_coeff_a.get(), opt_coeff_b.get(), opt_category.get());

    // values can be reference with various method.
    some_function(*opt_coeff_a, *opt_coeff_b, *opt_category);
    printf(opt_category->c_str());
    some_function(*opt_coeff_a, *opt_coeff_b, (std::string const&)opt_category);
  }

  return 0;
}

void some_function(double, int, std::string const&) {}
```
