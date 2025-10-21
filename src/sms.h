#include "ModelTypes.h"
#include "SimModel.h"
#include "nanobind/intrusive/counter.h"
#include <string>

#include <nanobind/intrusive/ref.h>
#include <nanobind/nanobind.h>

namespace nb = nanobind;

/**
 * @class Model
 * @brief model
 * @nb extra: 'nb::intrusive_ptr<Model>([](Model* o, PyObject*
 po) noexcept { o->set_self_py(po); })'
 */
class Model : public nb::intrusive_base {
protected:
  pGModel s_model;
  nb::ref<Model> parent = nullptr;
  pMConnector s_connector;

public:
  Model(pGModel s_model) : s_model(s_model), s_connector(MC_new()) {};

  Model(pGModel s_model, nb::ref<Model> parent, pMConnector s_connector)
      : s_model(s_model), parent(parent), s_connector(s_connector) {};

  /**
   * Whether this model is an assembly model (multiple root parts).
   *
   * @return True if assemby model.
   */
  bool is_assembly_model();

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
  auto make_nonmanifold_model() -> nb::ref<Model>;

  /**
   * Mesh
   *
   * @param filename Filename
   * @param mesh_size Relative mesh size
   * @nb
   */
  void mesh(std::string filename, double mesh_size);
};
