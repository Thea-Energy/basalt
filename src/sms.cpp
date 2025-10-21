#include "sms.h"
#include "MeshSim.h"
#include "MeshTypes.h"
#include "ModelTypes.h"
#include "SimModel.h"
#include "SimPList.h"
#include "SimParasolidKrnl.h"
#include "spdlog/spdlog.h"
#include <optional>

auto Entity::related_parts() -> std::vector<nb::ref<Part>> {
  std::vector<nb::ref<Part>> parts;
  auto s_assem_parts = ANMConnection_relatedParts(
      this->model->s_model_connection, this->s_entity);

  void *iter = 0;
  while (auto s_part = (pGEntity)PList_next(s_assem_parts, &iter)) {
    parts.push_back({new Part(s_part, this->model)});
  }
  PList_delete(s_assem_parts);
  return parts;
}

auto Part::get_name() -> std::optional<std::string> {
  char *name = GIP_nativeName((pGIPart)this->s_entity);
  if (name != 0) {
    return std::string(name);
    Sim_deleteString(name);
  } else {
    return std::nullopt;
  }
}

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

auto Model::make_non_manifold_model() -> nb::ref<Model> {
  auto s_connector = MC_new();
  auto s_new_model =
      GM_createFromAssemblyModel(this->s_model, s_connector, nullptr);

  auto non_manifold_model =
      new Model(s_new_model, nb::ref<Model>(this), s_connector);

  return {non_manifold_model};
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

auto Model::regions() -> std::vector<nb::ref<Region>> {
  std::vector<nb::ref<Region>> regions;

  auto s_model_regions = GM_regionIter(this->s_model);
  while (auto s_model_region = GRIter_next(s_model_regions)) {
    regions.push_back({new Region(s_model_region, {this})});
  }

  return regions;
}