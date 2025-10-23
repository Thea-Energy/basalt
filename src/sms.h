#include "AttributeTypes.h"
#include "MeshSim.h"
#include "ModelTypes.h"
#include "SimModel.h"
#include "nanobind/intrusive/counter.h"
#include <optional>
#include <string>

#include <nanobind/intrusive/ref.h>
#include <nanobind/nanobind.h>
#include <variant>
#include <vector>

namespace nb = nanobind;

class Model;
class Part;

/**
 * @class Entity
 * @brief Model entity
 * @nb extra: 'nb::intrusive_ptr<Entity>([](Entity* o, PyObject*
 po) noexcept { o->set_self_py(po); })'
 */
class Entity : public nb::intrusive_base {
public:
  pGEntity s_entity;
  nb::ref<Model> model;

  Entity(pGEntity s_entity, nb::ref<Model> model)
      : s_entity(s_entity), model(model) {};

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
  pGIPart s_entity;
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
   * @return NonManifold model
   * @nb
   */
  auto make_non_manifold_model() -> nb::ref<Model>;

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
   @ @nb
   */
  auto regions() const -> std::vector<nb::ref<Region>>;

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
  std::optional<double> mesh_size;
  std::optional<double> mesh_curve;

public:
  pACase s_mesh_case;
  MeshCase(nb::ref<Model> model);

  /**
   * Constructor
   *
   * @param model Model
   * @return Mesh case
   * @nb
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