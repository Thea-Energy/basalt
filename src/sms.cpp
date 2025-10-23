#include "sms.h"
#include "AttributeTypes.h"
#include "MeshSim.h"
#include "MeshTypes.h"
#include "ModelTypes.h"
#include "SimModel.h"
#include "SimPList.h"
#include "SimParasolidKrnl.h"
#include "gmsh.h"
#include "init.h"
#include "spdlog/spdlog.h"
#include <cstddef>
#include <optional>
#include <stdexcept>

Model::Connection::Connection(nb::ref<Model> parent, pMConnector s_connector)
    : parent(std::move(parent)), s_connector(s_connector) {
  this->s_anm = ANMConnection_new(s_connector);
}

Model::Connection::~Connection() {
  if (sms_is_initialized()) {
    ANMConnection_delete(this->s_anm);
    MC_release(this->s_connector);
  }
}

Model::Model(pGModel s_model_, nb::ref<Model> parent, pMConnector s_connector)
    : s_model(s_model_),
      connection(std::in_place, std::move(parent), s_connector) {}

auto Model::require_connection() const -> const Connection & {
  if (!this->connection)
    throw_exception("Operation requires a connected model.");
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
  if (sms_is_initialized())
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

void Model::write(std::string filename) {
  // GM_write(this->s_model, filename.c_str(), 0, nullptr);
  pNativeModel nonManNative = GM_nativeModel(this->s_model);
  NM_write(nonManNative, filename.c_str(), nullptr);
}

auto Model::read(std::string filename) -> nb::ref<Model> {
  auto model = GM_load(filename.c_str(), nullptr, nullptr);

  return {new Model(model)};
}

auto Model::from_parasolid_file(std::string filename) -> nb::ref<Model> {
  auto parasolid_native_model = ParasolidNM_createFromFile(filename.c_str(), 0);
  if (!parasolid_native_model) {
    throw std::runtime_error("failed to load Parasolid file: " + filename);
  }

  auto assembly_model =
      GAM_createFromNativeModel(parasolid_native_model, nullptr);

  return {new Model(assembly_model)};
}

auto Model::make_non_manifold_model(std::optional<double> tolerance)
    -> nb::ref<Model> {
  auto connector = MC_new();

  auto model_builder = ModelBuilder_new(this->s_model);
  ModelBuilder_setConnector(model_builder, connector);
  if (tolerance.has_value()) {
    ModelBuilder_setBooleanTolerance(model_builder, tolerance.value());
  }
  auto s_new_model = ModelBuilder_execute(model_builder, nullptr);

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

MeshCase::MeshCase(nb::ref<Model> model) : model(model) {
  this->s_mesh_case = MS_newMeshCase(this->model->s_model);
}

auto MeshCase::make(nb::ref<Model> model) -> nb::ref<MeshCase> {
  return {new MeshCase(model)};
}

void MeshCase::set_mesh_size(double mesh_size,
                             std::optional<nb::ref<Entity>> entity) {
  auto s_model_item = entity.has_value() ? entity.value()->s_entity
                                         : GM_domain(this->model->s_model);

  MS_setMeshSize(this->s_mesh_case, s_model_item, 2, mesh_size, 0);
}

auto Mesh::from_model(nb::ref<Model> model, nb::ref<MeshCase> mesh_case)
    -> nb::ref<Mesh> {
  auto s_mesh = M_new(0, model->s_model);
  auto s_mesh_case = mesh_case->s_mesh_case;
  auto s_surface_mesher = SurfaceMesher_new(s_mesh_case, s_mesh);

  SurfaceMesher_execute(s_surface_mesher, nullptr);
  M_write(s_mesh, "mesh.sms", 0, nullptr);

  SurfaceMesher_delete(s_surface_mesher);
  MS_deleteMeshCase(s_mesh_case);

  return {new Mesh(s_mesh, model)};
}

void Mesh::write_gmsh(std::string filename) {
  gmsh::initialize();
  gmsh::model::add("sms");

  auto vit = M_vertexIter(this->s_mesh);
  size_t vid = 1;
  while (auto v = VIter_next(vit)) {
    EN_setID((pEntity)v, vid++);
  }
  VIter_delete(vit);

  auto fitAll = M_faceIter(this->s_mesh);
  size_t fid = 1;
  while (auto f = FIter_next(fitAll)) {
    EN_setID((pEntity)f, fid++);
  }
  FIter_delete(fitAll);

  for (auto r : this->model->regions()) {
    auto tag = GEN_tag(r->s_entity);

    std::vector<int> region_boundary_tags;
    auto s_faces = GEN_faces(r->s_entity);
    void *iter = 0;
    while (auto s_face = (pGFace)(PList_next(s_faces, &iter))) {
      auto tag = GEN_tag(s_face);
      region_boundary_tags.push_back(tag);
      try {
        gmsh::model::addDiscreteEntity(2, tag);
      } catch (const std::runtime_error &) {
        continue;
      }

      std::vector<size_t> node_tags;
      std::vector<double> node_coords;

      // REVIEW(akoen): M_numClassifiedVertices seems to give 0 sometimes.
      auto s_face_verts = M_classifiedVertexIter(this->s_mesh, s_face, 1);
      while (auto s_vertex = VIter_next(s_face_verts)) {
        auto s_vertex_id = EN_id(s_vertex);
        node_tags.push_back(s_vertex_id);
        double coord[3];
        V_coord(s_vertex, coord);
        node_coords.push_back(coord[0]);
        node_coords.push_back(coord[1]);
        node_coords.push_back(coord[2]);
      }
      gmsh::model::mesh::addNodes(2, tag, node_tags, node_coords);
      VIter_delete(s_face_verts);

      std::vector<int> element_types(1, 2);
      std::vector<std::vector<size_t>> element_tags(1);
      std::vector<std::vector<size_t>> element_nodes(1);

      auto s_mesh_faces = M_classifiedFaceIter(this->s_mesh, s_face, 1);
      while (auto s_mesh_face = FIter_next(s_mesh_faces)) {
        auto s_mesh_face_tag = EN_id(s_mesh_face);
        element_tags[0].push_back(s_mesh_face_tag);

        auto s_face_verts = F_vertices(s_mesh_face, 1);
        void *it2 = 0;
        std::vector<size_t> face_nodes;
        while (auto s_vertex = (pVertex)PList_next(s_face_verts, &it2)) {
          auto s_vertex_tag = EN_id(s_vertex);
          face_nodes.push_back(s_vertex_tag);
        }
        PList_delete(s_face_verts);

        element_nodes[0].insert(element_nodes[0].end(), face_nodes.begin(),
                                face_nodes.end());
      }
      gmsh::model::mesh::addElements(2, tag, element_types, element_tags,
                                     element_nodes);
      FIter_delete(s_mesh_faces);
    }
    PList_delete(s_faces);

    gmsh::model::addDiscreteEntity(3, tag, region_boundary_tags);
  }

  gmsh::write(filename);
  gmsh::finalize();
}