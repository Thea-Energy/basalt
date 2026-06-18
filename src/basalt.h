#include "AttributeTypes.h"
#include "MeshSim.h"
#include "MeshTypes.h"
#include "ModelTypes.h"
#include "SimModel.h"
#include "nanobind/intrusive/counter.h"
#include <array>
#include <optional>
#include <string>

#include <nanobind/intrusive/ref.h>
#include <nanobind/nanobind.h>
#include <vector>

namespace nb = nanobind;

class Model;
class Part;
class Assembly;
class Region;
class Face;

/**
 * Push pPList to std::vector.
 *
 * Deletes list.
 *
 * @tparam T
 * @param plist
 * @return std::vector<T>
 */
template <typename T> auto plist_to_vec(pPList plist) -> std::vector<T>;

/**
 * @class ModelItem
 * @brief Model entity
 * @nb
 * @nb_intrusive_ptr
 */
class ModelItem : public nb::intrusive_base {
public:
  const pModelItem s_model_item;
  nb::ref<Model> model;

  ModelItem(pModelItem s_model_item, nb::ref<Model> model)
      : s_model_item(s_model_item), model(model) {};

  static auto downcast(pModelItem s_model_item, nb::ref<Model> model)
      -> nb::ref<ModelItem>;
};

/**
 * @class Entity
 * @nb
 * @nb_inherit ModelItem
 */
class Entity : public ModelItem {
public:
  using ModelItem::ModelItem;

  /**
   * Get entity tag (GEN_tag)
   *
   * @return Tag
   * @nb
   * @nb_prop_ro tag
   */
  auto get_tag() const -> int;

  /**
   * Get related parts
   *
   * @return Related parts
   * @nb
   * @nb_prop_ro related_parts
   */
  auto get_related_parts() const -> std::vector<nb::ref<Part>>;

  /**
   * Name
   *
   * @return Name or None
   * @nb
   * @nb_prop_ro name
   */
  auto get_name() const -> std::optional<std::string>;

  /**
   * Get all native Parasolid attributes stored on this entity.
   *
   * Returns a dict mapping each attribute name to a list of its values.
   * Value types are Python int, float, or str.
   *
   * @return Dict of attribute name to list of values
   * @nb
   * @nb_prop_ro native_attributes
   */
  auto get_native_attributes() const -> nb::dict;
};

/**
 * @class Part
 * @brief Model part
 * @nb
 * @nb_inherit ModelItem
 */
class Part : public ModelItem {

public:
  Part(pModelItem s_model_item, nb::ref<Model> model);
  /**
   * Name
   *
   * @return Name or None
   * @nb
   * @nb_prop_ro name
   */
  auto get_name() const -> std::optional<std::string>;

  /**
   * Get the parent assembly
   *
   * @return Assembly or none
   * @nb
   * @nb_prop_ro parent_assembly
   */
  auto get_parent_assembly() const -> std::optional<nb::ref<Assembly>>;

  /**
   * Get regions of this part.
   *
   * @return Regions
   * @nb
   * @nb_prop_ro regions
   */
  auto get_regions() const -> std::vector<nb::ref<Region>>;

  /**
   * Get faces of this part.
   *
   * @return Faces
   * @nb
   * @nb_prop_ro faces
   */
  auto get_faces() const -> std::vector<nb::ref<Face>>;

  /**
   * Get all native Parasolid attributes stored on this part.
   *
   * @return Dict of attribute name to list of values
   * @nb
   * @nb_prop_ro native_attributes
   */
  auto get_native_attributes() const -> nb::dict;

  /**
   * Compute the centroid of this part's region.
   *
   * @return Centroid [x, y, z]
   * @nb
   * @nb_prop_ro centroid
   */
  auto get_centroid() const -> std::array<double, 3>;
};

/**
 * @class Assembly
 * @brief Model assembly
 * @nb
 * @nb_inherit ModelItem
 */
class Assembly : public ModelItem {

  using ModelItem::ModelItem;

public:
  /**
   * Get the name
   *
   * @return Name or None
   * @nb
   * @nb_prop_ro name
   */
  auto get_name() const -> std::optional<std::string>;

  /**
   * Get the parent assembly
   *
   * @return Assembly or None
   * @nb
   * @nb_prop_ro parent_assembly
   */
  auto get_parent_assembly() const -> std::optional<nb::ref<Assembly>>;

  /**
   * Get instantiated parts in this assembly.
   *
   * @return Parts
   * @nb
   * @nb_prop_ro parts
   */
  auto get_parts() const -> std::vector<nb::ref<Part>>;

  /**
   * Get sub-assemblies in this assembly.
   *
   * @return Sub-assemblies
   * @nb
   * @nb_prop_ro assemblies
   */
  auto get_assemblies() const -> std::vector<nb::ref<Assembly>>;

  /**
   * Get all native Parasolid attributes stored on this assembly.
   *
   * @return Dict of attribute name to list of values
   * @nb
   * @nb_prop_ro native_attributes
   */
  auto get_native_attributes() const -> nb::dict;
};

/**
 * @class Vertex
 * @brief Model vertex
 * @nb
 * @nb_inherit Entity
 */
class Vertex : public Entity {
  using Entity::Entity;
};

/**
 * @class Edge
 * @brief Model edge
 * @nb
 * @nb_inherit Entity
 */
class Edge : public Entity {
  using Entity::Entity;

public:
  /**
   * Length of this edge.
   *
   * @return Length
   * @nb
   * @nb_prop_ro length
   */
  auto get_length() const -> double;

  /**
   * Midpoint of this edge [x, y, z].
   *
   * @return Midpoint
   * @nb
   * @nb_prop_ro midpoint
   */
  auto get_midpoint() const -> std::array<double, 3>;
};

/**
 * @class Face
 * @brief Model face
 * @nb
 * @nb_inherit Entity
 */
class Face : public Entity {
  using Entity::Entity;

public:
  /**
   * Get the region on the forward side of this face, if any.
   *
   * @return Forward region or None
   * @nb
   * @nb_prop_ro forward_region
   */
  auto get_forward_region() const -> std::optional<nb::ref<Region>>;

  /**
   * Get the region on the reverse side of this face, if any.
   *
   * @return Reverse region or None
   * @nb
   * @nb_prop_ro reverse_region
   */
  auto get_reverse_region() const -> std::optional<nb::ref<Region>>;

  /**
   * Get the edges bounding this face.
   *
   * @return Edges
   * @nb
   * @nb_prop_ro edges
   */
  auto get_edges() const -> std::vector<nb::ref<Edge>>;

  /**
   * Compute the centroid (center of mass) of this face.
   *
   * @return Centroid [x, y, z]
   * @nb
   * @nb_prop_ro centroid
   */
  auto get_centroid() const -> std::array<double, 3>;
};

/**
 * @class Region
 * @brief Model region
 * @nb
 * @nb_inherit Entity
 */
class Region : public Entity {
  using Entity::Entity;

public:
  /**
   * Compute the centroid of this region.
   *
   * @return Centroid [x, y, z]
   * @nb
   * @nb_prop_ro centroid
   */
  auto get_centroid() const -> std::array<double, 3>;
};

/**
 * @class Model
 * @brief model
 * @nb
 * @nb_intrusive_ptr
 */
class Model : public nb::intrusive_base {
  struct Connection {
    nb::ref<Model> parent;
    pMConnector s_connector;
    pANMConnection s_anm;

    Connection(nb::ref<Model> parent, pMConnector connector);
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;
    Connection(Connection &&other) noexcept;
    Connection &operator=(Connection &&other) noexcept;
    ~Connection();
  };

  std::optional<Connection> connection;

public:
  pGModel s_model;

  Model(pGModel s_model) : s_model(s_model) {};
  Model(pGModel s_model, nb::ref<Model> parent, pMConnector s_connector);
  ~Model();

  /**
   * Whether this model is an assembly model (multiple root parts).
   *
   * @return True if assemby model.
   */
  bool is_assembly_model() const;

  /**
   * @brief Get model root items.
   *
   * @return Root items.
   * @nb
   * @nb_prop_ro root_items
   */
  auto get_root_items() const -> std::vector<nb::ref<ModelItem>>;

  /**
   * Whether this model is topologically and geometrically valid (GM_isValid).
   *
   * @return True if valid.
   * @nb
   */
  bool is_valid() const;

  /**
   * Cache this model to disk so it round-trips through read().
   *
   * Writes two files: the SMS model topology (the given .smd) and, alongside it,
   * the underlying native (Parasolid) geometry as <stem>.xmt_txt. Both are
   * needed -- the .smd alone has no geometry, so a reloaded model could not
   * compute centroids or be meshed. read() reloads the pair.
   *
   * @param filename SMS model filename (.smd)
   * @nb
   */
  void write(std::string filename);

  /**
   * Read a model cached by write(): loads the native geometry companion
   * (<stem>.xmt_txt) and binds it to the SMS model from the .smd.
   *
   * @param filename SMS model filename (.smd)
   * @return Model
   * @nb
   */
  static auto read(std::string filename) -> nb::ref<Model>;

  /**
   * Load a GAM assembly model from a Parasolid file (stage 1 of 2).
   * Call translate() on the result to obtain the SMS model needed for meshing.
   *
   * @param filename Filename
   * @param load_nx_attrs If true, auto-detect <basename>_attrs.json alongside
   *                      the .x_t and load NX component attributes as native
   *                      Parasolid attributes on each Part/Assembly.
   * @return GAM assembly model
   * @nb
   */
  static auto from_parasolid_file(std::string filename,
                                   bool load_nx_attrs = false) -> nb::ref<Model>;

  /**
   * Translate this GAM assembly model into an SMS model (stage 2 of 2).
   * This is where geometry validation occurs; invalid faces will raise here.
   *
   * @return Translated SMS model
   * @nb
   */
  auto translate() -> nb::ref<Model>;

  /**
   * Make nonmanifold model from assembly model
   *
   * @param tolerance Boolean tolerance other than default. Defaults to None.
   * @return NonManifold model
   * @nb
   */
  auto make_non_manifold_model(std::optional<double> tolerance = std::nullopt)
      -> nb::ref<Model>;

  /**
   * Mesh
   *
   * @param filename Filename
   * @param mesh_size Relative mesh size
   * @nb
   */
  void mesh(std::string filename, double mesh_size);

  /**
   * Return all the faces in this model.
   *
   * @return Faces
   * @nb
   * @nb_prop_ro faces
   */
  auto get_faces() const -> std::vector<nb::ref<Face>>;

  /**
   * Return all the regions in this model.
   *
   * @return Regions
   * @nb
   * @nb_prop_ro regions
   */
  auto get_regions() const -> std::vector<nb::ref<Region>>;

  /**
   * Return all the edges in this model.
   *
   * @return Edges
   * @nb
   * @nb_prop_ro edges
   */
  auto get_edges() const -> std::vector<nb::ref<Edge>>;

  /**
   * Return all the vertices in this model.
   *
   * @return Vertices
   * @nb
   * @nb_prop_ro vertices
   */
  auto get_vertices() const -> std::vector<nb::ref<Vertex>>;

  bool has_connection() const;
  auto require_connection() const -> const struct Connection &;
};

/**
 * @class MeshCase
 * @brief Mesh attributes
 * @nb
 * @nb_intrusive_ptr
 */
class MeshCase : public nb::intrusive_base {
  nb::ref<Model> model;

public:
  pACase s_mesh_case;
  MeshCase(nb::ref<Model> model);

  /**
   * Constructor
   *
   * @param model Model
   * @return Mesh case
   * @nb
   * @nb_new
   */
  static auto make(nb::ref<Model> model) -> nb::ref<MeshCase>;

  /**
   * Set the mesh size.
   *
   * @param mesh_size Mesh size
   * @param relative If true (default) the size is relative to the target entity's bounding box (Simmetrix type 2); if false it is an absolute size (type 1).
   * @param model_item Optional model entity. A Region sets the volume element size; a Face/Edge sets the surface/edge size; default is the whole domain.
   * @nb
   */
  void set_size(double mesh_size, bool relative = true,
                std::optional<nb::ref<ModelItem>> model_item = std::nullopt);

  /**
   * Set the mesh curvature refinement.
   *
   * @param value Value of curvature refinement.
   * @param relative Use relative rather than absolute value. Defaults to
   * False.
   * @param use_edges Consider edge curvature in addition to face curvature.
   * Defaults to False.
   * @param min_size Min allowable refinement size. Defaults to None.
   * @param anisotropic Whether refinement is anisotropic. Defaults to False.
   * @param entity Optional model entity
   * @nb
   */
  void set_curvature_refinement(
      double value, bool relative = false, bool use_edges = false,
      std::optional<double> min_size = std::nullopt, bool anisotropic = false,
      std::optional<nb::ref<ModelItem>> model_item = std::nullopt);

  /**
   * Set proximity refinement for thin sections.
   *
   * @param value Mesh size will be set to (section thickness) / value.
   * @param min_size Min allowable refinement size. Defaults to None.
   * @param model_item Optional model entity
   * @nb
   */
  void set_proximity_refinement(
      double value, std::optional<double> min_size = std::nullopt,
      std::optional<nb::ref<ModelItem>> model_item = std::nullopt);

  /**
   * Set the global mesh size gradation rate.
   *
   * Controls how quickly element size grows from finer to coarser regions.
   * Smaller values give a slower, smoother transition. Wraps
   * MS_setGlobalSizeGradationRate.
   *
   * @param rate Gradation rate.
   * @nb
   */
  void set_gradation_rate(double rate);

  /**
   * Exclude a model entity (e.g. a region/body) and its closure from meshing.
   *
   * Use to mesh only a subset of an assembly: mark the bodies you do not want
   * meshed. Wraps MS_setNoMesh with closure enabled.
   *
   * @param model_item Model entity to exclude (Region, Face, ...).
   * @nb
   */
  void set_no_mesh(nb::ref<ModelItem> model_item);

  /**
   * Add a point refinement: refine the mesh to `size` around a point.
   *
   * Wraps MS_addPointRefinement.
   *
   * @param size Refinement size at the point.
   * @param point Point [x, y, z] at which to refine.
   * @nb
   */
  void add_point_refinement(double size, std::array<double, 3> point);
};

/**
 * @class Mesh
 * @brief Mesh attributes
 * @nb
 * @nb_intrusive_ptr
 */
class Mesh : public nb::intrusive_base {
public:
  pMesh s_mesh;
  nb::ref<Model> model;
  nb::ref<MeshCase> mesh_case;

  Mesh(pMesh s_mesh, nb::ref<Model> model, nb::ref<MeshCase> mesh_case)
      : s_mesh(s_mesh), model(model), mesh_case(mesh_case) {};

  /**
   * Write mesh to Gmsh .msh file.
   *
   * @param filename Filename
   * @nb
   */
  void write_msh(std::string filename, double scale_factor = 1.0);
};

/**
 * @class SurfaceMesh
 * @brief Surface Mesh
 * @nb
 * @nb_inherit Mesh
 */
class SurfaceMesh : public Mesh {
public:
  using Mesh::Mesh;

  /**
   * Surface mesh
   *
   * @param model Model
   * @param mesh_case Mesh case
   * @return Mesh
   * @nb
   */
  static auto from_model(nb::ref<Model> model, nb::ref<MeshCase> mesh_case)
      -> nb::ref<Mesh>;
};

/**
 * @class VolumeMesh
 * @brief Volume Mesh
 * @nb
 * @nb_inherit Mesh
 */
class VolumeMesh : public Mesh {
public:
  using Mesh::Mesh;

  /**
   * Create from surface mesh.
   *
   * @param surface_mesh Surface mesh
   * @param enforce_size Interior mesh-size enforcement (VolumeMesher_setEnforceSize):
   * 0 = not strictly enforced (default, interior may be coarser than requested),
   * 1 = adapt interior toward the requested size, 2 = adapt + redistribute.
   * @return Volume Mesh
   * @nb
   */
  static auto from_surface_mesh(nb::ref<SurfaceMesh> surface_mesh,
                                int enforce_size = 0) -> nb::ref<VolumeMesh>;
};
