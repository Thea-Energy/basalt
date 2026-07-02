============
Installation
============

Basalt is a Python wrapper for `Simmetrix SimModSuite`_ that you build from source
against your own SMS install. Basalt itself is MIT-licensed; SMS is proprietary
and is not bundled, redistributed, or accessible through Basalt's wheel.

.. _Simmetrix SimModSuite: https://www.simmetrix.com/

------------
Requirements
------------

* **Linux x86-64** with glibc 2.34+ (RHEL 9 line, Ubuntu 22.04+, Debian 12+).
  RHEL 8 / glibc 2.28 is supported via ``SIMMODSUITE_ABI=x64_rhel8_gcc83``.
* **CMake** 3.15 or newer
* **A C++17 compiler** (gcc 11+ for the rhel9 ABI, gcc 8.3+ for the rhel8 ABI)
* **Python** 3.12 or newer
* **A valid Simmetrix license** and the four required SMS module tarballs
  (see `Acquire SimModSuite`_)

------------------------
Acquire SimModSuite
------------------------

Basalt links against four Simmetrix module tarballs. All four must be at the
**same version-datestamp**:

* ``gmcore`` — geometric model core
* ``mscore`` — mesh core
* ``pskrnl`` — Parasolid kernel
* ``simlicense`` — license enforcement

Download all four for ``linux64`` from the Simmetrix customer portal, plus a
license file (path-based or ``port@host`` floating).

------------------------
Lay out the install
------------------------

Extract all four tarballs into a single parent directory so they merge into one
unified tree:

.. code:: sh

   mkdir -p ~/sms/install
   cd ~/sms/install
   tar xzf /path/to/gmcore-linux64.tgz
   tar xzf /path/to/mscore-linux64.tgz
   tar xzf /path/to/pskrnl-linux64.tgz
   tar xzf /path/to/simlicense-linux64.tgz

The result is a version-stamped directory (e.g. ``2026.0-260502``):

.. code:: text

   ~/sms/install/<version>-<datestamp>/      # e.g. 2026.0-260502
       include/
           SimUtil.h
           ...
       lib/
           x64_rhel9_gcc11/
               libSimParasolid<N>.a
               libSimModel.a
               ...
               psKrnl/
                   libpskernel.so
                   schema/
                       *.sch_xmt

-------------------------
Set environment variables
-------------------------

.. code:: sh

   export SIMMODSUITE_ROOT="$HOME/sms/install/<version>-<datestamp>"
   export SimModSuite_licenseFile="/path/to/your/license.lic"
   # Optional: only set if you are not on the default rhel9 ABI
   # export SIMMODSUITE_ABI="x64_rhel8_gcc83"

.. warning::

   ``SimModSuite_licenseFile`` is **case-sensitive** (Simmetrix convention).
   ``SIMMODSUITE_LICENSEFILE`` will not be picked up.

``P_SCHEMA`` is **not** required: Basalt bakes the schema directory at build time
and exports it on import. Customers who need an override may still export
``P_SCHEMA`` and it will take precedence.

----------------
Install with pip
----------------

Basalt ships as a source distribution, so ``pip`` builds the C++ extension at
install time against the SMS install at ``$SIMMODSUITE_ROOT``. ``scikit-build-core``
pulls ``nanobind`` and ``nanobindgen`` into the build environment automatically,
and ``spdlog`` is fetched at configure time by CMake, but the remaining libraries
basalt links — **gmsh** and **nlohmann_json** — must be present at build time
(and gmsh at runtime). In practice basalt is built inside a conda/pixi
environment where these come from conda-forge; a bare ``pip install`` outside
such an environment is not supported.

.. code:: sh

   pip install gmsh    # if you do not already have it
   pip install basalt-mesh

Build output appears in ``build/``; the compiled module lands at
``<site-packages>/basalt/_core/_core.cpython-<...>.so``.

-----------------
Install with pixi
-----------------

.. tabs::

   .. tab:: Pixi git source

      Add to your ``pixi.toml`` under ``[pypi-dependencies]`` (basalt is a
      pypi package that builds against the conda-forge libraries already in
      your environment):

      .. code:: toml

         basalt-mesh = { git = "https://github.com/Thea-Energy/basalt.git", branch = "main", editable = false }

      Then ``pixi install``. ``$SIMMODSUITE_ROOT`` and
      ``$SimModSuite_licenseFile`` must be exported in the shell that runs
      ``pixi install``.

   .. tab:: Pixi path source

      If you have a local checkout:

      .. code:: toml

         basalt-mesh = { path = "/path/to/basalt", editable = true }

      Then ``pixi install``.

------
Verify
------

.. code:: sh

   python -c "import basalt; print(basalt.Model)"

Expected output:

.. code:: text

   <class 'basalt._core._core.Model'>

If import succeeds, the build linked correctly and the license loaded.

---------------
Troubleshooting
---------------

**RuntimeError: SimModSuite_licenseFile environment variable is not set or is empty**
    The license file env var is missing. Note the **case-sensitive** name —
    ``SimModSuite_licenseFile``, not ``SIMMODSUITE_LICENSEFILE`` or
    ``simmodsuite_licensefile``. If you are using a floating license, set the
    variable to ``port@host`` (e.g. ``2800@licsrv.example.com``).

**FATAL_ERROR: SIMMODSUITE_ROOT environment variable is not set**
    Set ``SIMMODSUITE_ROOT`` to the version-stamped directory that resulted
    from extracting all four tarballs into one parent directory.

**FATAL_ERROR: Simmetrix headers not found at .../include/SimUtil.h**
    The four tarballs were not extracted into the same parent directory, or
    ``SIMMODSUITE_ROOT`` points at the wrong level. The path you set should be
    the directory that contains both ``include/`` and ``lib/``.

**FATAL_ERROR: Simmetrix library directory not found at .../lib/<abi>**
    Your SMS install uses a different ABI subdirectory than the default
    ``x64_rhel9_gcc11``. Set ``SIMMODSUITE_ABI`` to match (look at
    ``$SIMMODSUITE_ROOT/lib/`` to see the available ABI directory names).

**FATAL_ERROR: Expected exactly one libSimParasolid*.a in ...**
    Your unified install dir contains zero or multiple SimParasolid library
    files. Re-extract only one Parasolid kernel tarball into the dir.

**ImportError: libpskernel.so: cannot open shared object file**
    The RPATH baked into ``_core.so`` points at
    ``$SIMMODSUITE_ROOT/lib/<abi>/psKrnl``. If you moved or renamed the SMS
    install after building, you will need to rebuild (``pip install
    --force-reinstall --no-deps basalt-mesh``).
