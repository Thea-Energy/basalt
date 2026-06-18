=====================
Geometry sources & NX
=====================
.. module:: basalt
   :no-index:

Basalt meshes **Parasolid** assemblies. Any tool that exports Parasolid
(``.x_t``) works — Siemens NX is one common source, but it is entirely
optional.

.. code:: python

   import basalt as bslt

   model = bslt.Model.from_parasolid_file("geometry.x_t")

Optional: NX attributes
=======================

When the geometry comes from NX, an export journal writes a sidecar of
per-body metadata alongside the Parasolid file. Pass ``load_nx_attrs=True`` to
pick up a sibling ``*_attrs.json`` and apply it:

.. code:: python

   model = bslt.Model.from_parasolid_file("geometry.x_t", load_nx_attrs=True)

Run the journal inside NX against the open assembly; it exports both the
Parasolid body and the sidecar. Set ``BASALT_NX_EXPORT_DIR`` to the output
directory before running.

:download:`export_attrs.cs <_static/export_attrs.cs>`

Metadata flow
=============

Each body carries its CAD attributes — part number, part and body names,
material — through the pipeline as one stable **ID** (slug). The slug becomes
the DAGMC volume's group name, so every cell stays traceable from CAD to
transport:

.. code:: text

   Parasolid attributes        ID slug          DAGMC volume
   part 0012808, body 13   →   0012808.b13  →   group "mat:0012808.b13"

The slug identifies a *body*, not a physical material — consumers map slugs to
materials downstream. See :doc:`format` for the producer-side details.
