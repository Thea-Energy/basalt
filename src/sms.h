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
 * @class Entity
 * @brief Model entity
 * @nb extra: 'nb::intrusive_ptr<Entity>([](Entity* o, PyObject*
 po) noexcept { o->set_self_py(po); })'
 */
class Entity : public nb::intrusive_base {
protected:
  pGEntity s_entity;
  nb::ref<Model> model;

public:
  Entity(pGEntity s_entity, nb::ref<Model> model)
      : s_entity(s_entity), model(model) {};

public:
  /**
   * Get related parts
   *
   * @return Related parts
   * @nb
   */
  auto related_parts() -> std::vector<nb::ref<Part>>;
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
  auto get_name() -> std::optional<std::string>;
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

public:
  pGModel s_model;
  nb::ref<Model> parent = nullptr;
  pMConnector s_connector;
  pANMConnection s_model_connection = nullptr;
  Model(pGModel s_model) : s_model(s_model), s_connector(MC_new()) {};

  Model(pGModel s_model, nb::ref<Model> parent, pMConnector s_connector)
      : s_model(s_model), parent(parent), s_connector(s_connector),
        s_model_connection(ANMConnection_new(s_connector)) {};

  /**
   * Whether this model is an assembly model (multiple root parts).
   *
   * @return True if assemby model.
   */
  bool is_assembly_model();

  /**
   * Whether this model is topologically and geometrically valid.
   *
   * @return True if valid.
   */
  bool is_valid();

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
  auto regions() -> std::vector<nb::ref<Region>>;
};
