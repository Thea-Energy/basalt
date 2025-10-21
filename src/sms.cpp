#include "sms.h"
#include "MeshSim.h"
#include "MeshTypes.h"
#include "ModelTypes.h"
#include "SimModel.h"
#include "SimParasolidKrnl.h"
#include "spdlog/spdlog.h"

bool Model::is_assembly_model() { return GM_isAssemblyModel(this->s_model); }

bool Model::is_valid() {
  auto error_list{PList_new()};
  auto is_valid = GM_isValid(this->s_model, 1, error_list);
  PList_delete(error_list);
  return is_valid;
}

auto Model::from_parasolid_file(std::string filename) -> nb::ref<Model> {
  auto parasolid_native_model = ParasolidNM_createFromFile(filename.c_str(), 0);
  auto assembly_model =
      GAM_createFromNativeModel(parasolid_native_model, nullptr);

  return {new Model(assembly_model)};
}

auto Model::make_nonmanifold_model() -> nb::ref<Model> {
  auto s_connector = MC_new();
  auto s_new_model =
      GM_createFromAssemblyModel(this->s_model, s_connector, nullptr);

  auto unified_model = new Model(s_model, nb::ref<Model>(this), s_connector);

  return {unified_model};
}

void Model::mesh(std::string filename, double mesh_size) {
  auto s_mesh = M_new(0, this->s_model);
  auto s_mesh_case = MS_newMeshCase(this->s_model);
  auto s_model_domain = GM_domain(this->s_model);

  MS_setMeshSize(s_mesh_case, s_model_domain, 1, mesh_size, 0);

  auto s_surface_mesher = SurfaceMesher_new(s_mesh_case, s_mesh);
  SurfaceMesher_execute(s_surface_mesher, nullptr);
  M_write(s_mesh, filename.c_str(), 0, nullptr);

  SurfaceMesher_delete(s_surface_mesher);
  MS_deleteMeshCase(s_mesh_case);
  M_release(s_mesh);
}
