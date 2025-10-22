#include "sms.h"
#include "MeshSim.h"
#include "MeshTypes.h"
#include "ModelTypes.h"
#include "SimModel.h"
#include "SimPList.h"
#include "SimParasolidKrnl.h"
#include "spdlog/spdlog.h"
#include <optional>
#include <stdexcept>

Model::Connection::Connection(nb::ref<Model> parent, pMConnector s_connector)
    : parent(std::move(parent)), s_connector(s_connector) {
  this->s_anm = ANMConnection_new(s_connector);
}

Model::Connection::~Connection() {
  ANMConnection_delete(this->s_anm);
  MC_release(this->s_connector);
}

Model::Model(pGModel s_model_, nb::ref<Model> parent, pMConnector s_connector)
    : s_model(s_model_),
      connection(std::in_place, std::move(parent), s_connector) {}

auto Model::require_connection() const -> const Connection & {
  if (!this->connection)
    throw std::runtime_error("operation requires a non-manifold connection");
  return *this->connection;
}

auto Entity::related_parts() const -> std::vector<nb::ref<Part>> {
  const auto &conn = this->model->require_connection();

  std::vector<nb::ref<Part>> parts;
  auto s_assem_parts = ANMConnection_relatedParts(conn.s_anm, this->s_entity);

  void *iter = 0;
  while (auto s_part = (pGEntity)PList_next(s_assem_parts, &iter)) {
    parts.push_back({new Part(s_part, this->model)});
  }
  PList_delete(s_assem_parts);
  return parts;
}

auto Part::get_name() const -> std::optional<std::string> {
  char *name = GIP_nativeName((pGIPart)this->s_entity);
  if (name != nullptr) {
    std::string result(name);
    Sim_deleteString(name);
    return result;
  } else {
    return std::nullopt;
  }
}

Model::~Model() {
  GM_release(s_model);
  connection.reset();
}

bool Model::is_assembly_model() const {
  return GM_isAssemblyModel(this->s_model);
}

bool Model::is_valid() const {
  auto error_list{PList_new()};
  auto is_valid = GM_isValid(this->s_model, 1, error_list);
  PList_delete(error_list);
  return is_valid;
}

auto Model::from_parasolid_file(std::string filename) -> nb::ref<Model> {
  auto parasolid_native_model = ParasolidNM_createFromFile(filename.c_str(), 0);
  if (!parasolid_native_model) {
    throw std::runtime_error("failed to load Parasolid file: " + filename);
  }

  auto assembly_model =
      GAM_createFromNativeModel(parasolid_native_model, nullptr);
  if (!assembly_model) {
    throw std::runtime_error(
        "failed to create assembly model from Parasolid native model");
  }

  return {new Model(assembly_model)};
}

auto Model::make_non_manifold_model() -> nb::ref<Model> {
  auto connector = MC_new();
  auto s_new_model =
      GM_createFromAssemblyModel(this->s_model, connector, nullptr);

  return {new Model(s_new_model, nb::ref<Model>(this), connector)};
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

auto Model::regions() const -> std::vector<nb::ref<Region>> {
  std::vector<nb::ref<Region>> regions;

  auto s_model_regions = GM_regionIter(this->s_model);
  while (auto s_model_region = GRIter_next(s_model_regions)) {
    regions.push_back({new Region(s_model_region,
                                  nb::ref<Model>(const_cast<Model *>(this)))});
  }
  GRIter_delete(s_model_regions);

  return regions;
}

bool Model::has_connection() const { return connection.has_value(); }