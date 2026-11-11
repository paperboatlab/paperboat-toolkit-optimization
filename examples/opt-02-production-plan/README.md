# opt-02-production-plan

A linear program solved as one step in a Step flow, with the optional `optimization` toolkit.

```
Start → Solve LP → Stop
           └→ Graph and Display of x
```

`plan.lp.json` follows MATLAB's fmincon field names:

```
min fᵀx   with   A·x ≤ b,   lb ≤ x ≤ ub
```

```json
{ "f": [-3, -2], "A": [[2, 1], [1, 2], [1, 1]], "b": [8, 8, 5], "lb": [0, 0] }
```

The two products earn 3 and 2 per unit; machine hours (2,1), (1,2) and labour (1,1) are capped at 8,
8 and 5. The solution is `x = [3, 2]`, objective `-13` (a profit of 13).

## Run

```sh
paperboat toolkit install optimization --repo https://github.com/paperboatlab/paperboat-toolkit-optimization.git
paperboat run opt-02-production-plan
```
