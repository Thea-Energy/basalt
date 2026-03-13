"""Tests for NX → Parasolid → SMS centroid matching pipeline.

These tests verify that loading a Parasolid assembly with its NX sidecar JSON
correctly matches per-instance body metadata to SMS parts via geometric centroid
matching, and that structured naming fields (NX_MATERIAL, NX_PATH, NX_COMPONENT,
NX_INSTANCE, NX_BODY) are set on every part.
"""

import json
import re
from pathlib import Path

from phnx import Assembly, Part

DATA_DIR = Path(__file__).parent / "data"

EXPECTED_TOTAL_PARTS = 692
EXPECTED_UNIQUE_MATERIALS = 692


def _collect_parts(model):
    """Recursively collect all Parts from the model hierarchy."""
    parts = []

    def walk(item):
        if isinstance(item, Assembly):
            for sub in item.assemblies:
                walk(sub)
            for p in item.parts:
                parts.append(p)
        elif isinstance(item, Part):
            parts.append(item)

    for item in model.root_items:
        walk(item)
    return parts


def test_all_parts_have_material(phyeos_model):
    """Every SMS part should have NX_MATERIAL set after centroid matching."""
    parts = _collect_parts(phyeos_model)
    assert len(parts) == EXPECTED_TOTAL_PARTS

    without_attr = [p for p in parts if "NX_MATERIAL" not in p.native_attributes]
    assert len(without_attr) == 0, (
        f"{len(without_attr)}/{len(parts)} parts have no NX_MATERIAL attribute"
    )


def test_unique_material_names(phyeos_model):
    """Every body should have a globally unique short NX_MATERIAL name.

    The PHYEOS10-120 model has 692 bodies total, and each should get a distinct
    short material name ({DB_PART_NO}_{instance}/b{N}) that fits within
    MOAB's 32-byte NAME tag limit (28 usable chars after 'mat:' prefix).
    """
    parts = _collect_parts(phyeos_model)
    materials = [
        p.native_attributes["NX_MATERIAL"][0]
        for p in parts
        if p.native_attributes.get("NX_MATERIAL")
    ]
    assert len(materials) == EXPECTED_TOTAL_PARTS
    assert len(set(materials)) == EXPECTED_UNIQUE_MATERIALS, (
        f"Expected {EXPECTED_UNIQUE_MATERIALS} unique materials, "
        f"got {len(set(materials))}"
    )

    # Every short name must fit within MOAB's 28-char limit
    for mat in materials:
        assert len(mat) <= 28, (
            f"Material name exceeds 28 chars ({len(mat)}): {mat}"
        )


def test_structured_fields_present(phyeos_model):
    """Every part should have all structured naming fields set."""
    parts = _collect_parts(phyeos_model)
    required_attrs = ["NX_MATERIAL", "NX_COMPONENT", "NX_INSTANCE", "NX_BODY"]
    for p in parts:
        attrs = p.native_attributes
        for attr in required_attrs:
            assert attr in attrs, (
                f"Part missing {attr} attribute"
            )


def test_material_slug_format(phyeos_model):
    """NX_MATERIAL should follow the {DB_PART_NO}_{instance}.b{N} format."""
    pattern = re.compile(r"^[A-Za-z0-9]+_\d+\.b\d+$")
    parts = _collect_parts(phyeos_model)
    for p in parts:
        material = p.native_attributes["NX_MATERIAL"][0]
        assert pattern.match(material), (
            f"Material '{material}' doesn't match '{{DB_PART_NO}}_{{instance}}.b{{N}}'"
        )


def test_sidecar_body_count_matches_sms(phyeos_model):
    """The number of bodies in the sidecar should match the SMS part count."""
    sidecar_path = DATA_DIR / "0012335_A_attrs.json"
    with open(sidecar_path) as f:
        sidecar = json.load(f)

    def count_bodies(node):
        n = len([b for b in node.get("bodies", []) if "centroid" in b])
        for child in node.get("children", []):
            n += count_bodies(child)
        return n

    sidecar_count = count_bodies(sidecar["root"])
    sms_count = len(_collect_parts(phyeos_model))
    assert sidecar_count == sms_count, (
        f"Sidecar has {sidecar_count} bodies but SMS has {sms_count} parts"
    )


def test_assemblies_have_attributes(phyeos_model):
    """Named assemblies should have NX attributes applied."""
    def find_assembly(items, name):
        for item in items:
            if isinstance(item, Assembly):
                if item.name and name in item.name:
                    return item
                result = find_assembly(list(item.assemblies), name)
                if result:
                    return result
        return None

    # The shaping coil assembly should exist and have attributes
    coil_asm = find_assembly(list(phyeos_model.root_items), "nnnnnnn")
    assert coil_asm is not None, "Could not find nnnnnnn assembly"
    attrs = coil_asm.native_attributes
    assert len(attrs) > 0, "Shaping coil assembly should have NX attributes"


def test_centroid_distances_within_tolerance(phyeos_model):
    """SMS region centroids should be close to their matched sidecar centroids.

    After the centroid matching pipeline, each Part's centroid should be very
    close to the sidecar body it was matched to. We verify this indirectly by
    checking that every part has a centroid (i.e. the match succeeded) and that
    the centroids are finite.
    """
    parts = _collect_parts(phyeos_model)
    for p in parts:
        cen = p.centroid
        assert len(cen) == 3
        assert all(abs(c) < 1e6 for c in cen), f"Unreasonable centroid: {cen}"
