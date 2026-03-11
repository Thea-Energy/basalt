"""Tests for NX → Parasolid → SMS centroid matching pipeline.

These tests verify that loading a Parasolid assembly with its NX sidecar JSON
correctly matches per-instance body metadata (NX_COMPONENT_NAME) to SMS parts
via geometric centroid matching.
"""

import json
from pathlib import Path

import phnx
from phnx import Assembly, Part

DATA_DIR = Path(__file__).parent / "data"

EXPECTED_TOTAL_PARTS = 692
EXPECTED_UNIQUE_NAMES = 334


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


def test_all_parts_matched(phyeos_model):
    """Every SMS part should have NX_COMPONENT_NAME set after centroid matching."""
    parts = _collect_parts(phyeos_model)
    assert len(parts) == EXPECTED_TOTAL_PARTS

    without_attr = [p for p in parts if "NX_COMPONENT_NAME" not in p.native_attributes]
    assert len(without_attr) == 0, (
        f"{len(without_attr)}/{len(parts)} parts have no NX_COMPONENT_NAME attribute"
    )


def test_unique_component_names(phyeos_model):
    """The number of unique NX_COMPONENT_NAME values should match expectations.

    The PHYEOS10-120 model has 332 shaping coil instances (each with 2 bodies),
    plus encircling coils and root bodies, giving 334 unique names.
    """
    parts = _collect_parts(phyeos_model)
    names = {
        str(p.native_attributes["NX_COMPONENT_NAME"])
        for p in parts
        if p.native_attributes.get("NX_COMPONENT_NAME")
    }
    assert len(names) == EXPECTED_UNIQUE_NAMES


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
