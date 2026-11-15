Problem files
=============

Problems are JSON files on the block, in **MATLAB fmincon's field names**.

Linear and quadratic programs
-----------------------------

A QP is min ½ xᵀHx + fᵀx, an LP min fᵀx:

.. code-block:: javascript

   {
     "H": [[2, 0], [0, 2]],      // QP only; the upper triangle is used
     "f": [-2, -1],
     "A": [[2, 1], [1, 2]],      // A x <= b
     "b": [8, 8],
     "Aeq": [[1, 1]],            // Aeq x = beq
     "beq": [1],
     "lb": [0, 0],               // lb <= x <= ub; null is infinite
     "ub": [1, 1],
     "x0": [0.5, 0.5]            // accepted, used by the NLP solver
   }

Nonlinear programs
------------------

The nonlinear block takes the full fmincon form:

.. code-block:: javascript

   {
     "fun": "(x(1) - 1)^2 + (x(2) - 2)^2",
     "x0": [0.5, 1.5],
     "Aeq": [[1, 1]], "beq": [2],
     "lb": [0, 0], "ub": [2, 2],
     "nonlcon": {
       "c":   ["x(1)^2 + x(2)^2 - 2"],   // c(x) <= 0
       "ceq": []                          // ceq(x) = 0
     },
     "options": { "tol": 1e-8, "max_iter": 400, "display": "iter" }
   }

Write ``x(1)``, ``x(2)`` (or ``x1``, ``x2``) in expressions; the language is the same as the signal
generators' (``^``, ``sin``, ``exp``, …).

Options
-------

``options.display`` follows MATLAB's ``Display``: ``"iter"`` prints the solver's iteration table,
``"final"`` one summary line (objective, iterations, ``x``), and ``"off"`` nothing. It works in the
LP and QP files too. The older solver-shaped forms (``P``/``q``/``A``/``l``/``u``, ``c``, and
``variables``/``objective``/``constraints``) are still read.
