"""Tests for new MeshCase mesh-size controls (gradation + body exclusion)."""

import os

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

    assert os.path.getsize(out) > 0


def _single_region_meshcase(w7x_model):
    """A MeshCase that meshes only the first region (keeps the test fast)."""
    nm = w7x_model.make_non_manifold_model()
    mc = basalt.MeshCase(nm)
    regions = nm.regions
    for r in regions[1:]:
        mc.set_no_mesh(r)
    return nm, mc, regions


def test_set_size_absolute(w7x_model):
    """set_size(relative=False) applies an absolute size and still meshes."""
    nm, mc, _ = _single_region_meshcase(w7x_model)
    mc.set_size(0.5, relative=False)  # absolute size (Simmetrix type 1)
    sm = basalt.SurfaceMesh.from_model(nm, mc)
    vm = basalt.VolumeMesh.from_surface_mesh(sm)
    out = "/tmp/basalt_size_absolute_test.msh"
    vm.write_msh(out, scale_factor=1.0)

    assert os.path.getsize(out) > 0


def test_add_point_refinement(w7x_model):
    """add_point_refinement is bound and meshes around a point of interest."""
    nm, mc, regions = _single_region_meshcase(w7x_model)
    mc.set_size(0.3)
    c = regions[0].centroid
    mc.add_point_refinement(0.05, [c[0], c[1], c[2]])
    sm = basalt.SurfaceMesh.from_model(nm, mc)
    vm = basalt.VolumeMesh.from_surface_mesh(sm)
    out = "/tmp/basalt_point_refinement_test.msh"
    vm.write_msh(out, scale_factor=1.0)

    assert os.path.getsize(out) > 0


def test_volume_mesh_enforce_size(w7x_model):
    """from_surface_mesh(enforce_size=1) runs the interior-enforcement path."""
    nm, mc, _ = _single_region_meshcase(w7x_model)
    mc.set_size(0.3)
    sm = basalt.SurfaceMesh.from_model(nm, mc)
    vm = basalt.VolumeMesh.from_surface_mesh(sm, enforce_size=1)
    out = "/tmp/basalt_enforce_size_test.msh"
    vm.write_msh(out, scale_factor=1.0)

    assert os.path.getsize(out) > 0
