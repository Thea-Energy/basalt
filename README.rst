====
PHNX
====

PHNX is a Python meshing library for nuclear workflows. It wraps the
commercial `Simmetrix SimModSuite <https://www.simmetrix.com/>`__
library through C++ bindings, ingests Parasolid CAD assemblies,
performs non-manifold imprinting, generates surface and volume
meshes, and exports them as Gmsh ``.msh`` files annotated for use
with `Stellarmesh <https://github.com/Thea-Energy/stellarmesh>`__
and DAGMC.

**Features**

* Parasolid (``.x_t``) assembly import via Simmetrix
* NX user-attribute ingestion through a JSON sidecar
* Non-manifold imprint and merge of conformal geometry
* Surface and volume meshing with curvature and proximity refinement
* Gmsh export with metadata for downstream DAGMC conversion

---------------
Getting Started
---------------

* `Installation <install.html>`__
* `Usage <usage.html>`__
* `API <api.html>`__

-------
Example
-------

.. code:: python

   import phnx

   model = phnx.Model.from_parasolid_file("geometry.x_t")
   nm_model = model.translate().make_non_manifold_model()

   mesh_case = phnx.MeshCase(nm_model)
   mesh_case.set_size(0.1)

   surface_mesh = phnx.SurfaceMesh.from_model(nm_model, mesh_case)
   volume_mesh = phnx.VolumeMesh.from_surface_mesh(surface_mesh)
   volume_mesh.write_gmsh("output.msh")

----------------
Acknowledgements
----------------

PHNX is a project of `Thea Energy <https://thea.energy/>`__.
