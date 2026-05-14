Test fixture: scaled W7-X stellarator
======================================

Source
------

This directory contains a derivative of the "scaled W7-X" model from
`Proxima Fusion open_stellarator_models`_ (MIT licensed), version 0.1.0,
released 2025-04-28.

.. _Proxima Fusion open_stellarator_models: https://github.com/proximafusion/open_stellarator_models

Files
-----

``scaled_w7x_stellarator.step``
   Upstream STEP file, verbatim (SHA256:
   ``384d025207b7d6ccc00bef364611526ef0c3093b091f6f4005fb8586ae4f34e3``).
   Tracked via git-LFS.

``scaled_w7x_stellarator.x_t``
   Parasolid export, **derived** -- see "Derivation" below. Tracked via git-LFS.

``scaled_w7x_stellarator_attrs.json``
   v6 sidecar (component metadata + body centroids + NX attributes),
   produced alongside the ``.x_t``. Raw (not LFS).

Derivation
----------

The committed ``.x_t`` and ``_attrs.json`` are **not** a journal-only
export of the upstream STEP. Before export, the NX assembly was
restructured (see
``docs/superpowers/specs/2026-05-08-NE-11-sub-project-B1-fixtures-design.md``
section 2.1) to produce a nested-assembly layout that exercises two
GAM topology patterns:

- The 50 individual ``COIL_*_STEP`` components are grouped under a new
  sub-assembly part ``coils.prt``. ``coils`` is one component at the
  root with 50 child components (one per coil), each pointing at its
  original ``coil_<n>_step.prt``.
- ``firstwall``, ``blanket``, and ``vessel`` are grouped under a new
  sub-assembly part ``blanket_module.prt``. ``blanket_module`` is one
  component at the root with three child components.
- ``plasma`` and ``gap`` remain top-level singletons.

Body count is unchanged at 55. The sidecar's ``components`` dict is a
**flat** map of every component visited during the recursive walk,
including container components that hold no bodies. With option B that
total is 58: one synthetic root entry (the displayed part itself,
keyed ``_1``), four top-level entries (``COILS_1``,
``BLANKET_MODULE_1``, ``PLASMA_1``, ``GAP_1``), 50 coil children under
``COILS``, and 3 children under ``BLANKET_MODULE``.

The two restructure-created parts (``coils.prt``, ``blanket_module.prt``)
are not committed separately — they live only inside the Parasolid
export above. Re-running the restructure journal regenerates them
fresh from the upstream STEP.

Regeneration is **not** runnable in CI (requires Siemens NX on Windows
plus a Simmetrix license). To regenerate from a new upstream STEP, on
a developer machine:

1. Download the upstream STEP into this directory.
2. Open it in NX (NX 2406 is the known-working version).
3. Set ``BASALT_NX_EXPORT_DIR`` to this directory's absolute path
   (Windows UNC path to the WSL checkout: use ``wslpath -wa`` to
   translate from the WSL path).
4. Run ``nx_journals/restructure_w7x.cs`` via the ``nx-bridge`` skill.
5. Run ``nx_journals/export_attrs.cs`` via the ``nx-bridge`` skill.

License
-------

The upstream STEP file and this directory's derivatives are MIT
licensed (Proxima Fusion). The inlined LICENSE and CITATION blocks
below are reproduced verbatim from the upstream repo
(commit of version 0.1.0):

LICENSE
~~~~~~~

::

   MIT License

   Copyright (c) 2025 proximafusion

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

CITATION
~~~~~~~~

::

   cff-version: 1.2.0
   message: "If you use this software, please cite it as below."
   authors:
   - family-names: "Proxima Fusion"
   title: "Open Stellarator Models"
   version: 0.1.0
   date-released: 2025-4-28
   url: "https://github.com/proximafusion/open_stellarator_models"
