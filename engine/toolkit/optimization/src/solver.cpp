// Reading a problem file and running OSQP.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <osqp.h>

#include "paperboat/toolkit/optimization.hpp"


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

namespace {

using Clock = std::chrono::steady_clock;

// The same sparsity pattern means OSQP can keep its factorization and update values only.
bool same_pattern(const Csc& a, const Csc& b) { return a.i == b.i && a.p == b.p; }

}  // namespace

struct Workspace::Impl {
  explicit Impl(Options o) : options(o) {}
  ~Impl() { cleanup(); }

  void cleanup() {
    if (solver) {
      osqp_cleanup(solver);
      solver = nullptr;
    }
    has_last = false;
  }

  Result solve(const Problem& problem, const Vector& x0) {
    Csc new_P;
    if (problem.P.empty()) {  // an LP: an n×n zero P
      new_P.rows = static_cast<OSQPInt>(problem.variables);
      new_P.cols = new_P.rows;
      new_P.p.assign(static_cast<std::size_t>(problem.variables) + 1, 0);
    } else {
      new_P = upper_csc(problem.P);
    }
    Csc new_A = full_csc(problem.A, problem.variables);
    std::vector<OSQPFloat> new_q(problem.q.begin(), problem.q.end());
    std::vector<OSQPFloat> new_l(problem.l.begin(), problem.l.end());
    std::vector<OSQPFloat> new_u(problem.u.begin(), problem.u.end());

    const bool reuse = solver != nullptr && n == static_cast<OSQPInt>(problem.variables) &&
                       m == static_cast<OSQPInt>(problem.constraints) && same_pattern(P, new_P) &&
                       same_pattern(A, new_A);
    if (!reuse) {
      cleanup();
      P = std::move(new_P);
      A = std::move(new_A);
      q = std::move(new_q);
      l = std::move(new_l);
      u = std::move(new_u);
      n = static_cast<OSQPInt>(problem.variables);
      m = static_cast<OSQPInt>(problem.constraints);

      OSQPSettings settings;
      osqp_set_default_settings(&settings);
      settings.verbose = 0;
      settings.eps_abs = static_cast<OSQPFloat>(options.eps_abs > 0.0 ? options.eps_abs : 1e-5);
      settings.eps_rel = static_cast<OSQPFloat>(options.eps_rel > 0.0 ? options.eps_rel : 1e-5);
      settings.max_iter = static_cast<OSQPInt>(options.max_iter > 0 ? options.max_iter : 4000);
      settings.polishing = options.polish ? 1 : 0;

      OSQPCscMatrix p_matrix = P.matrix();
      OSQPCscMatrix a_matrix = A.matrix();
      const OSQPInt setup = osqp_setup(&solver, &p_matrix, q.data(), &a_matrix, l.data(), u.data(), m, n, &settings);
      if (setup != 0) {
        solver = nullptr;
        throw std::runtime_error("OSQP setup failed (" + std::to_string(setup) + ")");
      }
    } else {
      P.x = std::move(new_P.x);
      A.x = std::move(new_A.x);
      q = std::move(new_q);
      l = std::move(new_l);
      u = std::move(new_u);
      const OSQPFloat* px = P.x.empty() ? nullptr : P.x.data();
      const OSQPFloat* ax = A.x.empty() ? nullptr : A.x.data();
      const OSQPFloat* lp = l.empty() ? nullptr : l.data();
      const OSQPFloat* up = u.empty() ? nullptr : u.data();
      osqp_update_data_mat(solver, px, nullptr, static_cast<OSQPInt>(P.x.size()), ax, nullptr,
                           static_cast<OSQPInt>(A.x.size()));
      osqp_update_data_vec(solver, q.data(), lp, up);
    }

    if (options.warm_start) {
      const Vector& warm = !x0.empty() ? x0 : last_x;
      if (!warm.empty() && static_cast<OSQPInt>(warm.size()) == n)
        osqp_warm_start(solver, warm.data(), nullptr);
    }

    const auto started = Clock::now();
    const OSQPInt solved = osqp_solve(solver);
    const auto finished = Clock::now();

    Result result;
    result.solve_time = std::chrono::duration<double>(finished - started).count();
    if (solved != 0) {
      result.status = "error " + std::to_string(solved);
    } else {
      result.status = solver->info->status;
      result.iterations = static_cast<double>(solver->info->iter);
      result.objective = std::isfinite(solver->info->obj_val) ? solver->info->obj_val : 0.0;
      if (solver->info->status_val == OSQP_SOLVED || solver->info->status_val == OSQP_SOLVED_INACCURATE) {
        result.x.assign(solver->solution->x, solver->solution->x + n);
        last_x = result.x;
        has_last = true;
      }
    }
    if (result.x.empty()) result.x.assign(static_cast<std::size_t>(n), 0.0);
    return result;
  }

  Options options;
  OSQPSolver* solver = nullptr;
  Csc P, A;
  std::vector<OSQPFloat> q, l, u;
  OSQPInt n = 0, m = 0;
  Vector last_x;
  bool has_last = false;
};

Workspace::Workspace(Options options) : impl_(std::make_unique<Impl>(options)) {}
Workspace::~Workspace() = default;
Workspace::Workspace(Workspace&&) noexcept = default;
Workspace& Workspace::operator=(Workspace&&) noexcept = default;

Result Workspace::solve(const Problem& problem, const Vector& x0) { return impl_->solve(problem, x0); }

Result Workspace::solve(const std::string& file, bool quadratic, const Vector& x0) {
  return impl_->solve(read_problem(file, quadratic), x0);
}

void Workspace::restart() { impl_->cleanup(); }

const Options& Workspace::options() const { return impl_->options; }

void Workspace::set_options(const Options& options) {
  impl_->options = options;
  impl_->cleanup();
}

Result solve(const Problem& problem, const Options& options, const Vector& x0) {
  Workspace workspace(options);
  return workspace.solve(problem, x0);
}

}  // namespace paperboat::toolkit::optimization
