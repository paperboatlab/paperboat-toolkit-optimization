// The qp block: min 1/2 x'Px + q'x with l <= Ax <= u on OSQP. A problem is a JSON file on the block (see
// the public header for the format). The block is a Step block: the path arrives at `in`, one
// solve, the path leaves by `out`. The signal outputs carry the result.

#include "optimization/solve_shared.hpp"

namespace paperboat::toolkit::optimization {

void add_qp(Registry& registry) {
  registry.add(step_program("qp", "Solve QP", true,
                            "Solves min 1/2 x'Px + q'x with l <= Ax <= u from a JSON file (OSQP)."));
}

}  // namespace paperboat::toolkit::optimization
