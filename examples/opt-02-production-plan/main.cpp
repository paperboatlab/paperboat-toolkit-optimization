// opt-02-production-plan: a linear program as one step in a Step flow, with the optional
// optimization toolkit.

#include <paperboat/sdk.hpp>

#include "paperboat/toolkit/optimization.hpp"

int main(int argc, char** argv) {
  static const paperboat::Registry registry = [] {
    paperboat::Registry r = paperboat::builtins();
    paperboat::toolkit::optimization::register_optimization(r);
    return r;
  }();
  static const paperboat::Catalog catalog = paperboat::load_descriptor_file(PAPERBOAT_DESCRIPTOR, registry);
  const paperboat::Backend backend{
      .engine = "opt-02-production-plan",
      .catalog = &catalog,
      .run = {.real_time = false},
  };
  return paperboat::main(backend, argc, argv, 8783);
}
