Install
=======

.. code-block:: sh

   paperboat toolkit install optimization --repo https://github.com/paperboatlab/paperboat-toolkit-optimization.git

It finds a system OSQP (``find_package(osqp)``) or fetches OSQP v1.0.0; ``nlp`` needs a build with
Ipopt. A C++ backend registers it next to the builtins:

.. code-block:: cpp

   tk::Registry r = tk::builtins();
   tk::optimization::register_optimization(r);

and imports ``"optimization"`` in its ``descriptor.toml``. A backend that lists the toolkit in its
``backend.toml`` (``toolkits = ["optimization"]``) is checked for it by ``paperboat check``.

To work on the toolkit itself, overlay it onto a Paperboat checkout so it sits at
``engine/toolkit/optimization/`` and build the engine as usual:

.. code-block:: sh

   cp -r engine/toolkit/optimization ../paperboat/engine/toolkit/optimization
   cmake -S ../paperboat/engine -B ../paperboat/engine/build -G Ninja -DPAPERBOAT_BUILD_TESTS=ON
   cmake --build ../paperboat/engine/build
