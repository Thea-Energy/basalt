

class ModelItem:
    """Model entity"""

class Entity(ModelItem):
    @property
    def tag(self) -> int:
        """
        Get entity tag (GEN_tag)

        Returns:
            Tag
        """

    @property
    def related_parts(self) -> list[Part]:
        """
        Get related parts

        Returns:
            Related parts
        """

    @property
    def name(self) -> str | None:
        """
        Name

        Returns:
            Name or None
        """

    @property
    def native_attributes(self) -> dict:
        """
        Get all native Parasolid attributes stored on this entity.

        Returns a dict mapping each attribute name to a list of its values.
        Value types are Python int, float, or str.

        Returns:
            Dict of attribute name to list of values
        """

class Part(ModelItem):
    """Model part"""

    @property
    def name(self) -> str | None:
        """
        Name

        Returns:
            Name or None
        """

    @property
    def parent_assembly(self) -> Assembly | None:
        """
        Get the parent assembly

        Returns:
            Assembly or none
        """

    @property
    def regions(self) -> list[Region]:
        """
        Get regions of this part.

        Returns:
            Regions
        """

    @property
    def faces(self) -> list[Face]:
        """
        Get faces of this part.

        Returns:
            Faces
        """

    @property
    def native_attributes(self) -> dict:
        """
        Get all native Parasolid attributes stored on this part.

        Returns:
            Dict of attribute name to list of values
        """

    @property
    def centroid(self) -> list[float]:
        """
        Compute the centroid of this part's region.

        Returns:
            Centroid [x, y, z]
        """

class Assembly(ModelItem):
    """Model assembly"""

    @property
    def name(self) -> str | None:
        """
        Get the name

        Returns:
            Name or None
        """

    @property
    def parent_assembly(self) -> Assembly | None:
        """
        Get the parent assembly

        Returns:
            Assembly or None
        """

    @property
    def parts(self) -> list[Part]:
        """
        Get instantiated parts in this assembly.

        Returns:
            Parts
        """

    @property
    def assemblies(self) -> list[Assembly]:
        """
        Get sub-assemblies in this assembly.

        Returns:
            Sub-assemblies
        """

    @property
    def native_attributes(self) -> dict:
        """
        Get all native Parasolid attributes stored on this assembly.

        Returns:
            Dict of attribute name to list of values
        """

class Vertex(Entity):
    """Model vertex"""

class Edge(Entity):
    """Model edge"""

class Face(Entity):
    """Model face"""

    @property
    def forward_region(self) -> Region | None:
        """
        Get the region on the forward side of this face, if any.

        Returns:
            Forward region or None
        """

    @property
    def reverse_region(self) -> Region | None:
        """
        Get the region on the reverse side of this face, if any.

        Returns:
            Reverse region or None
        """

class Region(Entity):
    """Model region"""

    @property
    def centroid(self) -> list[float]:
        """
        Compute the centroid of this region.

        Returns:
            Centroid [x, y, z]
        """

class Model:
    """model"""

    @property
    def root_items(self) -> list[ModelItem]:
        """
        Get model root items.

        Returns:
            Root items.
        """

    def is_valid(self) -> bool:
        """
        Whether this model is topologically and geometrically valid (GM_isValid).

        Returns:
            True if valid.
        """

    def write(self, filename: str) -> None:
        """
        Write to SMS native file .smd

        Args:
            filename: Filename
        """

    @staticmethod
    def read(filename: str) -> Model:
        """
        Read from SMS native file .smd

        Args:
            filename: Filename

        Returns:
            Model
        """

    @staticmethod
    def from_parasolid_file(filename: str, load_nx_attrs: bool = False) -> Model:
        """
        Load a GAM assembly model from a Parasolid file (stage 1 of 2).
        Call translate() on the result to obtain the SMS model needed for meshing.

        Args:
            filename: Filename
            load_nx_attrs: If true, auto-detect <basename>_attrs.json alongside
                             the .x_t and load NX component attributes as native
                             Parasolid attributes on each Part/Assembly.

        Returns:
            GAM assembly model
        """

    def translate(self) -> Model:
        """
        Translate this GAM assembly model into an SMS model (stage 2 of 2).
        This is where geometry validation occurs; invalid faces will raise here.

        Returns:
            Translated SMS model
        """

    def make_non_manifold_model(self, tolerance: float | None = None) -> Model:
        """
        Make nonmanifold model from assembly model

        Args:
            tolerance: Boolean tolerance other than default. Defaults to None.

        Returns:
            NonManifold model
        """

    def mesh(self, filename: str, mesh_size: float) -> None:
        """
        Mesh

        Args:
            filename: Filename
            mesh_size: Relative mesh size
        """

    @property
    def faces(self) -> list[Face]:
        """
        Return all the faces in this model.

        Returns:
            Faces
        """

    @property
    def regions(self) -> list[Region]:
        """
        Return all the regions in this model.

        Returns:
            Regions
        """

    @property
    def edges(self) -> list[Edge]:
        """
        Return all the edges in this model.

        Returns:
            Edges
        """

    @property
    def vertices(self) -> list[Vertex]:
        """
        Return all the vertices in this model.

        Returns:
            Vertices
        """

class MeshCase:
    """Mesh attributes"""

    def __init__(self, model: Model) -> None:
        """
        Constructor

        Args:
            model: Model

        Returns:
            Mesh case
        """

    def set_size(self, mesh_size: float, model_item: ModelItem | None = None) -> None:
        """
        Set the mesh size

        Args:
            mesh_size: Mesh size
            entity: Optional model entity
        """

    def set_curvature_refinement(self, value: float, relative: bool = False, use_edges: bool = False, min_size: float | None = None, anisotropic: bool = False, model_item: ModelItem | None = None) -> None:
        """
        Set the mesh curvature refinement.

        Args:
            value: Value of curvature refinement.
            relative: Use relative rather than absolute value. Defaults to
        False.
            use_edges: Consider edge curvature in addition to face curvature.
        Defaults to False.
            min_size: Min allowable refinement size. Defaults to None.
            anisotropic: Whether refinement is anisotropic. Defaults to False.
            entity: Optional model entity
        """

    def set_proximity_refinement(self, value: float, min_size: float | None = None, model_item: ModelItem | None = None) -> None:
        """
        Set proximity refinement for thin sections.

        Args:
            value: Mesh size will be set to (section thickness) / value.
            min_size: Min allowable refinement size. Defaults to None.
            model_item: Optional model entity
        """

    def set_gradation_rate(self, rate: float) -> None:
        """
        Set the global mesh size gradation rate.

        Controls how quickly element size grows from finer to coarser regions.
        Smaller values give a slower, smoother transition. Wraps
        MS_setGlobalSizeGradationRate.

        Args:
            rate: Gradation rate.
        """

    def set_no_mesh(self, model_item: ModelItem) -> None:
        """
        Exclude a model entity (e.g. a region/body) and its closure from meshing.

        Use to mesh only a subset of an assembly: mark the bodies you do not want
        meshed. Wraps MS_setNoMesh with closure enabled.

        Args:
            model_item: Model entity to exclude (Region, Face, ...).
        """

class Mesh:
    """Mesh attributes"""

    def write_msh(self, filename: str, scale_factor: float = 1.0) -> None:
        """
        Write mesh to Gmsh .msh file.

        Args:
            filename: Filename
        """

class SurfaceMesh(Mesh):
    """Surface Mesh"""

    @staticmethod
    def from_model(model: Model, mesh_case: MeshCase) -> Mesh:
        """
        Surface mesh

        Args:
            model: Model
            mesh_case: Mesh case

        Returns:
            Mesh
        """

class VolumeMesh(Mesh):
    """Volume Mesh"""

    @staticmethod
    def from_surface_mesh(surface_mesh: SurfaceMesh) -> VolumeMesh:
        """
        Create from surface mesh

        Args:
            surface_mesh: Surface mesh

        Returns:
            Volume Mesh
        """
