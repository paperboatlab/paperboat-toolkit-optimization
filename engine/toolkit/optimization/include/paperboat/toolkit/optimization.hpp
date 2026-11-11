#pragma once

// The optimization package: quadratic and linear programs as blocks, solved by OSQP.
//
// A problem is a JSON file on the block (a setting, not a signal). The fields follow MATLAB's
// fmincon, with the objective in matrix form for the convex solvers:
//
//   QP:  { "H": n×n, "f": n, "A": m×n, "b": m, "Aeq": k×n, "beq": k, "lb": n, "ub": n }
//        min 1/2 x'Hx + f'x   s.t. A x <= b, Aeq x = beq, lb <= x <= ub
//   LP:  { "f": n, ... the same constraint fields ... }   min f'x
//
// The older solver-shaped form ({ "P", "q", "A", "l", "u" } or { "c", ... }) is still read.
//
// P uses its upper triangle; a null in l or u is an infinite bound; A, l and u may be
// omitted (an unconstrained problem). The result is `x` (numbers), `objective`, `iterations`
// and a `status` string. Usually wired into a run-once (a design point, a plan, a schedule):
// the block solves on its first step and keeps the answer.
//
// The same solver is available as a C++ workspace for a per-step controller: it keeps the
// OSQP factorization when the dimensions and sparsity are unchanged, updates the values, and
// warm-starts from the last solution. The control toolkit uses this API; it does not copy
// OSQP.
//
// Optional toolkit: the engine discovers this folder from its toolkit.toml when it is
// installed at engine/toolkit/optimization. A backend registers it next to the builtins and
// imports "optimization" in its descriptor:
//
//   tk::Registry r = tk::builtins();
//   tk::optimization::register_optimization(r);

#include <memory>
#include <string>
#include <vector>

#include "paperboat/toolkit/registry.hpp"

namespace paperboat::toolkit::optimization {

using Vector = std::vector<double>;
using Matrix = std::vector<std::vector<double>>;

struct Problem {
  Matrix P;  // empty for an LP
  Vector q;
  Matrix A;
  Vector l;
  Vector u;
  std::size_t variables = 0;
  std::size_t constraints = 0;
};

struct Options {
  double eps_abs = 1e-5;
  double eps_rel = 1e-5;
  int max_iter = 4000;
  bool polish = true;
  bool warm_start = true;
};

struct Result {
  Vector x;
  double objective = 0.0;
  double iterations = 0.0;
  double solve_time = 0.0;
  std::string status;
};

// A nonlinear program in the same fmincon shape:
//
//   { "fun": "...", "x0": n, "A": m×n, "b": m, "Aeq": k×n, "beq": k,
//     "lb": n, "ub": n, "nonlcon": { "c": ["..."], "ceq": ["..."] },
//     "options": { "tol": 1e-8, "max_iter": 500 } }
//
//   min fun(x)  s.t.  A x <= b, Aeq x = beq, lb <= x <= ub,
//                     c(x) <= 0, ceq(x) = 0
//
// Write `x(1)`, `x(2)` in expressions (or `x1`, `x2`); the expressions use the same language as
// the signal/generator blocks. The older { "variables", "objective", "constraints" } form is
// still read for the NMPC path.
struct NlpVariable {
  std::string name;
  double initial = 0.0;
  double lower = -1e20;
  double upper = 1e20;
};

struct NlpConstraint {
  std::string expression;
  double lower = -1e20;
  double upper = 1e20;
};

struct NlpProblem {
  std::vector<NlpVariable> variables;
  std::string objective;
  std::vector<NlpConstraint> constraints;
  /// From the file's `options`; 0 means "use the workspace's options".
  double tol = 0.0;
  int max_iter = 0;
};

// Read `file` as an NlpProblem. Throws std::runtime_error naming the key/name that does not fit.
NlpProblem read_nlp_problem(const std::string& file);

// True when the toolkit was built with Ipopt. Without it, the NLP workspace and block report a
// clear "needs Ipopt" error; the rest of the toolkit still works.
bool has_nlp();

// A reusable NLP workspace (Ipopt). It rebuilds the Ipopt problem for each solve; warm starts
// are passed through `x0`/the last solution.
class NlpWorkspace {
 public:
  explicit NlpWorkspace(Options options = {});
  ~NlpWorkspace();
  NlpWorkspace(const NlpWorkspace&) = delete;
  NlpWorkspace& operator=(const NlpWorkspace&) = delete;

  Result solve(const NlpProblem& problem, const Vector& x0 = {});
  Result solve(const std::string& file, const Vector& x0 = {});

  const Options& options() const;
  void set_options(const Options& options);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Read `file` as a QP (`quadratic` true) or an LP. Throws std::runtime_error naming the
// key and the shape that does not fit.
Problem read_problem(const std::string& file, bool quadratic);

// A reusable OSQP workspace. Same dimensions and sparsity: update the values, keep the
// factorization, warm-start from `x0` or the last solution. A changed shape rebuilds it.
class Workspace {
 public:
  explicit Workspace(Options options = {});
  ~Workspace();
  Workspace(const Workspace&) = delete;
  Workspace& operator=(const Workspace&) = delete;
  Workspace(Workspace&&) noexcept;
  Workspace& operator=(Workspace&&) noexcept;

  // Solve `problem`. Never throws on an infeasible or failed solve: the status says so.
  Result solve(const Problem& problem, const Vector& x0 = {});
  Result solve(const std::string& file, bool quadratic, const Vector& x0 = {});

  // Forget the warm start and the factorization; the next solve builds a fresh solver.
  void restart();

  const Options& options() const;
  void set_options(const Options& options);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// One-shot convenience: a fresh workspace.
Result solve(const Problem& problem, const Options& options = {}, const Vector& x0 = {});

void register_optimization(Registry& registry);

}  // namespace paperboat::toolkit::optimization
