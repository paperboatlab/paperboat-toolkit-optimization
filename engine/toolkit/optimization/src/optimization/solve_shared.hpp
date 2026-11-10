#pragma once

// Shared by the qp and lp blocks: the Step both solve with, and the template they differ in only
// by type, label and description.

#include <memory>
#include <string>
#include <utility>

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

inline Template step_program(std::string type, std::string label, bool quadratic, std::string description) {
  Template t;
  t.name = "optimization/" + type;
  t.def.type = std::move(type);
  t.def.label = std::move(label);
  t.def.kind = Kind::Function;
  t.def.inputs = {{"in", "step"}};
  t.def.outputs = {{"out", "step"}, numbers_port("x"), PortDef{.id = "objective"},
                   PortDef{.id = "iterations"}, PortDef{.id = "status", .type = "text"}};
  t.def.params = {{.id = "file", .type = "file"},
                  {.id = "eps_abs", .default_value = 1e-5, .type = "number"},
                  {.id = "eps_rel", .default_value = 1e-5, .type = "number"},
                  {.id = "max_iter", .default_value = 4000.0, .type = "number"},
                  {.id = "polish", .default_value = 1.0, .type = "boolean"}};
  t.def.description = std::move(description);
  t.def.view = {.compact = false, .value = "{file}"};
  t.def.flows = {"step"};
  t.make_step = [quadratic](const Values& p, const Config& c) {
    return std::make_unique<SolveStep>(p, c, quadratic);
  };
  return t;
}

}  // namespace paperboat::toolkit::optimization
