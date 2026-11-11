# opt-01-beam-sizing

A small quadratic program solved as one step in a Step flow, with the optional
`optimization` toolkit.

```
Start → Solve QP → Stop
           └→ Graph and Display of x
```

`beam.qp.json` uses the fmincon field names:

```
min ½·xᵀHx + fᵀx   with   Aeq·x = beq,  lb ≤ x ≤ ub
```

```json
{
  "H": [[2, 0], [0, 2]],
  "f": [-2, -1],
  "Aeq": [[1, 1]],
  "beq": [1],
  "lb": [0, 0],
  "ub": [1, 1]
}
```

The solution is `x = [0.75, 0.25]`. The `qp` block is a Step block: the path arrives at
`in`, the block solves once, and the path leaves by `out`. The graph and display read the
`x` signal output.

## Run

```sh
paperboat toolkit install optimization --repo https://github.com/paperboatlab/paperboat-toolkit-optimization.git
paperboat run opt-01-beam-sizing
```

Or from this folder:

```sh
paperboat run .
```
