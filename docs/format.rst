======
Format
======

.. module:: basalt
   :no-index:

Basalt emits Gmsh ``.msh`` v4.1 ASCII files conforming to the
`stellarmesh format specification
<https://stellarmesh.readthedocs.io/en/latest/format.html>`__.
That page is the canonical schema; this page documents Basalt's
**producer-side** semantics — what values Basalt populates for each
schema field, the coordinate units it writes, and the
:py:meth:`Mesh.write_msh` API surface.

-------------------------
Material name resolution
-------------------------

Basalt populates each volume's ``material=<slug>`` field by walking
the originating CAD attributes in this order, taking the first
non-empty value:

1. ``DB_PART_NAME`` native attribute on the part (set by the NX
   export pipeline from the NX part's DB part number).
2. :py:attr:`Part.name`.
3. Parent :py:attr:`Assembly.name`.

The :py:meth:`Model.from_parasolid_file` ``material_namer`` callback
overrides this resolution — set it for cases where the CAD attributes
do not encode usable names.

See :doc:`nx` for how CAD attributes become the body slug.

Slug length is bounded to **28 characters** — a hard MOAB limit
applied downstream when stellarmesh converts the ``.msh`` into a
DAGMC ``.h5m`` file. :py:meth:`Mesh.write_msh` does not enforce this
at write time; the limit surfaces as a stellarmesh error if violated.

-------------------------------------------------------
The ``material`` slug identifies a body, not a material
-------------------------------------------------------

A common pitfall: the ``material=<slug>`` field encodes a slug that
uniquely identifies one **body** in the source CAD assembly. It does
**not** identify a *physical material* by itself.

Consumers (typically OpenMC, after stellarmesh has rendered the
``.h5m``) are responsible for the slug → ``openmc.Material`` mapping.
The W7-X tutorial
(:doc:`notebooks/tutorials/w7x_neutronics`) walks through one such
mapping end-to-end.

This is intentional: the same physical material (e.g. "stainless
steel 316L") may be assigned to dozens of bodies, and the producer
has no way of knowing which assemblies are intended to share material
identity. Pushing that mapping to the consumer keeps Basalt's output
faithful to the CAD source.

------
Units
------

Basalt writes coordinates in **metres** (the Simmetrix SimModSuite
native unit) by default. Downstream consumers vary:

* DAGMC and OpenMC expect **centimetres**.
* Some Gmsh-based tools work in metres.

Use the ``scale_factor`` argument of :py:meth:`Mesh.write_msh` to
rescale at write time:

.. code:: python

   volume_mesh.write_msh("output.msh", scale_factor=100.0)  # m → cm

A scale factor of ``1.0`` (the default) preserves metres. Other
factors apply uniformly to every node coordinate.

-----------
Stability
-----------

Basalt commits to producing files that conform to the stellarmesh
format spec linked above. Basalt does **not** unilaterally extend the
URL-style key set — if a new key is needed (e.g. to encode a new
piece of CAD metadata), the schema change goes into stellarmesh
first.

What is **not** part of Basalt's commitment:

* In-memory Python object shapes (``basalt.Model``, ``basalt.Mesh``,
  etc.). These are versioned by Basalt's release process and may
  change between releases.
* Log line formats and verbosity.
* The sidecar JSON schema (``*_attrs.json``); see the source for
  that contract.
* The C++ ABI exposed by ``libbasalt.so`` or the nanobind bindings.
* Internal Gmsh tag numbering. Tags are guaranteed unique and stable
  within one written file; Basalt makes no promise about consistency
  across re-runs of the pipeline, or across Basalt versions.
