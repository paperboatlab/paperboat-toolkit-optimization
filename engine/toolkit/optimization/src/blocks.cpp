// The optimization package's blocks: a QP and an LP, both on OSQP. A problem is a JSON
// file on the block (see the public header for the format).

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "paperboat/toolkit/optimization.hpp"
#include "solver.hpp"

namespace paperboat::toolkit::optimization {
namespace {

Template program(std::string type, std::string label, bool quadratic, std::string description) {
  Template t;
  t.name = "optimization/" + type;
  t.def = {.type = std::move(type),
           .label = std::move(label),
           .kind = Kind::Function,
           .inputs = {},
           .outputs = {numbers_port("x"), PortDef{.id = "objective"}, PortDef{.id = "iterations"},
                       PortDef{.id = "status", .type = "text"}},
           .params = {{.id = "file", .type = "file"},
                      {.id = "eps_abs", .default_value = 1e-5, .type = "number"},
                      {.id = "eps_rel", .default_value = 1e-5, .type = "number"},
                      {.id = "max_iter", .default_value = 4000.0, .type = "number"},
                      {.id = "polish", .default_value = 1.0, .type = "boolean"}},
           .description = std::move(description),
           .view = {.compact = false, .value = "{file}"}};
  t.def.cacheable = true;  // a pure function of its file (the run's cache signature tracks it)
  t.make = [quadratic](const Values& p, const Config& c) { return std::make_unique<SolverBlock>(p, c, quadratic); };
  return t;
}

}  // namespace

void register_optimization(Registry& registry) {
  registry.add(program("qp", "Quadratic Program", true,
                       "Solves min 1/2 x'Px + q'x with l <= Ax <= u from a JSON file (OSQP)."));
  registry.add(program("lp", "Linear Program", false,
                       "Solves min c'x with l <= Ax <= u from a JSON file (OSQP)."));
}

}  // namespace paperboat::toolkit::optimization
