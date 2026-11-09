// Reading a problem file and running OSQP.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <osqp.h>

#include "solver.hpp"

namespace paperboat::toolkit::optimization {
namespace {

Matrix rows_of(const nlohmann::json& doc, const char* key, const std::string& at) {
  if (!doc.contains(key)) return {};
  if (!doc[key].is_array()) throw std::runtime_error(at + ": \"" + key + "\" must be an array of rows");
  for (const auto& row : doc[key])
    if (!row.is_array()) throw std::runtime_error(at + ": \"" + key + "\" must be an array of rows");
  return doc[key].get<Matrix>();
}

Vector vector_of(const nlohmann::json& doc, const char* key, const std::string& at) {
  if (!doc.contains(key)) return {};
  if (!doc[key].is_array()) throw std::runtime_error(at + ": \"" + key + "\" must be an array");
  Vector out;
  out.reserve(doc[key].size());
  for (const auto& item : doc[key]) {
    if (item.is_null()) {  // an infinite bound, sign chosen by the caller
      out.push_back(std::numeric_limits<double>::signaling_NaN());
    } else if (item.is_number()) {
      out.push_back(item.get<double>());
    } else {
      throw std::runtime_error(at + ": \"" + key + "\" must hold numbers or null");
    }
  }
  return out;
}

// One CSC column-major matrix. P keeps its upper triangle (OSQP's convention); A is full.
struct Csc {
  std::vector<OSQPFloat> x;
  std::vector<OSQPInt> i;
  std::vector<OSQPInt> p;
  OSQPInt rows = 0;
  OSQPInt cols = 0;

  OSQPCscMatrix matrix() {
    OSQPCscMatrix m{};
    m.m = rows;
    m.n = cols;
    m.p = p.data();
    m.i = i.data();
    m.x = x.data();
    m.nzmax = static_cast<OSQPInt>(x.size());
    m.nz = -1;
    m.owned = 0;
    return m;
  }
};

Csc upper_csc(const Matrix& matrix) {
  Csc out;
  out.rows = static_cast<OSQPInt>(matrix.size());
  out.cols = out.rows;
  out.p.push_back(0);
  for (std::size_t j = 0; j < matrix.size(); ++j) {
    for (std::size_t i = 0; i <= j; ++i) {
      const double value = matrix[i][j];
      if (value == 0.0) continue;
      out.i.push_back(static_cast<OSQPInt>(i));
      out.x.push_back(static_cast<OSQPFloat>(value));
    }
    out.p.push_back(static_cast<OSQPInt>(out.x.size()));
  }
  return out;
}

Csc full_csc(const Matrix& matrix, std::size_t columns) {
  Csc out;
  out.rows = static_cast<OSQPInt>(matrix.size());
  out.cols = static_cast<OSQPInt>(columns);
  out.p.push_back(0);
  for (std::size_t j = 0; j < columns; ++j) {
    for (std::size_t i = 0; i < matrix.size(); ++i) {
      const double value = matrix[i][j];
      if (value == 0.0) continue;
      out.i.push_back(static_cast<OSQPInt>(i));
      out.x.push_back(static_cast<OSQPFloat>(value));
    }
    out.p.push_back(static_cast<OSQPInt>(out.x.size()));
  }
  return out;
}

}  // namespace

Problem read_problem(const std::string& file, bool quadratic) {
  if (file.empty()) throw std::runtime_error("needs a file: a JSON problem with P/q or c, A, l and u");
  std::ifstream in(file);
  if (!in) throw std::runtime_error("cannot open " + file);
  nlohmann::json doc;
  try {
    in >> doc;
  } catch (const nlohmann::json::exception& err) {
    throw std::runtime_error(file + ": " + err.what());
  }
  if (!doc.is_object()) throw std::runtime_error(file + ": the problem must be a JSON object");

  Problem problem;
  if (quadratic) {
    problem.P = rows_of(doc, "P", file);
    problem.q = vector_of(doc, "q", file);
    if (problem.P.empty()) throw std::runtime_error(file + ": a QP needs \"P\" (n rows)");
    if (problem.q.empty()) throw std::runtime_error(file + ": a QP needs \"q\" (n numbers)");
    problem.variables = problem.q.size();
    if (problem.P.size() != problem.variables)
      throw std::runtime_error(file + ": \"P\" has " + std::to_string(problem.P.size()) + " rows, expected " +
                               std::to_string(problem.variables));
    for (const auto& row : problem.P)
      if (row.size() != problem.variables)
        throw std::runtime_error(file + ": every row of \"P\" needs " + std::to_string(problem.variables) + " numbers");
  } else {
    problem.q = vector_of(doc, "c", file);
    if (problem.q.empty()) problem.q = vector_of(doc, "q", file);
    if (problem.q.empty()) throw std::runtime_error(file + ": an LP needs \"c\" (n numbers)");
    problem.variables = problem.q.size();
  }

  problem.A = rows_of(doc, "A", file);
  problem.constraints = problem.A.size();
  if (!problem.A.empty())
    for (const auto& row : problem.A)
      if (row.size() != problem.variables)
        throw std::runtime_error(file + ": every row of \"A\" needs " + std::to_string(problem.variables) + " numbers");

  problem.l = vector_of(doc, "l", file);
  problem.u = vector_of(doc, "u", file);
  if (!problem.l.empty() && problem.l.size() != problem.constraints)
    throw std::runtime_error(file + ": \"l\" needs " + std::to_string(problem.constraints) + " numbers");
  if (!problem.u.empty() && problem.u.size() != problem.constraints)
    throw std::runtime_error(file + ": \"u\" needs " + std::to_string(problem.constraints) + " numbers");
  problem.l.resize(problem.constraints, -OSQP_INFTY);
  problem.u.resize(problem.constraints, OSQP_INFTY);
  for (std::size_t i = 0; i < problem.constraints; ++i) {
    if (std::isnan(problem.l[i])) problem.l[i] = -static_cast<double>(OSQP_INFTY);
    if (std::isnan(problem.u[i])) problem.u[i] = static_cast<double>(OSQP_INFTY);
  }
  return problem;
}

Result solve(const Problem& problem, double eps_abs, double eps_rel, int max_iter, bool polish) {
  Csc P;
  if (problem.P.empty()) {  // an LP: an n×n zero P
    P.rows = static_cast<OSQPInt>(problem.variables);
    P.cols = P.rows;
    P.p.assign(static_cast<std::size_t>(problem.variables) + 1, 0);
  } else {
    P = upper_csc(problem.P);
  }
  Csc A = full_csc(problem.A, problem.variables);
  std::vector<OSQPFloat> q(problem.q.begin(), problem.q.end());
  std::vector<OSQPFloat> l(problem.l.begin(), problem.l.end());
  std::vector<OSQPFloat> u(problem.u.begin(), problem.u.end());

  OSQPSettings settings;
  osqp_set_default_settings(&settings);
  settings.verbose = 0;
  settings.eps_abs = static_cast<OSQPFloat>(eps_abs > 0.0 ? eps_abs : 1e-5);
  settings.eps_rel = static_cast<OSQPFloat>(eps_rel > 0.0 ? eps_rel : 1e-5);
  settings.max_iter = static_cast<OSQPInt>(max_iter > 0 ? max_iter : 4000);
  settings.polishing = polish ? 1 : 0;

  OSQPCscMatrix p_matrix = P.matrix();
  OSQPCscMatrix a_matrix = A.matrix();
  OSQPSolver* solver = nullptr;
  const OSQPInt setup = osqp_setup(&solver, &p_matrix, q.data(), &a_matrix, l.data(), u.data(),
                                   static_cast<OSQPInt>(problem.constraints),
                                   static_cast<OSQPInt>(problem.variables), &settings);
  if (setup != 0) throw std::runtime_error("OSQP setup failed (" + std::to_string(setup) + ")");

  Result result;
  const OSQPInt solved = osqp_solve(solver);
  if (solved != 0) {
    result.status = "error " + std::to_string(solved);
  } else {
    result.status = solver->info->status;
    result.iterations = static_cast<double>(solver->info->iter);
    result.objective = std::isfinite(solver->info->obj_val) ? solver->info->obj_val : 0.0;
    if (solver->info->status_val == OSQP_SOLVED || solver->info->status_val == OSQP_SOLVED_INACCURATE) {
      result.x.assign(solver->solution->x, solver->solution->x + problem.variables);
    }
  }
  if (result.x.empty()) result.x.assign(problem.variables, 0.0);
  osqp_cleanup(solver);
  return result;
}

}  // namespace paperboat::toolkit::optimization
