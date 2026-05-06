"""Tests for the Type 2 (leaf .prt) export path.

nnnnnnn is a flat part exported from NX/TeamCenter with all bodies at the
display-part root (no component instances). The journal's collect_bodies_flat
path synthesizes one component record, and centroid matching attaches metadata
to each SMS Part.
"""

import json
import re
from pathlib import Path

from phnx import Assembly, Part

DATA_DIR = Path(__file__).parent / "data"
SIDECAR_PATH = DATA_DIR / "0012808_attrs.json"


def _collect_parts(model):
    """Recursively collect all Parts from the model hierarchy."""
    parts = []

    def walk(item):
        if isinstance(item, Assembly):
            for sub in item.assemblies:
                walk(sub)
            parts.extend(item.parts)
        elif isinstance(item, Part):
            parts.append(item)

    for item in model.root_items:
        walk(item)
    return parts


def _load_sidecar():
    with open(SIDECAR_PATH) as f:
        return json.load(f)


def test_sidecar_has_single_component():
    """Type 2 flat parts produce exactly one synthetic component."""
    sidecar = _load_sidecar()
    assert sidecar["schema_version"] == 6
    assert len(sidecar["components"]) == 1


def test_sidecar_bodies_reference_single_component():
    """Every body record must point at the single synthetic component."""
    sidecar = _load_sidecar()
    (only_key,) = sidecar["components"].keys()
    for body in sidecar["bodies"]:
        assert body["component"] == only_key


def test_sidecar_bodies_have_required_fields():
    """Every body record has centroid/component/body_name/body_index."""
    required = {"centroid", "component", "body_name", "body_index"}
    for body in _load_sidecar()["bodies"]:
        missing = required - set(body.keys())
        assert not missing, f"Body missing fields: {missing}"


def test_sidecar_body_names_are_descriptive():
    """body_name should be the user-assigned name, not LINKED_BODY(N)."""
    for body in _load_sidecar()["bodies"]:
        assert not body["body_name"].startswith("LINKED_BODY"), (
            f"Expected descriptive name, got {body['body_name']!r}"
        )


def test_gam_hierarchy_is_one_assembly_with_flat_parts(port_plug_model):
    """GAM represents a flat .prt as one root Assembly with anonymous Parts."""
    roots = list(port_plug_model.root_items)
    assert len(roots) == 1
    assert isinstance(roots[0], Assembly)
    parts = _collect_parts(port_plug_model)
    sidecar_body_count = len(_load_sidecar()["bodies"])
    assert len(parts) == sidecar_body_count, (
        f"Expected {sidecar_body_count} Parts, got {len(parts)}"
    )


def test_all_parts_have_nx_material(port_plug_model):
    """After centroid matching, every Part carries NX_MATERIAL."""
    parts = _collect_parts(port_plug_model)
    without = [p for p in parts if "NX_MATERIAL" not in p.native_attributes]
    assert not without, f"{len(without)}/{len(parts)} Parts missing NX_MATERIAL"


def test_material_slug_format(port_plug_model):
    """NX_MATERIAL follows the {DB_PART_NO}_{instance}.b{N} format."""
    pattern = re.compile(r"^[A-Za-z0-9]+_\d+\.b\d+$")
    parts = _collect_parts(port_plug_model)
    for p in parts:
        material = p.native_attributes["NX_MATERIAL"][0]
        assert pattern.match(material), f"Material {material!r} does not match format"


def test_material_slugs_unique_and_within_moab_limit(port_plug_model):
    """Each Part gets a globally unique material slug <= 28 chars."""
    parts = _collect_parts(port_plug_model)
    slugs = [p.native_attributes["NX_MATERIAL"][0] for p in parts]
    assert len(slugs) == len(set(slugs))
    for s in slugs:
        assert len(s) <= 28, f"Slug exceeds 28 chars: {s}"


def test_structured_fields_present(port_plug_model):
    """Every Part has NX_MATERIAL, NX_COMPONENT, NX_INSTANCE, NX_BODY."""
    required_attrs = ["NX_MATERIAL", "NX_COMPONENT", "NX_INSTANCE", "NX_BODY"]
    parts = _collect_parts(port_plug_model)
    for p in parts:
        attrs = p.native_attributes
        for key in required_attrs:
            assert key in attrs, f"Part missing {key}"


def test_instance_is_1_for_flat_part(port_plug_model):
    """Type 2 synthetic component uses instance='1'."""
    parts = _collect_parts(port_plug_model)
    for p in parts:
        assert p.native_attributes["NX_INSTANCE"][0] == "1"


def test_centroids_finite(port_plug_model):
    """Every Part has a finite 3-vector centroid after matching."""
    for p in _collect_parts(port_plug_model):
        cen = p.centroid
        assert len(cen) == 3
        assert all(abs(c) < 1e6 for c in cen), f"Unreasonable centroid: {cen}"
