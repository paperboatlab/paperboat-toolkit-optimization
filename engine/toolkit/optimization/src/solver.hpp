#pragma once

// Private to the optimization toolkit: a small JSON problem, OSQP, and the block both
// templates use. A file param is made absolute against the descriptor's folder by the core
// (prepare_graph), so a block just opens it.

#include <string>
#include <vector>

#include "paperboat/toolkit/block.hpp"

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

struct Result {
  Vector x;
  double objective = 0.0;
  double iterations = 0.0;
  std::string status;
};

// Read `file` as a QP (`quadratic` true) or an LP. Throws std::runtime_error naming the
// key and the shape that does not fit.
Problem read_problem(const std::string& file, bool quadratic);

// Solve it with OSQP. Never throws on an infeasible or failed solve: the status says so.
Result solve(const Problem& problem, double eps_abs, double eps_rel, int max_iter, bool polish);

// The block both templates use: solve once, on the first step, and keep the result. A new
// run makes a new instance, so a changed file is read again (and a run-once may reuse the
// output through its cache signature).
class SolverBlock final : public Block {
 public:
  SolverBlock(const Values& p, const Config& c, bool quadratic)
      : file_(c.contains("file") ? c.at("file") : std::string{}),
        eps_abs_(get_or(p, "eps_abs", 1e-5)),
        eps_rel_(get_or(p, "eps_rel", 1e-5)),
        max_iter_(static_cast<int>(get_or(p, "max_iter", 4000.0))),
        polish_(get_or(p, "polish", 1.0) != 0.0),
        quadratic_(quadratic) {}

  Values step(const Context&, const Values&) override {
    if (!solved_) {
      result_ = solve(read_problem(file_, quadratic_), eps_abs_, eps_rel_, max_iter_, polish_);
      solved_ = true;
    }
    return {{"x", result_.x},
            {"objective", result_.objective},
            {"iterations", result_.iterations},
            {"status", result_.status}};
  }

 private:
  std::string file_;
  double eps_abs_;
  double eps_rel_;
  int max_iter_;
  bool polish_;
  bool quadratic_;
  bool solved_ = false;
  Result result_;
};

}  // namespace paperboat::toolkit::optimization
