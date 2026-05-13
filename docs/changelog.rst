=========
Changelog
=========

This document records the user-visible changes between basalt releases.
The format follows `Keep a Changelog <https://keepachangelog.com>`__,
and this project follows `Semantic Versioning <https://semver.org>`__.

-------------
Unreleased
-------------

* **Renamed project.** Previously ``phnx``, now ``basalt``. The PyPI distribution
  is now ``basalt-mesh`` and the Python import is ``basalt``. No compatibility
  shim: ``import phnx`` raises ``ModuleNotFoundError``. The C extension module
  name ``_core`` is unchanged. The NX journal environment variable changed from
  ``PHNX_NX_EXPORT_DIR`` to ``BASALT_NX_EXPORT_DIR``. See NE-11.

-------------
1.0.0 (TBD)
-------------

Initial public release.

* Parasolid (``.x_t``) assembly import via Simmetrix SimModSuite.
* NX user-attribute ingestion through a JSON sidecar.
* Non-manifold imprint and merge of conformal geometry.
* Surface and volume meshing with curvature and proximity refinement.
* Gmsh ``.msh`` export with metadata for downstream DAGMC conversion.
