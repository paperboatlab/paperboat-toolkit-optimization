# paperboat-toolkit-optimization

The optional optimization toolkit for [Paperboat](https://github.com/paperboatlab/paperboat).

It provides the `optimization` package: linear, quadratic and nonlinear programs on the Flow
page, solved by [OSQP](https://osqp.org) (Apache-2.0) and, optionally,
[Ipopt](https://coin-or.github.io/Ipopt/). The step blocks are `lp`, `qp` and `nlp`, each with
`x`, `objective`, `iterations` and `status` outputs. The path arrives at `in`, the block solves
once, and the path leaves by `out`; the signal outputs carry the result on.

Problems are JSON files on the block, in **MATLAB fmincon's field names**:

```jsonc
// QP:  min 1/2 x'Hx + f'x      LP:  min f'x
{
  "H": [[2, 0], [0, 2]],      // QP only; the upper triangle is used
  "f": [-2, -1],
  "A": [[2, 1], [1, 2]],      // A x <= b
  "b": [8, 8],
  "Aeq": [[1, 1]],            // Aeq x = beq
  "beq": [1],
  "lb": [0, 0],               // lb <= x <= ub; null is infinite
  "ub": [1, 1],
  "x0": [0.5, 0.5]            // accepted, used by the NLP solver
}
```

The nonlinear block takes the full fmincon form:

```jsonc
{
  "fun": "(x(1) - 1)^2 + (x(2) - 2)^2",
  "x0": [0.5, 1.5],
  "Aeq": [[1, 1]], "beq": [2],
  "lb": [0, 0], "ub": [2, 2],
  "nonlcon": {
    "c":   ["x(1)^2 + x(2)^2 - 2"],   // c(x) <= 0
    "ceq": []                          // ceq(x) = 0
  },
  "options": { "tol": 1e-8, "max_iter": 400 }
}
```

Write `x(1)`, `x(2)` (or `x1`, `x2`) in expressions; the language is the same as the
signal/generator blocks (`^`, `sin`, `exp`, …). The older solver-shaped forms
(`P`/`q`/`A`/`l`/`u`, `c`, and `variables`/`objective`/`constraints`) are still read.

A solve block is one step in a procedure (a design point, a plan, a schedule); a new arrival
re-reads the file and solves again. Three examples ship with the toolkit, one per type:
`opt-01-beam-sizing` (QP), `opt-02-production-plan` (LP) and `opt-03-nonlinear-design` (NLP).

The same solver is a C++ workspace for a per-step controller. It keeps the OSQP
factorization when the dimensions and sparsity are unchanged, updates the values, and
warm-starts from the last solution. The control toolkit uses `Workspace::solve` rather than
copying OSQP:

```cpp
#include "paperboat/toolkit/optimization.hpp"

namespace opt = paperboat::toolkit::optimization;

opt::Workspace workspace;
opt::Problem problem = opt::read_problem("cart_pole.dyn.json", true);
auto result = workspace.solve(problem);
// Next step: update problem.q/problem.A and solve again. Same shape: warm start.
```

## Using it

Overlay this repository onto a Paperboat checkout so the toolkit sits at
`engine/toolkit/optimization/` (the engine discovers it automatically from its `toolkit.toml`):

```sh
# from your paperboat checkout
git clone https://github.com/paperboatlab/paperboat-toolkit-optimization /tmp/pb-optimization
cp -r /tmp/pb-optimization/engine/toolkit/optimization engine/toolkit/optimization
```

The toolkit finds a system OSQP (`find_package(osqp)`) or fetches OSQP v1.0.0. Configure
and build the engine as usual:

```sh
cmake -S engine -B engine/build -G Ninja -DPAPERBOAT_BUILD_TESTS=ON
cmake --build engine/build
```

In your backend's `main`, register it next to the builtins:

```cpp
tk::Registry r = tk::builtins();
tk::optimization::register_optimization(r);
```

and import `"optimization"` in your `descriptor.toml`. A backend that lists the toolkit in
its `backend.toml` (`toolkits = ["optimization"]`) is checked for it by `paperboat check`.

## CI

The build job checks out `paperboatlab/paperboat` and runs the engine's tests with this
toolkit overlaid; the validate job checks `toolkit.toml`.
