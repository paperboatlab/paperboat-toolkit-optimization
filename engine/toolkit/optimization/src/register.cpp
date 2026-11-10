// The optimization package: each block is a folder, optimization/<block>/<block>.cpp, defining
// add_<block>. This registers them.

#include "paperboat/toolkit/optimization.hpp"

namespace paperboat::toolkit::optimization {

void add_qp(Registry&);
void add_lp(Registry&);
void add_nlp(Registry&);

void register_optimization(Registry& registry) {
  add_qp(registry);
  add_lp(registry);
  add_nlp(registry);
}

}  // namespace paperboat::toolkit::optimization
