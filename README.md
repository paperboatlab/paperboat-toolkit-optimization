# simplicial-toolkit-optimization

The optional optimization toolkit for [Paperboat](https://github.com/paperboatlab/paperboat): linear,
quadratic and nonlinear programs on the Flow page (`lp`, `qp`, `nlp`), solved by
[OSQP](https://osqp.org) and, optionally, [Ipopt](https://coin-or.github.io/Ipopt/), in MATLAB
fmincon's field names. Its `Workspace` is also the per-step solver of the control toolkit's MPC.

**Docs:** <https://paperboatlab.github.io/toolkits/optimization/>, also in the Help window (F1) of
Paperboat. Their sources live with the code: `engine/toolkit/optimization/docs/` for the toolkit's
pages, and a block's notes in its folder (`src/optimization/<block>/doc.rst`); a block's ports and
params come from the catalog, so write only what the catalog can't say.

## Install

```sh
paperboat toolkit install optimization --repo https://github.com/paperboatlab/simplicial-toolkit-optimization.git
```

## CI

The build job checks out `paperboatlab/paperboat` and runs the engine's tests with this
toolkit overlaid; the validate job checks `toolkit.toml`; the docs step builds the docs site as the org site does.

## Licence

GNU General Public License version 3 (see [LICENSE](LICENSE)). Installed by the user and never
bundled into a Paperboat release artifact.
