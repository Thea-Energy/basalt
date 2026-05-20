======
Basalt
======

.. figure:: https://upload.wikimedia.org/wikipedia/commons/thumb/8/89/%D0%9C%D1%8B%D1%81_%D0%A1%D1%82%D0%BE%D0%BB%D0%B1%D1%87%D0%B0%D1%82%D1%8B%D0%B9._%D0%9F%D0%BE%D1%81%D0%BB%D0%B5_%D0%B7%D0%B0%D0%BA%D0%B0%D1%82%D0%B0.jpg/1920px-%D0%9C%D1%8B%D1%81_%D0%A1%D1%82%D0%BE%D0%BB%D0%B1%D1%87%D0%B0%D1%82%D1%8B%D0%B9._%D0%9F%D0%BE%D1%81%D0%BB%D0%B5_%D0%B7%D0%B0%D0%BA%D0%B0%D1%82%D0%B0.jpg
   :alt: Columnar basalt formations at Cape Stolbchaty, Kunashir Island, after sunset
   :width: 100%

   *Columnar basalt prisms are formed as lava cools rapidly and develops polygonal fractures.*
   Photo by `Ekaterina Vasyagina <https://commons.wikimedia.org/wiki/File:%D0%9C%D1%8B%D1%81_%D0%A1%D1%82%D0%BE%D0%BB%D0%B1%D1%87%D0%B0%D1%82%D1%8B%D0%B9._%D0%9F%D0%BE%D1%81%D0%BB%D0%B5_%D0%B7%D0%B0%D0%BA%D0%B0%D1%82%D0%B0.jpg>`__, licensed under `CC BY-SA 4.0 <https://creativecommons.org/licenses/by-sa/4.0/>`__.

Basalt is a Python meshing library for nuclear workflows. It wraps the
commercial `Simmetrix SimModSuite <https://www.simmetrix.com/>`__
library through C++ bindings, ingests Parasolid CAD assemblies,
performs non-manifold imprinting, generates surface and volume
meshes, and exports them as Gmsh ``.msh`` files annotated for use
with `Stellarmesh <https://github.com/Thea-Energy/stellarmesh>`__
and DAGMC.

.. note::

   Basalt requires a valid `Simmetrix SimModSuite
   <https://www.simmetrix.com/>`__ license and the corresponding
   module distribution (``gmcore``, ``mscore``, ``pskrnl``,
   ``simlicense``). The library cannot be built or run without
   these. See `Installation <install.html>`__ for details.

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

   import basalt as bslt

   model = bslt.Model.from_parasolid_file("geometry.x_t")
   nm_model = model.make_non_manifold_model()
   mesh_case = bslt.MeshCase(nm_model)
   mesh_case.set_size(0.1)
   surface_mesh = bslt.SurfaceMesh.from_model(nm_model, mesh_case)
   volume_mesh = bslt.VolumeMesh.from_surface_mesh(surface_mesh)
   volume_mesh.write_msh("output.msh")

----------------
Try the tutorial
----------------

For an end-to-end neutronics workflow against the public W7-X
stellarator fixture — from Parasolid CAD through basalt meshing,
stellarmesh DAGMC conversion, and an OpenMC fixed-source DT-neutron
calculation with four tallies and a 3D PyVista flux render — see the
`W7-X neutronics tutorial
<https://basalt.readthedocs.io/en/latest/notebooks/tutorials/w7x_neutronics.html>`__.

----------------
Acknowledgements
----------------

basalt is originally a project of Thea Energy, who are building the
world's first planar coil stellarator.

.. figure:: https://github.com/user-attachments/assets/37b9ba1c-b22c-4837-b226-a6212854127e
   :width: 200px
