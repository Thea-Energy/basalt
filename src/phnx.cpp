#include "phnx.h"
#include "AttributeTypes.h"
#include "MeshSim.h"
#include "MeshTypes.h"
#include "ModelEnums.h"
#include "ModelTypes.h"
#include "SimModel.h"
#include "SimPList.h"
#include "SimParasolidKrnl.h"
#include "gmsh.h"
#include "init.h"
#include "spdlog/spdlog.h"
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>

template <typename T> auto plist_to_vec(pPList plist) -> std::vector<T> {
  std::vector<T> vec;
  void *it2 = 0;
  std::vector<size_t> face_nodes;
  while (auto item = static_cast<T>(PList_next(plist, &it2))) {
    vec.push_back(item);
  }
  PList_delete(plist);
  return vec;
};

auto ModelItem::downcast(pModelItem s_model_item, nb::ref<Model> model)
    -> nb::ref<ModelItem> {
  auto s_type = ModelItem_type(s_model_item);
  switch (s_type) {
  case Gregion:
    return {new Region(s_model_item, model)};
  case Gassembly:
    return {new Assembly(s_model_item, model)};
  case GinstantiatedPart:
    return {new Part(s_model_item, model)};
  default:
    throw_exception("Downcast for gType {} not implemented", s_type);
  }
}

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

auto Entity::get_related_parts() const -> std::vector<nb::ref<Part>> {
  const auto &conn = this->model->require_connection();

  std::vector<nb::ref<Part>> parts;
  auto s_assem_parts = ANMConnection_relatedParts(
      conn.s_anm, static_cast<pGEntity>(this->s_model_item));

  void *iter = 0;
  while (auto s_part = (pGEntity)PList_next(s_assem_parts, &iter)) {
    parts.push_back({new Part(s_part, this->model)});
  }
  PList_delete(s_assem_parts);
  return parts;
}

auto Entity::get_name() const -> std::optional<std::string> {
  char *name = GEN_nativeName((pGEntity)this->s_model_item);
  if (name != nullptr) {
    std::string result(name);
    Sim_deleteString(name);
    return result;
  } else {
    return std::nullopt;
  }
}

auto Part::get_name() const -> std::optional<std::string> {
  char *name = GIP_nativeName((pGIPart)this->s_model_item);
  if (name != nullptr) {
    std::string result(name);
    Sim_deleteString(name);
    return result;
  } else {
    return std::nullopt;
  }
}

auto Part::get_parent_assembly() const -> std::optional<nb::ref<Assembly>> {
  auto s_parent_assembly = GIP_parentAssembly((pGIPart)this->s_model_item);
  return {new Assembly(s_parent_assembly, this->model)};
}

auto Assembly::get_name() const -> std::optional<std::string> {
  char *name = GA_nativeName((pGAssembly)this->s_model_item);
  if (name != nullptr) {
    std::string result(name);
    Sim_deleteString(name);
    return result;
  } else {
    return std::nullopt;
  }
}

auto Assembly::get_parent_assembly() const -> std::optional<nb::ref<Assembly>> {
  auto s_parent_assembly = GA_parentAssembly((pGAssembly)this->s_model_item);
  return {new Assembly(s_parent_assembly, this->model)};
}

Model::~Model() {
  if (sms_is_initialized())
    GM_release(s_model);
  connection.reset();
}

bool Model::is_assembly_model() const {
  return GM_isAssemblyModel(this->s_model);
}

auto Model::get_root_items() const -> std::vector<nb::ref<ModelItem>> {
  std::vector<nb::ref<ModelItem>> root_items;
  auto s_root_items = GM_rootItems(this->s_model);
  void *itr = nullptr;
  while (auto s_model_item =
             static_cast<pModelItem>(PList_next(s_root_items, &itr))) {
    root_items.push_back(ModelItem::downcast(
        s_model_item, nb::ref<Model>(const_cast<Model *>(this))));
  }
  PList_delete(s_root_items);
  return root_items;
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

auto Model::get_vertices() const -> std::vector<nb::ref<Vertex>> {
  std::vector<nb::ref<Vertex>> vertices;
  auto s_model_vertices = GM_vertexIter(this->s_model);
  while (auto s_model_vertex = GVIter_next(s_model_vertices)) {
    vertices.push_back({new Vertex(s_model_vertex,
                                   nb::ref<Model>(const_cast<Model *>(this)))});
  }
  GVIter_delete(s_model_vertices);
  return vertices;
}

auto Model::get_edges() const -> std::vector<nb::ref<Edge>> {
  std::vector<nb::ref<Edge>> edges;
  auto s_model_edges = GM_edgeIter(this->s_model);
  while (auto s_model_edge = GEIter_next(s_model_edges)) {
    edges.push_back(
        {new Edge(s_model_edge, nb::ref<Model>(const_cast<Model *>(this)))});
  }
  GEIter_delete(s_model_edges);
  return edges;
}

auto Model::get_faces() const -> std::vector<nb::ref<Face>> {
  std::vector<nb::ref<Face>> faces;

  auto s_model_faces = GM_faceIter(this->s_model);
  while (auto s_model_face = GFIter_next(s_model_faces)) {
    faces.push_back(
        {new Face(s_model_face, nb::ref<Model>(const_cast<Model *>(this)))});
  }
  GFIter_delete(s_model_faces);

  return faces;
}

auto Model::get_regions() const -> std::vector<nb::ref<Region>> {
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

void MeshCase::set_size(double mesh_size,
                        std::optional<nb::ref<ModelItem>> model_item) {
  auto s_model_item = model_item.has_value() ? model_item.value()->s_model_item
                                             : GM_domain(this->model->s_model);

  MS_setMeshSize(this->s_mesh_case, s_model_item, 2, mesh_size, 0);
}

void MeshCase::set_curvature_refinement(
    double value, bool relative, bool use_edges, std::optional<double> min_size,
    bool anisotropic, std::optional<nb::ref<ModelItem>> model_item) {
  auto s_model_item = model_item.has_value() ? model_item.value()->s_model_item
                                             : GM_domain(this->model->s_model);

  int s_type = relative ? 2 : 1;
  int s_calc_from = use_edges ? 3 : 2;
  if (anisotropic) {
    MS_setAnisoMeshCurv(this->s_mesh_case, s_model_item, s_type, value,
                        s_calc_from);
  } else {
    MS_setMeshCurv(this->s_mesh_case, s_model_item, s_type, value, s_calc_from);
  }

  if (min_size.has_value()) {
    MS_setMinCurvSize(this->s_mesh_case, s_model_item, s_type,
                      min_size.value());
  }
}

void MeshCase::set_proximity_refinement(
    double value, std::optional<double> min_size,
    std::optional<nb::ref<ModelItem>> model_item) {

  auto s_model_item = model_item.has_value() ? model_item.value()->s_model_item
                                             : GM_domain(this->model->s_model);

  MS_setProximityRefinement(this->s_mesh_case, s_model_item, value);

  if (min_size.has_value()) {
    MS_setMinProximitySize(this->s_mesh_case, s_model_item, min_size.value());
  }
}

void Mesh::write_gmsh(std::string filename) {
  gmsh::initialize();
  gmsh::model::add("phnx");

  // Assign node IDs
  auto vit = M_vertexIter(this->s_mesh);
  size_t vid = 1;
  while (auto v = VIter_next(vit)) {
    EN_setID((pEntity)v, vid++);
  }
  VIter_delete(vit);

  // Assign face IDs
  auto fit = M_faceIter(this->s_mesh);
  size_t fid = 1;
  while (auto f = FIter_next(fit)) {
    EN_setID((pEntity)f, fid++);
  }
  FIter_delete(fit);

  std::map<std::string, std::vector<int>> region_physical_name_map;

  for (auto region : this->model->get_regions()) {
    auto s_region_model_item = static_cast<pGEntity>(region->s_model_item);
    auto region_tag = GEN_tag(s_region_model_item);

    // Iterate through topologically faces of current region
    std::vector<int> region_boundary_tags;
    auto s_faces = GEN_faces(s_region_model_item);
    void *itr = 0;
    while (auto s_face = (pGFace)(PList_next(s_faces, &itr))) {
      auto current_face_tag = GEN_tag(s_face);
      region_boundary_tags.push_back(current_face_tag);
      try {
        gmsh::model::addDiscreteEntity(2, current_face_tag);
      } catch (const std::runtime_error &) {
        continue;
      }

      std::vector<size_t> node_tags;
      std::vector<double> node_coords;

      auto s_face_verts = M_classifiedVertexIter(this->s_mesh, s_face, 0);
      while (auto s_vertex = VIter_next(s_face_verts)) {
        auto s_vertex_id = EN_id(s_vertex);
        node_tags.push_back(s_vertex_id);
        double coord[3];
        V_coord(s_vertex, coord);
        node_coords.push_back(coord[0]);
        node_coords.push_back(coord[1]);
        node_coords.push_back(coord[2]);
      }
      gmsh::model::mesh::addNodes(2, current_face_tag, node_tags, node_coords);
      VIter_delete(s_face_verts);

      std::vector<int> element_types(1, 2);
      std::vector<std::vector<size_t>> element_tags(1);
      std::vector<std::vector<size_t>> element_nodes(1);

      // Iterate through mesh faces(Gmsh elements)
      auto s_mesh_faces = M_classifiedFaceIter(this->s_mesh, s_face, 0);
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
      gmsh::model::mesh::addElements(2, current_face_tag, element_types,
                                     element_tags, element_nodes);
      FIter_delete(s_mesh_faces);
    }
    PList_delete(s_faces);

    gmsh::model::addDiscreteEntity(3, region_tag, region_boundary_tags);

    // Create physical groups for part/component names
    auto related_parts = region->get_related_parts();
    if (related_parts.size() == 0) {
      spdlog::warn("Region has no related parts");
    } else if (related_parts.size() > 1) {
      spdlog::warn("Region has more than one related part.");
    } else {
      auto name = related_parts.at(0)->get_name();
      if (!name.has_value()) {
        name = related_parts.at(0)->get_parent_assembly()->get()->get_name();
      }
      if (!name.has_value()) {
        spdlog::warn("Related part has no name.");
      } else {
        region_physical_name_map[name.value()].push_back(region_tag);
      }
    }
  }

  for (const auto &[name, region_tags] : region_physical_name_map) {
    auto physical_tag = gmsh::model::addPhysicalGroup(3, region_tags);
    gmsh::model::setPhysicalName(3, physical_tag, "mat:" + name);
  }

  // spdlog::debug("Started node deduplication");
  // gmsh::model::mesh::removeDuplicateNodes();
  // spdlog::debug("Finished node deduplication");

  gmsh::option::setNumber("Mesh.SaveAll", 1);
  gmsh::write(filename);
  gmsh::finalize();
}

auto SurfaceMesh::from_model(nb::ref<Model> model, nb::ref<MeshCase> mesh_case)
    -> nb::ref<Mesh> {
  auto s_mesh = M_new(0, model->s_model);
  auto s_mesh_case = mesh_case->s_mesh_case;
  auto s_surface_mesher = SurfaceMesher_new(s_mesh_case, s_mesh);

  SurfaceMesher_execute(s_surface_mesher, nullptr);
  M_write(s_mesh, "mesh.sms", 0, nullptr);

  SurfaceMesher_delete(s_surface_mesher);

  return {new SurfaceMesh(s_mesh, model, mesh_case)};
}

auto VolumeMesh::from_surface_mesh(nb::ref<SurfaceMesh> surface_mesh)
    -> nb::ref<VolumeMesh> {
  auto s_new_mesh = M_copy(surface_mesh->s_mesh, 1);
  auto s_volume_mesher =
      VolumeMesher_new(surface_mesh->mesh_case->s_mesh_case, s_new_mesh);
  VolumeMesher_execute(s_volume_mesher, nullptr);
  M_write(s_new_mesh, "meshv.sms", 0, nullptr);
  VolumeMesher_delete(s_volume_mesher);

  return {
      new VolumeMesh(s_new_mesh, surface_mesh->model, surface_mesh->mesh_case)};
}