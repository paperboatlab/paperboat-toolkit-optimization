// The public workspace API: solve, update the same pattern, warm start, and read a file.

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>

#include "paperboat/toolkit/descriptor.hpp"
#include "paperboat/toolkit/optimization.hpp"

namespace opt = paperboat::toolkit::optimization;
namespace tk = paperboat::toolkit;

namespace {

opt::Problem qp() {
  opt::Problem p;
  p.P = {{2.0, 0.0}, {0.0, 2.0}};
  p.q = {-2.0, -1.0};
  p.A = {{1.0, 1.0}};
  p.l = {1.0};
  p.u = {1.0};
  p.variables = 2;
  p.constraints = 1;
  return p;
}

}  // namespace

TEST_CASE("a QP solves, and the workspace updates the same pattern") {
  opt::Workspace workspace;
  auto result = workspace.solve(qp());
  CHECK(result.status.find("solved") != std::string::npos);
  REQUIRE(result.x.size() == 2);
  CHECK(std::abs(result.x[0] - 0.75) < 1e-4);
  CHECK(std::abs(result.x[1] - 0.25) < 1e-4);
  CHECK(result.iterations > 0.0);
  CHECK(result.solve_time >= 0.0);

  // Same dimensions and sparsity: update q, warm start from the last x.
  opt::Problem next = qp();
  next.q = {-4.0, -2.0};
  auto again = workspace.solve(next);
  CHECK(again.status.find("solved") != std::string::npos);
  REQUIRE(again.x.size() == 2);
  CHECK(std::abs(again.x[0] - 1.0) < 1e-4);
  CHECK(std::abs(again.x[1]) < 1e-4);
}

TEST_CASE("the workspace reads a JSON problem file") {
  const auto path = std::filesystem::temp_directory_path() / "paperboat_optimization_test.qp.json";
  {
    std::ofstream out(path);
    out << R"({"P": [[2, 0], [0, 2]], "q": [-2, -1], "A": [[1, 1]], "l": [1], "u": [1]})";
  }
  opt::Workspace workspace;
  const auto result = workspace.solve(path.string(), true);
  CHECK(result.status.find("solved") != std::string::npos);
  REQUIRE(result.x.size() == 2);
  std::filesystem::remove(path);
}

TEST_CASE("the package registers Step blocks") {
  tk::Registry registry = tk::builtins();
  opt::register_optimization(registry);
  const auto catalog = tk::load_descriptor(R"(
domain = "opt"
import = ["optimization"]
[graph]
nodes = []
edges = []
)", registry);
  const auto message = nlohmann::json::parse(tk::catalog_message(catalog));
  const auto qp = std::ranges::find_if(message.at("blocks"), [](const nlohmann::json& b) {
    return b.value("type", "") == "qp";
  });
  REQUIRE(qp != message.at("blocks").end());
  CHECK(qp->at("flows") == nlohmann::json{"step"});
  CHECK(qp->at("inputs").at(0).at("type") == "step");
  CHECK(qp->at("outputs").at(0).at("type") == "step");
}

TEST_CASE("an NLP problem reads from JSON") {
  const auto path = std::filesystem::temp_directory_path() / "paperboat_optimization_test.nlp.json";
  {
    std::ofstream out(path);
    out << R"({
      "variables": [
        {"name": "x", "initial": 0, "lower": -5, "upper": 5},
        {"name": "y", "initial": 0, "lower": -5, "upper": 5}
      ],
      "objective": "x^2 + y^2",
      "constraints": [{"expression": "x + y - 1", "lower": 0, "upper": 0}]
    })";
  }
  const auto problem = opt::read_nlp_problem(path.string());
  REQUIRE(problem.variables.size() == 2);
  CHECK(problem.variables[0].name == "x");
  CHECK(problem.objective == "x^2 + y^2");
  REQUIRE(problem.constraints.size() == 1);
  CHECK(problem.constraints[0].lower == 0.0);
  CHECK(problem.constraints[0].upper == 0.0);
  std::filesystem::remove(path);
}

TEST_CASE("the NLP workspace solves, or says it needs Ipopt") {
  opt::NlpProblem problem;
  problem.variables = {{.name = "x", .initial = 0.0, .lower = -5.0, .upper = 5.0},
                       {.name = "y", .initial = 0.0, .lower = -5.0, .upper = 5.0}};
  problem.objective = "x^2 + y^2";
  problem.constraints = {{.expression = "x + y - 1", .lower = 0.0, .upper = 0.0}};

  opt::NlpWorkspace workspace;
  if (!opt::has_nlp()) {
    CHECK_THROWS_WITH(workspace.solve(problem), Catch::Matchers::ContainsSubstring("needs Ipopt"));
    return;
  }
  const auto result = workspace.solve(problem);
  CHECK(result.status.find("solved") != std::string::npos);
  REQUIRE(result.x.size() == 2);
  CHECK(std::abs(result.x[0] - 0.5) < 1e-4);
  CHECK(std::abs(result.x[1] - 0.5) < 1e-4);
}

TEST_CASE("fmincon fields read into an OSQP QP") {
  const auto path = std::filesystem::temp_directory_path() / "paperboat_optimization_fmincon.qp.json";
  {
    std::ofstream out(path);
    out << R"({"H": [[2, 0], [0, 2]], "f": [-2, -1],
               "Aeq": [[1, 1]], "beq": [1],
               "lb": [0, 0], "ub": [1, 1]})";
  }
  opt::Workspace workspace;
  const auto result = workspace.solve(path.string(), true);
  CHECK(result.status.find("solved") != std::string::npos);
  REQUIRE(result.x.size() == 2);
  CHECK(std::abs(result.x[0] - 0.75) < 1e-4);
  CHECK(std::abs(result.x[1] - 0.25) < 1e-4);
  std::filesystem::remove(path);
}

TEST_CASE("fmincon fields read into an LP") {
  const auto path = std::filesystem::temp_directory_path() / "paperboat_optimization_fmincon.lp.json";
  {
    std::ofstream out(path);
    out << R"({"f": [-3, -2], "A": [[2, 1], [1, 2], [1, 1]], "b": [8, 8, 5], "lb": [0, 0]})";
  }
  const opt::Problem problem = opt::read_problem(path.string(), false);
  REQUIRE(problem.variables == 2);
  REQUIRE(problem.constraints == 5);  // three inequalities + two bound rows
  const auto result = opt::solve(problem);
  CHECK(result.status.find("solved") != std::string::npos);
  REQUIRE(result.x.size() == 2);
  CHECK(std::abs(result.x[0] - 3.0) < 1e-4);
  CHECK(std::abs(result.x[1] - 2.0) < 1e-4);
  CHECK(std::abs(result.objective + 13.0) < 1e-4);
  std::filesystem::remove(path);
}

TEST_CASE("fmincon fields read into an NLP, with x(1) translated") {
  const auto path = std::filesystem::temp_directory_path() / "paperboat_optimization_fmincon.nlp.json";
  {
    std::ofstream out(path);
    out << R"({
      "fun": "(x(1) - 1)^2 + (x(2) - 2)^2",
      "x0": [0.5, 1.5],
      "Aeq": [[1, 1]], "beq": [2],
      "lb": [0, 0], "ub": [2, 2],
      "nonlcon": { "c": ["x(1)^2 + x(2)^2 - 2"] },
      "options": { "tol": 1e-8, "max_iter": 400 }
    })";
  }
  const opt::NlpProblem problem = opt::read_nlp_problem(path.string());
  REQUIRE(problem.variables.size() == 2);
  CHECK(problem.variables[0].name == "x1");
  CHECK(problem.objective == "(x1 - 1)^2 + (x2 - 2)^2");
  REQUIRE(problem.constraints.size() == 2);  // Aeq x = beq, then c(x) <= 0
  CHECK(problem.constraints[0].lower == 0.0);
  CHECK(problem.constraints[0].upper == 0.0);
  CHECK(problem.constraints[1].upper == 0.0);
  CHECK(problem.tol == 1e-8);
  CHECK(problem.max_iter == 400);

  opt::NlpWorkspace workspace;
  if (!opt::has_nlp()) {
    CHECK_THROWS_WITH(workspace.solve(problem), Catch::Matchers::ContainsSubstring("needs Ipopt"));
    std::filesystem::remove(path);
    return;
  }
  const auto result = workspace.solve(problem);
  CHECK(result.status.find("solved") != std::string::npos);
  REQUIRE(result.x.size() == 2);
  CHECK(std::abs(result.x[0] - 1.0) < 1e-4);
  CHECK(std::abs(result.x[1] - 1.0) < 1e-4);
  CHECK(std::abs(result.objective - 1.0) < 1e-4);
  std::filesystem::remove(path);
}

TEST_CASE("fmincon x(1) translation leaves max(...) alone") {
  const auto path = std::filesystem::temp_directory_path() / "paperboat_optimization_fmincon.names.json";
  {
    std::ofstream out(path);
    out << R"json({"fun": "max(x(1), 0) + x(2)", "x0": [0, 0]})json";
  }
  const opt::NlpProblem problem = opt::read_nlp_problem(path.string());
  CHECK(problem.objective == "max(x1, 0) + x2");
  std::filesystem::remove(path);
}
