=========
Changelog
=========

This document records the user-visible changes between Basalt releases.
The format follows `Keep a Changelog <https://keepachangelog.com>`__,
and this project follows `Semantic Versioning <https://semver.org>`__.

-------------
0.1
-------------

Initial public release.

* Parasolid (``.x_t``) assembly import via Simmetrix SimModSuite.
* NX user-attribute ingestion through a JSON sidecar.
* Non-manifold imprint and merge of conformal geometry.
* Surface and volume meshing with curvature and proximity refinement.
* Gmsh ``.msh`` export with metadata for downstream DAGMC conversion.
* End-to-end W7-X stellarator neutronics tutorial demonstrating the
  full Parasolid → Basalt mesh → Stellarmesh DAGMC → OpenMC pipeline,
  with four tallies and a 3D PyVista flux render.
* ``docs/format.rst`` documenting Basalt's producer-side ``.msh``
  output semantics; defers to Stellarmesh's canonical format spec for
  the schema itself.
