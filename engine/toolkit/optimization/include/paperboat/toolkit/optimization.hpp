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

// A small nonlinear program over named scalar variables:
//
//   min objective(x)   subject to  constraint.lower <= constraint.expression(x) <= constraint.upper
//
// The expressions use the same language as the signal/generator blocks (x^2, sin(x), …). The
// NLP block reads the JSON form; the control NMPC uses the same workspace.
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
