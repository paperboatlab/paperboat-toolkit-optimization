// The nlp block: a nonlinear program from a JSON file, solved by Ipopt. A Step block: the path
// arrives at `in`, one solve, the path leaves by `out`. Without Ipopt, `enter` throws a clear
// "needs Ipopt" error.

#include <memory>
#include <string>

#include "paperboat/toolkit/block.hpp"
#include "paperboat/toolkit/optimization.hpp"

namespace paperboat::toolkit::optimization {
namespace {

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

Template nlp_program() {
  Template t;
  t.name = "optimization/nlp";
  t.def.type = "nlp";
  t.def.label = "Solve NLP";
  t.def.kind = Kind::Function;
  t.def.inputs = {{"in", "step"}};
  t.def.outputs = {{"out", "step"}, numbers_port("x"), PortDef{.id = "objective"},
                   PortDef{.id = "iterations"}, PortDef{.id = "status", .type = "text"}};
  t.def.params = {{.id = "file", .type = "file"},
                  {.id = "tol", .default_value = 1e-7, .type = "number"},
                  {.id = "max_iter", .default_value = 500.0, .type = "number"}};
  t.def.description = "Solves min f(x) with g_l <= g(x) <= g_u from a JSON file (Ipopt).";
  t.def.view = {.compact = false, .value = "{file}"};
  t.def.flows = {"step"};
  t.make_step = [](const Values& p, const Config& c) { return std::make_unique<SolveNlpStep>(p, c); };
  return t;
}

}  // namespace

void add_nlp(Registry& registry) { registry.add(nlp_program()); }

}  // namespace paperboat::toolkit::optimization
