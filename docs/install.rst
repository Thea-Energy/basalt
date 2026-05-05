============
Installation
============

PHNX is built from source. You will need:

* `pixi <https://pixi.sh>`__ for environment management
* `CMake <https://cmake.org>`__ ≥ 3.20
* A valid `Simmetrix SimModSuite <https://www.simmetrix.com/>`__
  license

-----
Build
-----

.. code:: sh

   pixi shell
   make conf
   make build

``make conf`` configures the CMake build into ``build/``. ``make build``
compiles the C++ extension and installs it into ``phnx/_core/`` so the
package is importable in editable mode.

------
Verify
------

.. code:: sh

   python -c "import phnx; print(phnx.Model)"

If the import succeeds and prints the ``Model`` class, the build is
working.
