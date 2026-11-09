#pragma once

// The optimization package: quadratic and linear programs as blocks, solved by OSQP.
//
// A problem is a JSON file on the block (a setting, not a signal):
//
//   QP:  { "P": n×n, "q": n, "A": m×n, "l": m, "u": m }   min 1/2 x'Px + q'x, l <= Ax <= u
//   LP:  { "c": n, "A": m×n, "l": m, "u": m }             min c'x,          l <= Ax <= u
//
// P uses its upper triangle; a null in l or u is an infinite bound; A, l and u may be
// omitted (an unconstrained problem). The result is `x` (numbers), `objective`, `iterations`
// and a `status` string. Usually wired into a run-once (a design point, a plan, a schedule):
// the block solves on its first step and keeps the answer.
//
// Optional toolkit: the engine discovers this folder from its toolkit.toml when it is
// installed at engine/toolkit/optimization. A backend registers it next to the builtins and
// imports "optimization" in its descriptor:
//
//   tk::Registry r = tk::builtins();
//   tk::optimization::register_optimization(r);

#include "paperboat/toolkit/registry.hpp"

namespace paperboat::toolkit::optimization {

void register_optimization(Registry& registry);

}  // namespace paperboat::toolkit::optimization
