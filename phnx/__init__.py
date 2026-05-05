from ._core._core import (
    Mesh,
    MeshCase,
    Model,
    ModelItem,
    Entity,
    Assembly,
    Part,
    Region,
    Face,
    Edge,
    Vertex,
    SurfaceMesh,
    VolumeMesh,
)  # noqa: F401


def load_material_metadata(attrs_path: str) -> dict[str, dict]:
    """Load a v6 _attrs.json sidecar and return {material_slug: body_record}.

    Each returned record merges the body-specific fields (centroid, body_name,
    body_index) with all component-level fields (path, db_part_name, attributes,
    etc.) so the caller sees a flat dict per body -- same shape as the former v5
    API.
    """
    import json

    with open(attrs_path) as f:
        data = json.load(f)

    components = data["components"]
    result = {}
    for body in data["bodies"]:
        comp = components[body["component"]]
        material_slug = body["component"] + ".b" + str(body["body_index"])
        record = {**comp, **body, "material_slug": material_slug}
        result[material_slug] = record
    return result


def print_hierarchy(model: Model) -> None:
    """Print the assembly/part hierarchy of a model as an indented tree."""

    def _format_name(name, cls_label):
        display = name if name is not None else "<unnamed>"
        return f"{cls_label}: {display}"

    def _walk(item, indent=0):
        prefix = "  " * indent
        if isinstance(item, Assembly):
            print(f"{prefix}{_format_name(item.name, 'Assembly')}")
            for sub in item.assemblies:
                _walk(sub, indent + 1)
            for part in item.parts:
                _walk(part, indent + 1)
        elif isinstance(item, Part):
            print(f"{prefix}{_format_name(item.name, 'Part')}")
            for region in item.regions:
                _walk(region, indent + 1)
        elif isinstance(item, Region):
            print(f"{prefix}{_format_name(item.name, 'Region')}")

    for item in model.root_items:
        _walk(item)
