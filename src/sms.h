#include "ModelTypes.h"
#include "SimModel.h"
#include "nanobind/intrusive/counter.h"
#include <string>

#include <nanobind/intrusive/ref.h>
#include <nanobind/nanobind.h>

namespace nb = nanobind;

/**
 * @class Assembly Model
 * @brief Assembly model
 * @nb extra: 'nb::intrusive_ptr<AssemblyModel>([](AssemblyModel* o, PyObject*
 po) noexcept { o->set_self_py(po); })'
 */
class Model : public nb::intrusive_base {
public:
  pGModel s_model;

public:
  Model(pGModel s_model) : s_model(s_model) {};
};

/**
 * @class Assembly Model
 * @brief Assembly model
 * @nb inherit: Model
 */
class AssemblyModel : public Model {

public:
  /**
   * Make assembly model from parasolid file
   *
   * @param filename Filename
   * @return Assembly model
   * @nb
   */
  static auto from_parasolid_file(std::string filename)
      -> nb::ref<AssemblyModel>;

  AssemblyModel(pGModel s_model) : Model(s_model) {};
};

/**
 * @class Unified Model
 * @brief Unified model with shared topology
 * @nb inherit: Model
 */
class UnifiedModel : public Model {
public:
  pMConnector s_region_connector;
  pANMConnection s_part_connector;

  UnifiedModel(pGModel s_model, pMConnector s_region_connector,
               pANMConnection s_part_connector)
      : Model(s_model), s_region_connector(s_region_connector),
        s_part_connector(s_part_connector) {};

  /**
   * Make unified model from assembly model
   *
   * @param assembly_model Assembly model
   * @return Unified model
   * @nb
   */
  static auto from_assembly_model(nb::ref<AssemblyModel> assembly_model)
      -> nb::ref<UnifiedModel>;

  /**
   * Mesh
   *
   * @param filename Filename
   * @param mesh_size Relative mesh size
   * @nb
   */
  void mesh(std::string filename, double mesh_size);
};