============
Contributing
============

Thank you for your interest in contributing to phnx. This page covers the
practical details of submitting bug reports, feature requests, and pull
requests.

------------
Bug reports
------------

File issues at the project's GitHub issue tracker. Please include:

* The phnx version (``python -c "import phnx; print(phnx.__version__)"``)
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
