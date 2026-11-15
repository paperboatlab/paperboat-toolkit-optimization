The C++ workspace
=================

The same solver is a C++ workspace for a per-step controller. It keeps the OSQP factorization when
the dimensions and sparsity are unchanged, updates the values, and warm-starts from the last
solution. The control toolkit's ``mpc`` uses ``Workspace::solve`` rather than copying OSQP:

.. code-block:: cpp

   #include "paperboat/toolkit/optimization.hpp"

   namespace opt = paperboat::toolkit::optimization;

   opt::Workspace workspace;
   opt::Problem problem = opt::read_problem("cart_pole.dyn.json", true);
   auto result = workspace.solve(problem);
   // Next step: update problem.q/problem.A and solve again. Same shape: warm start.
