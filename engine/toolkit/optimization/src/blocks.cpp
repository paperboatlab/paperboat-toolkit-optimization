// The optimization package's blocks: a QP and an LP on OSQP. A problem is a JSON file on the
// block (see the public header for the format). The block is a Step block: the path arrives at
// `in`, one solve, the path leaves by `out`. The signal outputs carry the result.

#include <memory>
#include <string>
#include <utility>

#include "paperboat/toolkit/optimization.hpp"
#include "solver.hpp"

namespace paperboat::toolkit::optimization {
namespace {

Template step_program(std::string type, std::string label, bool quadratic, std::string description) {
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

}  // namespace

void register_optimization(Registry& registry) {
  registry.add(step_program("qp", "Solve QP", true,
                            "Solves min 1/2 x'Px + q'x with l <= Ax <= u from a JSON file (OSQP)."));
  registry.add(step_program("lp", "Solve LP", false,
                            "Solves min c'x with l <= Ax <= u from a JSON file (OSQP)."));
}

}  // namespace paperboat::toolkit::optimization
