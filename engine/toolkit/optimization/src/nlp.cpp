// The nonlinear program: JSON reading and the optional Ipopt workspace.

#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "paperboat/toolkit/expression.hpp"
#include "paperboat/toolkit/optimization.hpp"

#ifdef PAPERBOAT_HAS_IPOPT
#include <IpIpoptApplication.hpp>
#include <IpTNLP.hpp>
#endif

namespace paperboat::toolkit::optimization {
namespace {

constexpr double kInf = 1e20;

double number_or(const nlohmann::json& doc, const char* key, double fallback) {
  if (!doc.contains(key) || doc[key].is_null()) return fallback;
  if (!doc[key].is_number()) throw std::runtime_error(std::string("\"") + key + "\" must be a number");
  return doc[key].get<double>();
}

std::string string_of(const nlohmann::json& doc, const char* key, const std::string& at) {
  if (!doc.contains(key) || !doc[key].is_string() || doc[key].get<std::string>().empty())
    throw std::runtime_error(at + ": needs a non-empty \"" + key + "\"");
  return doc[key].get<std::string>();
}

}  // namespace

NlpProblem read_nlp_problem(const std::string& file) {
  if (file.empty()) throw std::runtime_error("needs a file: a JSON NLP with variables, objective and constraints");
  std::ifstream in(file);
  if (!in) throw std::runtime_error("cannot open " + file);
  nlohmann::json doc;
  try {
    in >> doc;
  } catch (const nlohmann::json::exception& err) {
    throw std::runtime_error(file + ": " + err.what());
  }
  if (!doc.is_object()) throw std::runtime_error(file + ": the problem must be a JSON object");

  NlpProblem problem;
  if (!doc.contains("variables") || !doc["variables"].is_array() || doc["variables"].empty())
    throw std::runtime_error(file + ": needs \"variables\" (a non-empty array)");
  for (const auto& v : doc["variables"]) {
    if (!v.is_object()) throw std::runtime_error(file + ": every variable must be an object");
    NlpVariable variable;
    variable.name = string_of(v, "name", file + ": variable");
    variable.initial = number_or(v, "initial", 0.0);
    variable.lower = number_or(v, "lower", -kInf);
    variable.upper = number_or(v, "upper", kInf);
    if (variable.lower > variable.upper)
      throw std::runtime_error(file + ": variable \"" + variable.name + "\" has lower > upper");
    problem.variables.push_back(std::move(variable));
  }

  problem.objective = string_of(doc, "objective", file);
  if (doc.contains("constraints")) {
    if (!doc["constraints"].is_array()) throw std::runtime_error(file + ": \"constraints\" must be an array");
    for (const auto& c : doc["constraints"]) {
      if (!c.is_object()) throw std::runtime_error(file + ": every constraint must be an object");
      NlpConstraint constraint;
      constraint.expression = string_of(c, "expression", file + ": constraint");
      constraint.lower = number_or(c, "lower", -kInf);
      constraint.upper = number_or(c, "upper", kInf);
      if (constraint.lower > constraint.upper)
        throw std::runtime_error(file + ": a constraint has lower > upper");
      problem.constraints.push_back(std::move(constraint));
    }
  }

  // Compile the expressions once here so a bad name/operator fails at the file, not mid-solve.
  std::vector<std::string> names;
  names.reserve(problem.variables.size());
  for (const auto& v : problem.variables) names.push_back(v.name);
  try {
    Expression objective(problem.objective, names);
    (void)objective;
    for (const auto& c : problem.constraints) {
      Expression constraint(c.expression, names);
      (void)constraint;
    }
  } catch (const ExpressionError& err) {
    throw std::runtime_error(file + ": " + err.what());
  }
  return problem;
}

bool has_nlp() {
#ifdef PAPERBOAT_HAS_IPOPT
  return true;
#else
  return false;
#endif
}

#ifdef PAPERBOAT_HAS_IPOPT

namespace {

class NlpTNLP final : public Ipopt::TNLP {
 public:
  NlpTNLP(const NlpProblem& problem, std::vector<Expression> constraints, Vector x0)
      : problem_(problem), constraints_(std::move(constraints)), x0_(std::move(x0)) {
    names_.reserve(problem_.variables.size());
    for (const auto& v : problem_.variables) names_.push_back(v.name);
    objective_expr_ = std::make_unique<Expression>(problem_.objective, names_);
  }

  bool get_nlp_info(Ipopt::Index& n, Ipopt::Index& m, Ipopt::Index& nnz_jac_g, Ipopt::Index& nnz_h_lag,
                    IndexStyleEnum& index_style) override {
    n = static_cast<Ipopt::Index>(problem_.variables.size());
    m = static_cast<Ipopt::Index>(constraints_.size());
    nnz_jac_g = m * n;
    nnz_h_lag = 0;  // limited-memory Hessian
    index_style = C_STYLE;
    return true;
  }

  bool get_bounds_info(Ipopt::Index n, Ipopt::Number* x_l, Ipopt::Number* x_u, Ipopt::Index m,
                       Ipopt::Number* g_l, Ipopt::Number* g_u) override {
    for (Ipopt::Index i = 0; i < n; ++i) {
      x_l[i] = problem_.variables[static_cast<std::size_t>(i)].lower;
      x_u[i] = problem_.variables[static_cast<std::size_t>(i)].upper;
    }
    for (Ipopt::Index i = 0; i < m; ++i) {
      g_l[i] = problem_.constraints[static_cast<std::size_t>(i)].lower;
      g_u[i] = problem_.constraints[static_cast<std::size_t>(i)].upper;
    }
    return true;
  }

  bool get_starting_point(Ipopt::Index n, bool init_x, Ipopt::Number* x, bool init_z, Ipopt::Number* /*z_L*/,
                          Ipopt::Number* /*z_U*/, Ipopt::Index /*m*/, bool init_lambda,
                          Ipopt::Number* /*lambda*/) override {
    if (!init_x) return false;
    for (Ipopt::Index i = 0; i < n; ++i) {
      const std::size_t at = static_cast<std::size_t>(i);
      x[i] = at < x0_.size() ? x0_[at] : problem_.variables[at].initial;
    }
    return !init_z && !init_lambda;
  }

  bool eval_f(Ipopt::Index n, const Ipopt::Number* x, bool /*new_x*/, Ipopt::Number& obj_value) override {
    obj_value = (*objective_expr_)(std::span<const double>(x, static_cast<std::size_t>(n)));
    return true;
  }

  bool eval_grad_f(Ipopt::Index n, const Ipopt::Number* x, bool /*new_x*/, Ipopt::Number* grad_f) override {
    for (Ipopt::Index i = 0; i < n; ++i) {
      const std::size_t at = static_cast<std::size_t>(i);
      const double h = 1e-6 * std::max(1.0, std::abs(x[at]));
      std::vector<double> plus(x, x + n), minus(x, x + n);
      plus[at] += h;
      minus[at] -= h;
      grad_f[at] = ((*objective_expr_)(plus) - (*objective_expr_)(minus)) / (2.0 * h);
    }
    return true;
  }

  bool eval_g(Ipopt::Index n, const Ipopt::Number* x, bool /*new_x*/, Ipopt::Index m,
              Ipopt::Number* g) override {
    for (Ipopt::Index i = 0; i < m; ++i)
      g[i] = constraints_[static_cast<std::size_t>(i)](std::span<const double>(x, static_cast<std::size_t>(n)));
    return true;
  }

  bool eval_jac_g(Ipopt::Index n, const Ipopt::Number* x, bool /*new_x*/, Ipopt::Index m, Ipopt::Index /*nele_jac*/,
                  Ipopt::Index* iRow, Ipopt::Index* jCol, Ipopt::Number* values) override {
    if (values == nullptr) {
      Ipopt::Index k = 0;
      for (Ipopt::Index i = 0; i < m; ++i)
        for (Ipopt::Index j = 0; j < n; ++j) {
          iRow[k] = i;
          jCol[k] = j;
          ++k;
        }
      return true;
    }
    for (Ipopt::Index i = 0; i < m; ++i) {
      const auto& expression = constraints_[static_cast<std::size_t>(i)];
      for (Ipopt::Index j = 0; j < n; ++j) {
        const std::size_t at = static_cast<std::size_t>(j);
        const double h = 1e-6 * std::max(1.0, std::abs(x[at]));
        std::vector<double> plus(x, x + n), minus(x, x + n);
        plus[at] += h;
        minus[at] -= h;
        values[static_cast<std::size_t>(i * n + j)] =
            (expression(plus) - expression(minus)) / (2.0 * h);
      }
    }
    return true;
  }

  bool eval_h(Ipopt::Index /*n*/, const Ipopt::Number* /*x*/, bool /*new_x*/, Ipopt::Number /*obj_factor*/,
              Ipopt::Index /*m*/, const Ipopt::Number* /*lambda*/, bool /*new_lambda*/, Ipopt::Index /*nele_hess*/,
              Ipopt::Index* /*iRow*/, Ipopt::Index* /*jCol*/, Ipopt::Number* /*values*/) override {
    return false;  // limited-memory
  }

  void finalize_solution(Ipopt::SolverReturn status, Ipopt::Index n, const Ipopt::Number* x,
                         const Ipopt::Number* /*z_L*/, const Ipopt::Number* /*z_U*/, Ipopt::Index /*m*/,
                         const Ipopt::Number* /*g*/, const Ipopt::Number* /*lambda*/, Ipopt::Number obj_value,
                         const Ipopt::IpoptData* /*ip_data*/, Ipopt::IpoptCalculatedQuantities* /*ip_cq*/) override {
    x_.assign(x, x + n);
    objective_value_ = obj_value;
    status_ = status;
  }

  const Vector& x() const { return x_; }
  double objective() const { return objective_value_; }
  Ipopt::SolverReturn status() const { return status_; }

 private:
  NlpProblem problem_;
  std::vector<Expression> constraints_;
  Vector x0_;
  std::vector<std::string> names_;
  std::unique_ptr<Expression> objective_expr_;
  Vector x_;
  double objective_value_ = 0.0;
  Ipopt::SolverReturn status_ = Ipopt::Invalid_Number_Detected;
};

std::string status_of(Ipopt::ApplicationReturnStatus status) {
  switch (status) {
    case Ipopt::Solve_Succeeded: return "solved";
    case Ipopt::Solved_To_Acceptable_Level: return "solved (acceptable)";
    case Ipopt::Infeasible_Problem_Detected: return "infeasible";
    case Ipopt::Maximum_Iterations_Exceeded: return "max iter";
    case Ipopt::Restoration_Failed: return "restoration failed";
    case Ipopt::Invalid_Number_Detected: return "invalid number";
    default: return "ipopt status " + std::to_string(static_cast<int>(status));
  }
}

}  // namespace

struct NlpWorkspace::Impl {
  explicit Impl(Options o) : options(o) {}
  Options options;

  Result solve(const NlpProblem& problem, const Vector& x0) {
    std::vector<std::string> names;
    names.reserve(problem.variables.size());
    for (const auto& v : problem.variables) names.push_back(v.name);
    std::vector<Expression> constraints;
    constraints.reserve(problem.constraints.size());
    for (const auto& c : problem.constraints) constraints.emplace_back(c.expression, names);

    Ipopt::SmartPtr<NlpTNLP> nlp = new NlpTNLP(problem, std::move(constraints), x0);
    Ipopt::SmartPtr<Ipopt::IpoptApplication> app = IpoptApplicationFactory();
    app->Options()->SetNumericValue("tol", options.eps_abs > 0 ? options.eps_abs : 1e-7);
    app->Options()->SetIntegerValue("max_iter", options.max_iter > 0 ? options.max_iter : 500);
    app->Options()->SetStringValue("hessian_approximation", "limited-memory");
    app->Options()->SetStringValue("print_level", "0");
    app->Options()->SetStringValue("sb", "yes");
    if (app->Initialize() != Ipopt::Solve_Succeeded) throw std::runtime_error("Ipopt failed to initialize");

    const auto started = std::chrono::steady_clock::now();
    const Ipopt::ApplicationReturnStatus status = app->OptimizeTNLP(nlp);
    const auto finished = std::chrono::steady_clock::now();

    Result result;
    result.x = nlp->x();
    result.objective = nlp->objective();
    result.iterations = static_cast<double>(app->Statistics()->IterationCount());
    result.solve_time = std::chrono::duration<double>(finished - started).count();
    result.status = status_of(status);
    return result;
  }
};

#else  // no Ipopt

struct NlpWorkspace::Impl {
  explicit Impl(Options o) : options(o) {}
  Options options;

  Result solve(const NlpProblem&, const Vector&) {
    throw std::runtime_error("NLP needs Ipopt: build the optimization toolkit with Ipopt (coinor-libipopt-dev)");
  }
};

#endif

NlpWorkspace::NlpWorkspace(Options options) : impl_(std::make_unique<Impl>(options)) {}
NlpWorkspace::~NlpWorkspace() = default;

Result NlpWorkspace::solve(const NlpProblem& problem, const Vector& x0) { return impl_->solve(problem, x0); }

Result NlpWorkspace::solve(const std::string& file, const Vector& x0) {
  return impl_->solve(read_nlp_problem(file), x0);
}

const Options& NlpWorkspace::options() const { return impl_->options; }

void NlpWorkspace::set_options(const Options& options) { impl_->options = options; }

}  // namespace paperboat::toolkit::optimization
