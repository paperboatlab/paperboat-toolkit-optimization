# paperboat-toolkit-optimization

The optional optimization toolkit for [Paperboat](https://github.com/paperboatlab/paperboat).

It provides the `optimization` package: quadratic and linear programs on the Flow page,
solved by [OSQP](https://osqp.org) (Apache-2.0). The blocks are `qp` (min ½·xᵀPx + qᵀx) and
`lp` (min cᵀx), both subject to `l ≤ Ax ≤ u`, with `x`, `objective`, `iterations` and
`status` outputs. A problem is a JSON file on the block:

```json
{ "P": [[1, 0], [0, 1]], "q": [-2, -1], "A": [[1, 1]], "l": [1], "u": [1] }
```

`P` uses its upper triangle, a `null` in `l` or `u` is an infinite bound, and `A`, `l` and
`u` may be omitted. An LP uses `c` instead of `P`/`q`. These blocks are usually run once (a
design point, a plan, a schedule): the block solves on its first step and keeps the answer.

## Using it

Overlay this repository onto a Paperboat checkout so the toolkit sits at
`engine/toolkit/optimization/` (the engine discovers it automatically from its `toolkit.toml`):

```sh
# from your paperboat checkout
git clone https://github.com/paperboatlab/paperboat-toolkit-optimization /tmp/pb-optimization
cp -r /tmp/pb-optimization/engine/toolkit/optimization engine/toolkit/optimization
```

The toolkit finds a system OSQP (`find_package(osqp)`) or fetches OSQP v1.0.0. Configure
and build the engine as usual:

```sh
cmake -S engine -B engine/build -G Ninja -DPAPERBOAT_BUILD_TESTS=ON
cmake --build engine/build
```

In your backend's `main`, register it next to the builtins:

```cpp
tk::Registry r = tk::builtins();
tk::optimization::register_optimization(r);
```

and import `"optimization"` in your `descriptor.toml`. A backend that lists the toolkit in
its `backend.toml` (`toolkits = ["optimization"]`) is checked for it by `paperboat check`.

## CI

The build job checks out `paperboatlab/paperboat` and runs the engine's tests with this
toolkit overlaid; the validate job checks `toolkit.toml`.
