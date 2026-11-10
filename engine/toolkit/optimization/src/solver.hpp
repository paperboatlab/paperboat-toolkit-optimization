#pragma once

// Private to the optimization toolkit: the block both templates use. The public problem,
// result and workspace API is in paperboat/toolkit/optimization.hpp.

#include <string>

#include "paperboat/toolkit/block.hpp"
#include "paperboat/toolkit/optimization.hpp"

namespace paperboat::toolkit::optimization {

// A Step block: the path arrives, one solve, the path leaves by `out`. The signal outputs
// carry the result to the rest of the flow.
class SolveStep final : public Step {
 public:
  SolveStep(const Values& p, const Config& c, bool quadratic)
      : file_(c.contains("file") ? c.at("file") : std::string{}),
        quadratic_(quadratic),
        options_{.eps_abs = get_or(p, "eps_abs", 1e-5),
                 .eps_rel = get_or(p, "eps_rel", 1e-5),
                 .max_iter = static_cast<int>(get_or(p, "max_iter", 4000.0)),
                 .polish = get_or(p, "polish", 1.0) != 0.0},
        workspace_(options_) {}

  void enter(const Context&, const Values&) override {
    result_ = workspace_.solve(read_problem(file_, quadratic_));
    solved_ = true;
  }

  std::string tick(const Context&, const Values&) override { return solved_ ? "out" : ""; }

  Values outputs() const override {
    return {{"x", result_.x},
            {"objective", result_.objective},
            {"iterations", result_.iterations},
            {"status", result_.status}};
  }

 private:
  std::string file_;
  bool quadratic_;
  Options options_;
  Workspace workspace_;
  bool solved_ = false;
  Result result_;
};

// The NLP Step block. Without Ipopt, `enter` throws a clear "needs Ipopt" error.
class SolveNlpStep final : public Step {
 public:
  SolveNlpStep(const Values& p, const Config& c)
      : file_(c.contains("file") ? c.at("file") : std::string{}),
        options_{.eps_abs = get_or(p, "tol", 1e-7),
                 .max_iter = static_cast<int>(get_or(p, "max_iter", 500.0))},
        workspace_(options_) {}

  void enter(const Context&, const Values&) override {
    result_ = workspace_.solve(read_nlp_problem(file_));
    solved_ = true;
  }

  std::string tick(const Context&, const Values&) override { return solved_ ? "out" : ""; }

  Values outputs() const override {
    return {{"x", result_.x},
            {"objective", result_.objective},
            {"iterations", result_.iterations},
            {"status", result_.status}};
  }

 private:
  std::string file_;
  Options options_;
  NlpWorkspace workspace_;
  bool solved_ = false;
  Result result_;
};

}  // namespace paperboat::toolkit::optimization
