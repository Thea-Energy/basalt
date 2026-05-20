============
Contributing
============

Thank you for your interest in contributing to Basalt. This page covers the
practical details of submitting bug reports, feature requests, and pull
requests.

------------
Bug reports
------------

File issues at the project's GitHub issue tracker. Please include:

* The Basalt version (``python -c "import basalt; print(basalt.__version__)"``)
* The Simmetrix SimModSuite version you have installed
* A minimal reproducing example
* The full error traceback or unexpected output

----------------
Feature requests
----------------

Open a GitHub issue describing the use case and the desired behaviour.
Larger features benefit from a short design discussion in the issue
before code lands.

-------------
Pull requests
-------------

* Fork the repository and create a topic branch.
* Run ``ruff check .`` and ``ruff format --check .`` locally before opening
  the PR.
* Add or update tests for behavioural changes.
* Update ``docs/changelog.rst`` with a brief entry describing the change.
* Reference any relevant issue numbers in the PR description.

-------------
CI coverage
-------------

The public CI workflow runs lint (ruff) and the Sphinx documentation
build on every push and pull request. It does **not** compile the
C++ extension or run the test suite, because both require a Simmetrix
SimModSuite license that is not available on standard GitHub Actions
runners.

This means **maintainers are responsible for running the full build and
test suite locally before merging**:

.. code:: sh

   pixi run build
   pixi run pytest

If you're submitting a PR as an external contributor, please confirm
locally that ``pixi run build`` succeeds and the tests pass against your
SMS install before requesting review.

----------
Code style
----------

Code style is enforced by ``ruff`` using the configuration in
``pyproject.toml``. Run ``ruff format .`` to auto-format your changes.

--------------------------
W7-X tutorial re-execution
--------------------------

The neutronics tutorial at
``docs/notebooks/tutorials/w7x_neutronics.ipynb`` is committed with its
cell outputs intact. Sphinx renders the notebook **without** re-executing
it (``nbsphinx_execute = "never"``) — so the committed outputs are the
source of truth for what readers see.

Re-execute and commit the regenerated notebook whenever any of the
following change:

* Basalt's public API (``Model``, ``MeshCase``, ``SurfaceMesh``,
  ``VolumeMesh``, ``Mesh.write_msh``, ``print_hierarchy``,
  ``load_material_metadata``).
* The ``.msh`` format spec (Stellarmesh's ``docs/format.rst``).
* The W7-X fixture (``tests/data/scaled_w7x_stellarator.x_t`` or its
  ``_attrs.json`` sidecar).
* Stellarmesh's DAGMC / Mesh / MOAB API surface that the notebook
  consumes.
* OpenMC's tally, source, or run APIs that the notebook consumes.

To re-execute (requires a working Simmetrix SimModSuite license):

.. code:: sh

   pixi run jupyter nbconvert --to notebook --execute --inplace \
     docs/notebooks/tutorials/w7x_neutronics.ipynb

Then ``git diff`` the file to confirm only intended output changes
landed, and commit.

Before tagging a release, re-execute the notebook against ``main`` and
commit any drift. CI does **not** detect stale outputs — this is a
maintainer responsibility.
