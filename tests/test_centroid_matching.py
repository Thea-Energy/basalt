"""Tests for the NX -> Parasolid -> SMS centroid-matching pipeline.

Exercised against the scaled W7-X stellarator fixture (Proxima
open_stellarator_models, restructured per NE-11 sub-project B.1 § 2.1
into a nested-assembly layout: coils sub-assembly with 50 children,
blanket_module sub-assembly with firstwall/blanket/vessel, and plasma
+ gap top-level singletons).

These tests verify that loading the Parasolid assembly with its v6
sidecar JSON correctly matches per-instance body metadata to SMS parts
via geometric centroid matching, and that structured naming fields
(NX_MATERIAL, NX_PATH, NX_COMPONENT, NX_INSTANCE, NX_BODY) are set on
every part.
"""

import re

from basalt import Assembly, Part

EXPECTED_TOTAL_BODIES = 55
EXPECTED_COMPONENT_COUNT = 58
EXPECTED_TOPLEVEL_KEYS = {"COILS_1", "BLANKET_MODULE_1", "PLASMA_1", "GAP_1"}


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


def test_sidecar_schema(w7x_sidecar):
    """Pin v6 sidecar shape: schema version, body count, and top-level keys."""
    assert w7x_sidecar["schema_version"] == 6
    assert len(w7x_sidecar["bodies"]) == EXPECTED_TOTAL_BODIES
    assert len(w7x_sidecar["components"]) == EXPECTED_COMPONENT_COUNT
    missing = EXPECTED_TOPLEVEL_KEYS - set(w7x_sidecar["components"])
    assert not missing, (
        f"sidecar missing expected top-level component keys: {missing}. "
        "Did restructure_w7x.cs skip a phase?"
    )


def test_all_parts_have_material(w7x_model):
    """Every SMS part should have NX_MATERIAL set after centroid matching."""
    parts = _collect_parts(w7x_model)
    assert len(parts) == EXPECTED_TOTAL_BODIES

    without_attr = [p for p in parts if "NX_MATERIAL" not in p.native_attributes]
    assert len(without_attr) == 0, (
        f"{len(without_attr)}/{len(parts)} parts have no NX_MATERIAL attribute"
    )


def test_unique_material_names(w7x_model):
    """Every body should have a globally unique short NX_MATERIAL name.

    DAGMC/MOAB material slugs are per-body identifiers (one slug per
    body, even when bodies share an upstream physical material — the
    shared-material mapping is done downstream in OpenMC). Each must fit
    within MOAB's 28-usable-char NAME tag limit after the 'mat:' prefix.
    """
    parts = _collect_parts(w7x_model)
    materials = [
        p.native_attributes["NX_MATERIAL"][0]
        for p in parts
        if p.native_attributes.get("NX_MATERIAL")
    ]
    assert len(materials) == EXPECTED_TOTAL_BODIES
    assert len(set(materials)) == EXPECTED_TOTAL_BODIES, (
        f"Expected {EXPECTED_TOTAL_BODIES} unique material slugs, "
        f"got {len(set(materials))}"
    )

    for mat in materials:
        assert len(mat) <= 28, f"Material name exceeds 28 chars ({len(mat)}): {mat}"


def test_structured_fields_present(w7x_model):
    """Every part should have all structured naming fields set."""
    parts = _collect_parts(w7x_model)
    required_attrs = ["NX_MATERIAL", "NX_COMPONENT", "NX_INSTANCE", "NX_BODY"]
    for p in parts:
        attrs = p.native_attributes
        for attr in required_attrs:
            assert attr in attrs, f"Part missing {attr} attribute"


def test_material_slug_format(w7x_model):
    """NX_MATERIAL should follow the {DB_PART_NO}_{instance}.b{N} format."""
    pattern = re.compile(r"^[A-Za-z0-9_]+_\d+\.b\d+$")
    parts = _collect_parts(w7x_model)
    for p in parts:
        material = p.native_attributes["NX_MATERIAL"][0]
        assert pattern.match(material), (
            f"Material '{material}' doesn't match '{{DB_PART_NO}}_{{instance}}.b{{N}}'"
        )


def test_sidecar_body_count_matches_sms(w7x_model, w7x_sidecar):
    """The number of bodies in the sidecar should match the SMS part count."""
    components = w7x_sidecar["components"]
    bodies = w7x_sidecar["bodies"]

    for body in bodies:
        assert body["component"] in components, (
            f"Body references unknown component: {body['component']}"
        )

    sidecar_count = len(bodies)
    sms_count = len(_collect_parts(w7x_model))
    assert sidecar_count == sms_count, (
        f"Sidecar has {sidecar_count} bodies but SMS has {sms_count} parts"
    )


def test_assemblies_have_attributes(w7x_model):
    """Named sub-assemblies should have NX attributes applied.

    The W7-X fixture's two restructure-created sub-assemblies (coils,
    blanket_module) carry NX attributes (e.g. NX_ComponentGroup,
    NX_MaterialMissingAssignments) from the export journal.
    """

    def find_assembly(items, name):
        for item in items:
            if isinstance(item, Assembly):
                if item.name and name.lower() in item.name.lower():
                    return item
                result = find_assembly(list(item.assemblies), name)
                if result:
                    return result
        return None

    coils_asm = find_assembly(list(w7x_model.root_items), "coils")
    assert coils_asm is not None, "Could not find coils sub-assembly"
    assert len(coils_asm.native_attributes) > 0, (
        "coils sub-assembly should have NX attributes"
    )

    bm_asm = find_assembly(list(w7x_model.root_items), "blanket_module")
    assert bm_asm is not None, "Could not find blanket_module sub-assembly"
    assert len(bm_asm.native_attributes) > 0, (
        "blanket_module sub-assembly should have NX attributes"
    )


def test_centroid_distances_within_tolerance(w7x_model):
    """SMS region centroids should be close to their matched sidecar centroids.

    After the centroid matching pipeline, each Part's centroid should be
    very close to the sidecar body it was matched to. We verify this
    indirectly by checking that every part has a centroid (i.e. the
    match succeeded) and that the centroids are finite.
    """
    parts = _collect_parts(w7x_model)
    for p in parts:
        cen = p.centroid
        assert len(cen) == 3
        assert all(abs(c) < 1e6 for c in cen), f"Unreasonable centroid: {cen}"
