#include "sms.h"
#include "MeshSim.h"
#include "ModelTypes.h"
#include "SimModel.h"
#include "SimParasolidKrnl.h"
#include "spdlog/spdlog.h"

auto AssemblyModel::from_parasolid_file(std::string filename)
    -> nb::ref<AssemblyModel> {
  auto parasolid_native_model = ParasolidNM_createFromFile(filename.c_str(), 0);
  auto assembly_model =
      GAM_createFromNativeModel(parasolid_native_model, nullptr);

  return {new AssemblyModel(assembly_model)};
}

auto UnifiedModel::from_assembly_model(nb::ref<AssemblyModel> assembly_model)
    -> nb::ref<UnifiedModel> {
  auto s_region_connector = MC_new();
  auto model = GM_createFromAssemblyModel(assembly_model->s_model,
                                          s_region_connector, nullptr);
  auto s_part_connector = ANMConnection_new(s_region_connector);

  auto unified_model{
      new UnifiedModel(model, s_region_connector, s_part_connector)};

  return {unified_model};
}

void UnifiedModel::mesh(std::string filename, double mesh_size) {
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
