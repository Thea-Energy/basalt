=========
Changelog
=========

This document records the user-visible changes between Basalt releases.
The format follows `Keep a Changelog <https://keepachangelog.com>`__,
and this project follows `Semantic Versioning <https://semver.org>`__.

-------------
Unreleased
-------------

* :py:meth:`Mesh.write_msh` writes only the vertices, edges and faces that
  carry mesh. A volume is written if it holds volume elements or bounds a
  written face, so every ``forward_volume``/``reverse_volume`` a face names
  is in the file; a surface mesh keeps all its volumes, without elements.
  Entities excluded with :py:meth:`MeshCase.set_no_mesh` were previously
  written as empty entities.
* :py:meth:`SurfaceMesh.from_model` no longer writes a ``mesh.sms`` debug
  file to the working directory.
* :py:meth:`MeshCase.set_domain` restricts meshing to the given entities and
  their closure.
* :py:meth:`MeshCase.set_surface_mesh_type` selects triangle, quad-dominant
  or all-quad surface meshes; :py:meth:`Mesh.write_msh` writes quads as Gmsh
  type 3 and raises on volume elements other than tetrahedra.

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
