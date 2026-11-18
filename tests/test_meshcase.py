"""Tests for new MeshCase mesh-size controls (gradation + body exclusion)."""

import os
from urllib.parse import parse_qs, quote, urlencode

import gmsh
import pytest

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


def _msh_entities(path):
    """(dim, tag) -> (physical-name fields, element count) for each entity."""
    gmsh.initialize()
    try:
        gmsh.option.setNumber("General.Terminal", 0)
        gmsh.open(path)
        entities = {}
        for dim, tag in gmsh.model.getEntities():
            groups = gmsh.model.getPhysicalGroupsForEntity(dim, tag)
            name = gmsh.model.getPhysicalName(dim, groups[0]) if len(groups) else ""
            fields = {k: v[0] for k, v in parse_qs(name).items()}
            _, elem_tags, _ = gmsh.model.mesh.getElements(dim, tag)
            entities[dim, tag] = (fields, sum(len(t) for t in elem_tags))
        return entities
    finally:
        gmsh.finalize()


def _assert_face_volumes_written(entities):
    """Every forward_volume/reverse_volume a face names is a written volume."""
    volumes = {tag for dim, tag in entities if dim == 3}
    for (dim, tag), (fields, _) in entities.items():
        if dim != 2:
            continue
        for side in ("forward_volume", "reverse_volume"):
            assert fields[side] == "None" or int(fields[side]) in volumes, (tag, side)


def test_surface_mesh_writes_volumes(w7x_model):
    """A surface mesh keeps every region as an element-free volume."""
    nm = w7x_model.make_non_manifold_model()
    mc = basalt.MeshCase(nm)
    mc.set_size(0.3)
    sm = basalt.SurfaceMesh.from_model(nm, mc)
    out = "/tmp/basalt_surface_volumes_test.msh"
    sm.write_msh(out, scale_factor=1.0)

    entities = _msh_entities(out)
    volumes = {tag: n for (dim, tag), (_, n) in entities.items() if dim == 3}
    assert set(volumes) == {r.tag for r in nm.regions}
    assert all(n == 0 for n in volumes.values())
    _assert_face_volumes_written(entities)


def test_write_msh_omits_unmeshed_entities(w7x_model):
    """Excluded entities are dropped; volumes named by written faces are kept."""
    nm, mc, regions = _single_region_meshcase(w7x_model)
    mc.set_size(0.3)
    sm = basalt.SurfaceMesh.from_model(nm, mc)
    vm = basalt.VolumeMesh.from_surface_mesh(sm)
    out = "/tmp/basalt_unmeshed_entities_test.msh"
    vm.write_msh(out, scale_factor=1.0)

    entities = _msh_entities(out)
    assert all(n > 0 for (dim, _), (_, n) in entities.items() if dim < 3)
    volumes = {tag: n for (dim, tag), (_, n) in entities.items() if dim == 3}
    assert volumes.pop(regions[0].tag) > 0
    assert all(n == 0 for n in volumes.values())
    assert len(volumes) < len(regions) - 1, "every excluded region was written"
    _assert_face_volumes_written(entities)


def test_set_domain_meshes_only_selected_face(w7x_model):
    """set_domain([face]) meshes only that face and its closure."""
    nm = w7x_model.make_non_manifold_model()
    face = nm.faces[0]
    mc = basalt.MeshCase(nm)
    mc.set_size(0.3)
    mc.set_domain([face])
    sm = basalt.SurfaceMesh.from_model(nm, mc)
    out = "/tmp/basalt_set_domain_test.msh"
    sm.write_msh(out, scale_factor=1.0)

    entities = _msh_entities(out)
    assert [tag for dim, tag in entities if dim == 2] == [face.tag]
    assert {tag for dim, tag in entities if dim == 1} == {e.tag for e in face.edges}
    assert all(n > 0 for (dim, _), (_, n) in entities.items() if dim < 3)
    fields = entities[2, face.tag][0]
    sides = {fields[s] for s in ("forward_volume", "reverse_volume")} - {"None"}
    volumes = {tag: n for (dim, tag), (_, n) in entities.items() if dim == 3}
    assert set(volumes) == {int(s) for s in sides}
    assert all(n == 0 for n in volumes.values())


def test_set_domain_rejects_empty(w7x_model):
    """set_domain([]) is an error: an empty domain would mesh nothing."""
    nm = w7x_model.make_non_manifold_model()
    with pytest.raises(RuntimeError):
        basalt.MeshCase(nm).set_domain([])


def test_set_surface_mesh_type_all_quad(w7x_model):
    """set_surface_mesh_type(3) yields only 4-node quads (gmsh type 3)."""
    nm = w7x_model.make_non_manifold_model()
    face = nm.faces[0]
    mc = basalt.MeshCase(nm)
    mc.set_size(0.3)
    mc.set_domain([face])
    mc.set_surface_mesh_type(3)
    sm = basalt.SurfaceMesh.from_model(nm, mc)
    out = "/tmp/basalt_all_quad_test.msh"
    sm.write_msh(out, scale_factor=1.0)

    gmsh.initialize()
    try:
        gmsh.option.setNumber("General.Terminal", 0)
        gmsh.open(out)
        elem_types, elem_tags, node_tags = gmsh.model.mesh.getElements(2, face.tag)
        assert list(elem_types) == [3]
        assert len(elem_tags[0]) > 0
        assert len(node_tags[0]) == 4 * len(elem_tags[0])
    finally:
        gmsh.finalize()


def test_set_surface_mesh_type_rejects_unknown(w7x_model):
    """Types outside 1-3 are refused (4/5 are thin-section variants)."""
    nm = w7x_model.make_non_manifold_model()
    with pytest.raises(RuntimeError):
        basalt.MeshCase(nm).set_surface_mesh_type(4)


def test_write_msh_rejects_non_tetrahedra(w7x_model):
    """A volume mesh on a quad surface mesh has pyramids; write_msh refuses it."""
    nm, mc, _ = _single_region_meshcase(w7x_model)
    mc.set_size(0.3)
    # Quad-dominant: all-quad fails on this region at this size (SMS error 1102).
    mc.set_surface_mesh_type(2)
    sm = basalt.SurfaceMesh.from_model(nm, mc)
    vm = basalt.VolumeMesh.from_surface_mesh(sm)
    with pytest.raises(RuntimeError, match="not tetrahedra"):
        vm.write_msh("/tmp/basalt_non_tetrahedra_test.msh", scale_factor=1.0)


def test_write_msh_names_round_trip(w7x_model):
    """Every physical name is a URL query that re-encodes to itself and has a tag."""
    nm, mc, _ = _single_region_meshcase(w7x_model)
    mc.set_size(0.3)
    sm = basalt.SurfaceMesh.from_model(nm, mc)
    vm = basalt.VolumeMesh.from_surface_mesh(sm)
    out = "/tmp/basalt_names_round_trip_test.msh"
    vm.write_msh(out, scale_factor=1.0)

    gmsh.initialize()
    try:
        gmsh.open(out)
        names = [
            gmsh.model.getPhysicalName(dim, tag)
            for dim, tag in gmsh.model.getPhysicalGroups()
        ]
    finally:
        gmsh.finalize()

    assert names
    for name in names:
        parsed = parse_qs(name, keep_blank_values=True, strict_parsing=True)
        assert all(len(v) == 1 for v in parsed.values()), name
        reencoded = urlencode(
            {k: v[0] for k, v in parsed.items()}, safe="", quote_via=quote
        )
        assert reencoded == name
        assert "tag" in parsed, name
