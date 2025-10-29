#include "AttributeTypes.h"
#include "MeshSim.h"
#include "MeshTypes.h"
#include "ModelTypes.h"
#include "SimModel.h"
#include "nanobind/intrusive/counter.h"
#include <optional>
#include <string>

#include <nanobind/intrusive/ref.h>
#include <nanobind/nanobind.h>
#include <vector>

namespace nb = nanobind;

class Model;
class Part;

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
 * @nb extra: 'nb::intrusive_ptr<ModelItem>([](ModelItem* o, PyObject*
 po) noexcept { o->set_self_py(po); })'
 */
class ModelItem : public nb::intrusive_base {
public:
  const pModelItem s_model_item;
  nb::ref<Model> model;

  ModelItem(pModelItem s_model_item, nb::ref<Model> model)
      : s_model_item(s_model_item), model(model) {};
};

/**
 * @class Entity
 * @nb inherit: ModelItem
 */
class Entity : public ModelItem {
public:
  using ModelItem::ModelItem;

  /**
   * Get related parts
   *
   * @return Related parts
   * @nb
   */
  auto related_parts() const -> std::vector<nb::ref<Part>>;
};

/**
 * @class Part
 * @brief Model part
 * @nb inherit: Entity
 */
class Part : public Entity {

  using Entity::Entity;

public:
  /**
   * Name
   *
   * @return Name or None
   * @nb prop_r: name
   */
  auto get_name() const -> std::optional<std::string>;
};

/**
 * @class Region
 * @brief Model region
 * @nb inherit: Entity
 */
class Region : public Entity {
  using Entity::Entity;
};

/**
 * @class Group
 * @brief Model group
 * @nb extra: 'nb::intrusive_ptr<Group>([](Group* o, PyObject*
 po) noexcept { o->set_self_py(po); })'
 */
class Group : public nb::intrusive_base {
public:
  pModelItemGroup s_group;
  std::string name;
  nb::ref<Model> model;

  Group(pModelItemGroup s_group, nb::ref<Model> model);
  Group(std::string &name, nb::ref<Model> model);

  // TODO(akoen): destructor

  /**
   * Constructor
   *
   * @param name Group name
   * @param model Group model
   * @nb new
   */
  static auto make(std::string &name, nb::ref<Model> model) -> nb::ref<Group>;

  /**
   * Add item to group
   *
   * @param item Item
   * @nb
   */
  void add(nb::ref<ModelItem> item);

  /**
   * Return items in group.
   *
   * @return item
   * @nb
   */
  auto items() -> std::vector<nb::ref<ModelItem>>;
};

/**
 * @class Model
 * @brief model
 * @nb extra: 'nb::intrusive_ptr<Model>([](Model* o, PyObject*
 po) noexcept { o->set_self_py(po); })'
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
   * Whether this model is topologically and geometrically valid.
   *
   * @return True if valid.
   */
  bool is_valid() const;

  /**
   * Write to SMS native file .smd
   *
   * @param filename Filename
   * @nb
   */
  void write(std::string filename);

  /**
   * Read from SMS native file .smd
   *
   * @param filename Filename
   * @return Model
   * @nb
   */
  static auto read(std::string filename) -> nb::ref<Model>;

  /**
   * Make assembly model from parasolid file
   *
   * @param filename Filename
   * @return Assembly model
   * @nb
   */
  static auto from_parasolid_file(std::string filename) -> nb::ref<Model>;

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
   * Return all the regions in this model.
   *
   * @return Regions
   * @nb
   */
  auto regions() const -> std::vector<nb::ref<Region>>;

  /**
   * Get groups in this model
   *
   * @return Groups
   * @nb prop_r: groups
   */
  auto get_groups() const -> std::vector<nb::ref<Group>>;

  bool has_connection() const;
  auto require_connection() const -> const struct Connection &;
};

/**
 * @class MeshCase
 * @brief Mesh attributes
 * @nb extra: 'nb::intrusive_ptr<MeshCase>([](MeshCase* o, PyObject*
 po) noexcept { o->set_self_py(po); })'
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
   * @nb new
   */
  static auto make(nb::ref<Model> model) -> nb::ref<MeshCase>;

  /**
   * Set the mesh size
   *
   * @param mesh_size Mesh size
   * @param entity Optional model entity
   * @nb
   */
  void set_mesh_size(double mesh_size,
                     std::optional<nb::ref<Entity>> entity = std::nullopt);

  /**
   * Set the mesh curvature refinement.
   *
   * @param value Value of curvature refinement.
   * @param relative Use relative rather than absolute value. Defaults to False.
   * @param use_edges Consider edge curvature in addition to face curvature.
   * Defaults to False.
   * @param min_size Min allowable refinement size. Defaults to None.
   * @param anisotropic Whether refinement is anisotropic. Defaults to False.
   * @param entity Optional model entity
   * @nb
   */
  void set_mesh_curvature_refinement(
      double value, bool relative = false, bool use_edges = false,
      std::optional<double> min_size = std::nullopt, bool anisotropic = false,
      std::optional<nb::ref<Entity>> entity = std::nullopt);

  // TODO Should be model item not entity.
  /**
   * Set proximity refinement for thin sections.
   *
   * @param value Mesh size will be set to (section thickness) / value.
   * @param min_size Min allowable refinement size. Defaults to None.
   * @param model_item Optional model entity
   * @nb
   */
  void set_mesh_proximity_refinement(
      double value, std::optional<double> min_size = std::nullopt,
      std::optional<nb::ref<Entity>> model_item = std::nullopt);
};

/**
 * @class Mesh
 * @brief Mesh attributes
 * @nb extra: 'nb::intrusive_ptr<Mesh>([](Mesh* o, PyObject*
 po) noexcept { o->set_self_py(po); })'
 */
class Mesh : public nb::intrusive_base {
  pMesh s_mesh;
  nb::ref<Model> model;

public:
  Mesh(pMesh s_mesh, nb::ref<Model> model) : s_mesh(s_mesh), model(model) {};

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

  /**
   * Write mesh to Gmsh file.
   *
   * @param filename Filename
   * @nb
   */
  void write_gmsh(std::string filename);
};