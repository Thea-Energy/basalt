"""Tests for new MeshCase mesh-size controls (gradation + body exclusion)."""

import basalt


def test_set_gradation_rate_accepts_float(w7x_model):
    """set_gradation_rate is bound and accepts a rate without raising."""
    nm = w7x_model.make_non_manifold_model()
    mc = basalt.MeshCase(nm)
    mc.set_size(0.1)
    mc.set_gradation_rate(1.3)  # global gradation; must not raise


def test_set_no_mesh_excludes_regions(w7x_model):
    """Excluding all-but-one region still meshes and writes a file.

    Mesh only the first region of the model (exclude the rest); assert meshing
    completes and produces a non-empty .msh.
    """
    nm = w7x_model.make_non_manifold_model()
    regions = nm.regions
    assert len(regions) > 1, "fixture should have multiple regions"

    mc = basalt.MeshCase(nm)
    mc.set_size(0.3)
    for r in regions[1:]:
        mc.set_no_mesh(r)  # exclude every region except regions[0]

    sm = basalt.SurfaceMesh.from_model(nm, mc)
    vm = basalt.VolumeMesh.from_surface_mesh(sm)
    out = "/tmp/basalt_no_mesh_test.msh"
    vm.write_msh(out, scale_factor=1.0)
    import os

    assert os.path.getsize(out) > 0
