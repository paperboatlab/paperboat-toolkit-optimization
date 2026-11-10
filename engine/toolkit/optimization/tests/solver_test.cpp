// The public workspace API: solve, update the same pattern, warm start, and read a file.

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>
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
