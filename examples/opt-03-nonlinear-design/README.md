# opt-03-nonlinear-design

A nonlinear program solved as one step in a Step flow, with the optional `optimization` toolkit
and Ipopt.

```
Start → Solve NLP → Stop
            └→ Graph and Display of x
```

`design.nlp.json` is the MATLAB fmincon shape:

```json
{
  "fun": "(x(1) - 1)^2 + (x(2) - 2)^2",
  "x0": [0.5, 1.5],
  "Aeq": [[1, 1]], "beq": [2],
  "lb": [0, 0], "ub": [2, 2],
  "nonlcon": { "c": ["x(1)^2 + x(2)^2 - 2"] },
  "options": { "tol": 1e-8, "max_iter": 400 }
}
```

`fun` is the objective, `x0` the start, `A`/`b` are inequalities, `Aeq`/`beq` equalities,
`lb`/`ub` bounds and `nonlcon.c`/`nonlcon.ceq` the nonlinear `c(x) ≤ 0` and `ceq(x) = 0`.
Expressions take `x(1)`, `x(2)` (or `x1`, `x2`) and the usual functions (`sin`, `exp`, `^`, …).

The closest point to `(1, 2)` on `x₁ + x₂ = 2` and `x₁² + x₂² ≤ 2` is `x = [1, 1]`, objective 1.

## Run

```sh
sudo apt install coinor-libipopt-dev   # the NLP block needs Ipopt
paperboat toolkit install optimization --repo https://github.com/paperboatlab/paperboat-toolkit-optimization.git
paperboat run opt-03-nonlinear-design
```

Without Ipopt the block still registers and says `needs Ipopt`.
