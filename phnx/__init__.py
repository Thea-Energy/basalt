from ._core._core import (
    Mesh,
    MeshCase,
    Model,
    Assembly,
    Part,
    SurfaceMesh,
    VolumeMesh,
    Region,
)  # noqa: F401


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
