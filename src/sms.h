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
class AssemblyModel : public nb::intrusive_base {

public:
  pGModel s_assembly_model;

  AssemblyModel(pGModel s_assembly_model)
      : s_assembly_model(s_assembly_model) {};

  /**
   * Make assembly model from parasolid file
   *
   * @param filename Filename
   * @return Assembly model
   * @nb
   */
  static auto from_parasolid_file(std::string filename)
      -> nb::ref<AssemblyModel>;
};

/**
 * @class Unified Model
 * @brief Unified model with shared topology
 * @nb
 * @nb extra: 'nb::intrusive_ptr<UnifiedModel>([](UnifiedModel* o,
 PyObject* po) noexcept { o->set_self_py(po); })'
 */
class UnifiedModel : public nb::intrusive_base {
public:
  pGModel s_nonmanifold_model;
  pMConnector s_region_connector;
  pANMConnection s_part_connector;

  UnifiedModel(pGModel s_nonmanifold_model, pMConnector s_region_connector,
               pANMConnection s_part_connector)
      : s_nonmanifold_model(s_nonmanifold_model),
        s_region_connector(s_region_connector),
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