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
#include <array>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

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

auto Entity::get_tag() const -> int {
  return GEN_tag((pGEntity)this->s_model_item);
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

auto Entity::get_native_attributes() const -> nb::dict {
  nb::dict result;
  pGEntity ent = (pGEntity)this->s_model_item;

  int n = GEN_numNativeAttributeNames(ent, 0);
  if (n == 0) return result;

  std::vector<char *> names(n);
  GEN_nativeAttributeNames(ent, 0, names.data());

  for (int i = 0; i < n; i++) {
    const char *name = names[i];
    nb::list values;

    int ns = GEN_numNativeStringAttribute(ent, name);
    if (ns > 0) {
      std::vector<char *> strs(ns);
      GEN_nativeStringAttribute(ent, name, strs.data());
      for (int j = 0; j < ns; j++) {
        values.append(nb::str(strs[j]));
        Sim_deleteString(strs[j]);
      }
    }

    int ni = GEN_numNativeIntAttribute(ent, name);
    if (ni > 0) {
      std::vector<int> ints(ni);
      GEN_nativeIntAttribute(ent, name, ints.data());
      for (int j = 0; j < ni; j++)
        values.append(nb::int_(ints[j]));
    }

    int nd = GEN_numNativeDoubleAttribute(ent, name);
    if (nd > 0) {
      std::vector<double> dbls(nd);
      GEN_nativeDoubleAttribute(ent, name, dbls.data());
      for (int j = 0; j < nd; j++)
        values.append(nb::float_(dbls[j]));
    }

    result[name] = values;
    Sim_deleteString(names[i]);
  }
  return result;
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

auto Part::get_native_attributes() const -> nb::dict {
  nb::dict result;
  pGIPart part = (pGIPart)this->s_model_item;

  int n = GIP_numNativeAttributeNames(part, 0);
  if (n == 0) return result;

  std::vector<char *> names(n);
  GIP_nativeAttributeNames(part, 0, names.data());

  for (int i = 0; i < n; i++) {
    const char *name = names[i];
    nb::list values;

    int ns = GIP_numNativeStringAttribute(part, name);
    if (ns > 0) {
      std::vector<char *> strs(ns);
      GIP_nativeStringAttribute(part, name, strs.data());
      for (int j = 0; j < ns; j++) {
        values.append(nb::str(strs[j]));
        Sim_deleteString(strs[j]);
      }
    }

    int ni = GIP_numNativeIntAttribute(part, name);
    if (ni > 0) {
      std::vector<int> ints(ni);
      GIP_nativeIntAttribute(part, name, ints.data());
      for (int j = 0; j < ni; j++)
        values.append(nb::int_(ints[j]));
    }

    int nd = GIP_numNativeDoubleAttribute(part, name);
    if (nd > 0) {
      std::vector<double> dbls(nd);
      GIP_nativeDoubleAttribute(part, name, dbls.data());
      for (int j = 0; j < nd; j++)
        values.append(nb::float_(dbls[j]));
    }

    result[name] = values;
    Sim_deleteString(names[i]);
  }
  return result;
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

auto Assembly::get_native_attributes() const -> nb::dict {
  nb::dict result;
  pGAssembly assem = (pGAssembly)this->s_model_item;

  int n = GA_numNativeAttributeNames(assem, 0);
  if (n == 0) return result;

  std::vector<char *> names(n);
  GA_nativeAttributeNames(assem, 0, names.data());

  for (int i = 0; i < n; i++) {
    const char *name = names[i];
    nb::list values;

    int ns = GA_numNativeStringAttribute(assem, name);
    if (ns > 0) {
      std::vector<char *> strs(ns);
      GA_nativeStringAttribute(assem, name, strs.data());
      for (int j = 0; j < ns; j++) {
        values.append(nb::str(strs[j]));
        Sim_deleteString(strs[j]);
      }
    }

    int ni = GA_numNativeIntAttribute(assem, name);
    if (ni > 0) {
      std::vector<int> ints(ni);
      GA_nativeIntAttribute(assem, name, ints.data());
      for (int j = 0; j < ni; j++)
        values.append(nb::int_(ints[j]));
    }

    int nd = GA_numNativeDoubleAttribute(assem, name);
    if (nd > 0) {
      std::vector<double> dbls(nd);
      GA_nativeDoubleAttribute(assem, name, dbls.data());
      for (int j = 0; j < nd; j++)
        values.append(nb::float_(dbls[j]));
    }

    result[name] = values;
    Sim_deleteString(names[i]);
  }
  return result;
}

Part::Part(pModelItem s_model_item, nb::ref<Model> model)
    : ModelItem(s_model_item, model) {
  auto s_iter = GIP_regionIter((pGIPart)s_model_item);
  int count = 0;
  while (GRIter_next(s_iter))
    count++;
  GRIter_delete(s_iter);
  if (count != 1)
    throw_exception("Expected Part to have exactly 1 region, got {}.", count);
}

auto Part::get_regions() const -> std::vector<nb::ref<Region>> {
  std::vector<nb::ref<Region>> regions;
  auto s_iter = GIP_regionIter((pGIPart)this->s_model_item);
  while (auto s_region = GRIter_next(s_iter)) {
    regions.push_back({new Region(s_region, this->model)});
  }
  GRIter_delete(s_iter);
  return regions;
}

auto Part::get_faces() const -> std::vector<nb::ref<Face>> {
  std::vector<nb::ref<Face>> faces;
  auto s_iter = GIP_faceIter((pGIPart)this->s_model_item);
  while (auto s_face = GFIter_next(s_iter)) {
    faces.push_back({new Face(s_face, this->model)});
  }
  GFIter_delete(s_iter);
  return faces;
}

auto Assembly::get_assemblies() const -> std::vector<nb::ref<Assembly>> {
  std::vector<nb::ref<Assembly>> assemblies;
  auto s_iter = GA_AIter((pGAssembly)this->s_model_item, 2);
  while (auto s_assembly = GAIter_next(s_iter)) {
    assemblies.push_back({new Assembly(s_assembly, this->model)});
  }
  GAIter_delete(s_iter);
  return assemblies;
}

auto Assembly::get_parts() const -> std::vector<nb::ref<Part>> {
  std::vector<nb::ref<Part>> parts;
  auto s_iter = GA_IPIter((pGAssembly)this->s_model_item, 2);
  while (auto s_part = GIPIter_next(s_iter)) {
    parts.push_back({new Part(s_part, this->model)});
  }
  GIPIter_delete(s_iter);
  return parts;
}

auto Face::get_forward_region() const -> std::optional<nb::ref<Region>> {
  auto s_region = GF_region(static_cast<pGFace>(this->s_model_item), 0);
  if (!s_region)
    return std::nullopt;
  return {new Region(s_region, this->model)};
}

auto Face::get_reverse_region() const -> std::optional<nb::ref<Region>> {
  auto s_region = GF_region(static_cast<pGFace>(this->s_model_item), 1);
  if (!s_region)
    return std::nullopt;
  return {new Region(s_region, this->model)};
}

auto Region::get_centroid() const -> std::array<double, 3> {
  std::array<double, 3> cen;
  GR_centroid(static_cast<pGRegion>(this->s_model_item), 1.0, cen.data());
  return cen;
}

auto Part::get_centroid() const -> std::array<double, 3> {
  auto regions = this->get_regions();
  return regions.at(0)->get_centroid();
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
  auto is_valid = GM_isValid(this->s_model, 2, error_list);
  PList_delete(error_list);
  return is_valid;
}

void Model::write(std::string filename) {
  // GM_write(this->s_model, filename.c_str(), 0, nullptr);
  pNativeModel nonManNative = GM_nativeModel(this->s_model);
  NM_write(nonManNative, filename.c_str(), nullptr);
}

using AttrMap = std::unordered_map<std::string, std::deque<nlohmann::json>>;

// A body from the sidecar JSON, with its world-space centroid and the
// component-level attributes that should be applied to the matching pGIPart.
struct SidecarBody {
  std::array<double, 3> centroid;
  nlohmann::json attributes;
  std::string component_name; // e.g. "0012318_332"
  std::string material;       // short DAGMC name: {db_part_no}_{instance}.b{N}
  std::string path;           // ancestor DB_PART_NOs joined by "/"
  std::string instance;       // instance number as string
  std::string body;           // body name from NX journal
  std::string db_part_name;   // human-readable part name
  std::string db_part_no;     // part number
  int body_index;             // 1-based position in component's body list
  bool matched = false;
};

static void set_part_attrs(pGIPart part, const nlohmann::json &attrs) {
  for (auto &[key, val] : attrs.items()) {
    if (val.is_string())
      GIP_setNativeStringAttribute(part, val.get<std::string>().c_str(),
                                   key.c_str());
    else if (val.is_boolean())
      GIP_setNativeIntAttribute(part, val.get<bool>() ? 1 : 0, key.c_str());
    else if (val.is_number_integer())
      GIP_setNativeIntAttribute(part, val.get<int>(), key.c_str());
    else if (val.is_number_float())
      GIP_setNativeDoubleAttribute(part, val.get<double>(), key.c_str());
  }
}

static void set_assembly_attrs(pGAssembly assem, const nlohmann::json &attrs) {
  for (auto &[key, val] : attrs.items()) {
    if (val.is_string())
      GA_setNativeStringAttribute(assem, val.get<std::string>().c_str(),
                                  key.c_str());
    else if (val.is_boolean())
      GA_setNativeIntAttribute(assem, val.get<bool>() ? 1 : 0, key.c_str());
    else if (val.is_number_integer())
      GA_setNativeIntAttribute(assem, val.get<int>(), key.c_str());
    else if (val.is_number_float())
      GA_setNativeDoubleAttribute(assem, val.get<double>(), key.c_str());
  }
}

// Find the nearest unmatched SidecarBody to the given centroid.
// Returns nullptr if no bodies remain or the nearest distance exceeds a
// reasonable threshold.
static SidecarBody *find_nearest_body(std::vector<SidecarBody> &bodies,
                                      const double cen[3]) {
  SidecarBody *best = nullptr;
  double best_dist2 = std::numeric_limits<double>::max();
  for (auto &b : bodies) {
    if (b.matched)
      continue;
    double dx = b.centroid[0] - cen[0];
    double dy = b.centroid[1] - cen[1];
    double dz = b.centroid[2] - cen[2];
    double d2 = dx * dx + dy * dy + dz * dz;
    if (d2 < best_dist2) {
      best_dist2 = d2;
      best = &b;
    }
  }
  if (best) {
    spdlog::debug("Matched part at ({:.3f}, {:.3f}, {:.3f}) to component '{}' "
                  "(dist={:.4f})",
                  cen[0], cen[1], cen[2], best->component_name,
                  std::sqrt(best_dist2));
  }
  return best;
}

// Match parts in an assembly to sidecar bodies by centroid proximity.
// If fallback_attrs is non-null, unmatched parts get those attrs.
static void match_parts_by_centroid(pGAssembly assem,
                                    const std::string &assem_name,
                                    std::vector<SidecarBody> &sidecar_bodies,
                                    const nlohmann::json *fallback_attrs) {
  auto p_iter = GA_IPIter(assem, 2);
  while (auto part = GIPIter_next(p_iter)) {
    auto r_iter = GIP_regionIter(part);
    auto region = GRIter_next(r_iter);
    GRIter_delete(r_iter);
    if (!region) {
      spdlog::warn("Part in assembly '{}' has no region, skipping",
                   assem_name);
      continue;
    }

    double cen[3];
    GR_centroid(region, 1.0, cen);

    auto *match = find_nearest_body(sidecar_bodies, cen);
    if (match) {
      set_part_attrs(part, match->attributes);
      GIP_setNativeStringAttribute(
          part, match->material.c_str(), "NX_MATERIAL");
      GIP_setNativeStringAttribute(
          part, match->path.c_str(), "NX_PATH");
      GIP_setNativeStringAttribute(
          part, match->component_name.c_str(), "NX_COMPONENT");
      GIP_setNativeStringAttribute(
          part, match->instance.c_str(), "NX_INSTANCE");
      GIP_setNativeStringAttribute(
          part, match->body.c_str(), "NX_BODY");
      match->matched = true;
    } else if (fallback_attrs) {
      set_part_attrs(part, *fallback_attrs);
    }
  }
  GIPIter_delete(p_iter);
}

static void apply_nx_attrs(pGAssembly assem, AttrMap &attr_map,
                           std::vector<SidecarBody> &sidecar_bodies) {
  char *name = GA_nativeName(assem);
  std::string name_str = name ? name : "<root>";
  if (name)
    Sim_deleteString(name);

  if (name) {
    // Look up assembly in attr_map. If exact name fails (e.g. "0012318_A"),
    // try stripping the Parasolid "_A" suffix to match the NX part stem.
    auto it = attr_map.find(name_str);
    if (it == attr_map.end() || it->second.empty()) {
      if (name_str.size() > 2 &&
          name_str.substr(name_str.size() - 2) == "_A") {
        auto base = name_str.substr(0, name_str.size() - 2);
        it = attr_map.find(base);
      }
    }

    if (it != attr_map.end() && !it->second.empty()) {
      const auto &attrs = it->second.front();
      set_assembly_attrs(assem, attrs);
      match_parts_by_centroid(assem, name_str, sidecar_bodies, &attrs);
      it->second.pop_front();
    }
  } else {
    // Unnamed root assembly — still try centroid matching for direct parts
    match_parts_by_centroid(assem, name_str, sidecar_bodies, nullptr);
  }

  auto a_iter = GA_AIter(assem, 2);
  while (auto child = GAIter_next(a_iter))
    apply_nx_attrs(child, attr_map, sidecar_bodies);
  GAIter_delete(a_iter);
}

auto Model::read(std::string filename) -> nb::ref<Model> {
  auto model = GM_load(filename.c_str(), nullptr, nullptr);

  return {new Model(model)};
}

auto Model::from_parasolid_file(std::string filename, bool load_nx_attrs)
    -> nb::ref<Model> {
  spdlog::debug("Loading Parasolid file.");
  auto parasolid_native_model = ParasolidNM_createFromFile(filename.c_str(), 0);
  if (!parasolid_native_model) {
    throw std::runtime_error("failed to load Parasolid file: " + filename);
  }
  spdlog::debug("Creating GAM assembly model.");
  auto gam_model = GAM_createFromNativeModel(parasolid_native_model, nullptr);

  if (load_nx_attrs) {
    namespace fs = std::filesystem;
    fs::path p(filename);
    auto attrs_path = p.parent_path() / (p.stem().string() + "_attrs.json");
    if (!fs::exists(attrs_path)) {
      spdlog::warn("load_nx_attrs=true but {} not found", attrs_path.string());
    } else {
      spdlog::debug("Loading NX attributes from {}", attrs_path.string());
      std::ifstream f(attrs_path);
      auto j = nlohmann::json::parse(f);

      auto version = j.value("schema_version", 0);
      if (version != 6) {
        throw std::runtime_error(
            "Sidecar schema version " + std::to_string(version) +
            " is not supported; re-export with the updated NX journal"
            " (expected version 6)");
      }

      AttrMap attr_map;
      std::vector<SidecarBody> sidecar_bodies;
      std::unordered_set<std::string> seen_bases;

      // Parse components dict and build attr_map.
      // Each component key is unique in the dict, so no deduplication needed
      // for component_name. Only seen_bases is needed because multiple
      // component keys can share the same part_file_stem (e.g. 332 instances
      // of "nnnnnnn" all map to stem "0012318_A").
      auto &components = j["components"];
      for (auto &[comp_key, comp_node] : components.items()) {
        auto comp_name = comp_node["component_name"].get<std::string>();
        attr_map[comp_name].push_back(comp_node["attributes"]);
        auto stem = comp_node["part_file_stem"].get<std::string>();
        if (stem != comp_name && seen_bases.insert(stem).second)
          attr_map[stem].push_back(comp_node["attributes"]);
      }

      // Parse bodies, resolving component references
      for (const auto &body_node : j["bodies"]) {
        auto comp_key = body_node["component"].get<std::string>();
        auto &comp_node = components.at(comp_key);

        SidecarBody entry;
        auto &c = body_node["centroid"];
        entry.centroid = {c[0].get<double>(), c[1].get<double>(),
                          c[2].get<double>()};
        entry.body_index = body_node["body_index"].get<int>();
        entry.body = body_node["body_name"].get<std::string>();

        // Derive material_slug: component_key + ".b" + body_index
        entry.material = comp_key + ".b" + std::to_string(entry.body_index);

        // Component-level fields
        entry.component_name =
            comp_node["component_name"].get<std::string>();
        entry.path = comp_node["path"].get<std::string>();
        entry.db_part_name = comp_node["db_part_name"].get<std::string>();
        entry.db_part_no = comp_node["db_part_no"].get<std::string>();
        entry.instance = comp_node["instance"].get<std::string>();
        entry.attributes = comp_node["attributes"];

        sidecar_bodies.push_back(std::move(entry));
      }

      spdlog::info("Loaded {} body centroid(s) from sidecar for matching",
                    sidecar_bodies.size());

      auto root_items = GM_rootItems(gam_model);
      void *iter = nullptr;
      while (auto s_item =
                 static_cast<pModelItem>(PList_next(root_items, &iter))) {
        auto type = ModelItem_type(s_item);
        if (type == Gassembly) {
          apply_nx_attrs((pGAssembly)s_item, attr_map, sidecar_bodies);
        }
      }
      PList_delete(root_items);

      // Report unmatched sidecar bodies
      int unmatched = 0;
      for (const auto &b : sidecar_bodies) {
        if (!b.matched) {
          spdlog::warn("Sidecar body from component '{}' at ({:.3f}, {:.3f}, "
                       "{:.3f}) was not matched to any Part",
                       b.component_name, b.centroid[0], b.centroid[1],
                       b.centroid[2]);
          unmatched++;
        }
      }
      if (unmatched > 0)
        spdlog::warn("{} sidecar bodies were not matched", unmatched);
    }
  }

  return {new Model(gam_model)};
}

auto Model::translate() -> nb::ref<Model> {
  auto connector = MC_new();
  spdlog::debug("Translating GAM model to SMS native.");
  auto s_translated = GM_translateModel(this->s_model, connector, true);
  return {new Model(s_translated, nb::ref<Model>(this), connector)};
}

auto Model::make_non_manifold_model(std::optional<double> tolerance)
    -> nb::ref<Model> {
  auto connector = MC_new();

  auto model_builder = ModelBuilder_new(this->s_model);
  ModelBuilder_setConnector(model_builder, connector);
  if (tolerance.has_value()) {
    ModelBuilder_setBooleanTolerance(model_builder, tolerance.value());
  }
  spdlog::debug("Creating Non-Manifold Model.");
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

static std::string get_native_string_attr(pGIPart part, const char *attr_name) {
  int n = GIP_numNativeStringAttribute(part, attr_name);
  if (n <= 0)
    return "";
  std::vector<char *> strs(n);
  GIP_nativeStringAttribute(part, attr_name, strs.data());
  std::string result = strs[0];
  for (int j = 0; j < n; j++)
    Sim_deleteString(strs[j]);
  return result;
}

void Mesh::write_gmsh(std::string filename, double scale_factor) {
  gmsh::initialize();
  gmsh::model::add("phnx");

  unsigned int dim;
  unsigned int gmsh_element_type;

  // Note(akoen): Add regions for tet/hex meshing

  // Gmsh Point / GeomSim Vertex
  dim = 0;
  gmsh_element_type = 15;

  auto vit = M_vertexIter(this->s_mesh);
  size_t next_element_tag = 1;
  while (auto v = VIter_next(vit)) {
    EN_setID((pEntity)v, next_element_tag++);

    auto adjacent_faces = V_faces(v);

    auto num_faces = PList_size(adjacent_faces);
    if (num_faces == 0) {
      spdlog::warn("Vertex {} is not adjacent to any mesh faces",
                   next_element_tag, num_faces);
    }
    PList_delete(adjacent_faces);
  }
  VIter_delete(vit);

  for (auto entity : this->model->get_vertices()) {
    auto sg_entity = static_cast<pGEntity>(entity->s_model_item);
    auto s_classified_verts =
        M_classifiedVertexIter(this->s_mesh, sg_entity, 0);

    std::vector<size_t> node_tags;
    std::vector<double> node_coords;
    while (auto s_vertex = VIter_next(s_classified_verts)) {
      auto s_vertex_id = EN_id(s_vertex);
      node_tags.push_back(s_vertex_id);
      double coord[3];
      V_coord(s_vertex, coord);
      node_coords.push_back(coord[0] * scale_factor);
      node_coords.push_back(coord[1] * scale_factor);
      node_coords.push_back(coord[2] * scale_factor);
    }

    auto sg_tag = GEN_tag(sg_entity);
    gmsh::model::addDiscreteEntity(dim, sg_tag);
    gmsh::model::mesh::addNodes(dim, sg_tag, node_tags, node_coords);

    std::vector<int> element_types(1, gmsh_element_type);
    std::vector<std::vector<size_t>> element_tags(1);
    std::vector<std::vector<size_t>> element_nodes(1);
    auto sm_elements = M_classifiedVertexIter(this->s_mesh, sg_entity, 0);
    while (auto sm_element = VIter_next(sm_elements)) {
      // Node ID is same as point ID
      // TODO(akoen): We shouldn't need to iterate
      auto sm_tag = EN_id(sm_element);
      element_tags[0].push_back(sm_tag);
      element_nodes[0].push_back(sm_tag);
    }
    gmsh::model::mesh::addElements(dim, sg_tag, element_types, element_tags,
                                   element_nodes);
    VIter_delete(sm_elements);
  }

  // Edge
  dim = 1;
  gmsh_element_type = 1;
  auto eit = M_edgeIter(this->s_mesh);
  while (auto e = EIter_next(eit)) {
    EN_setID((pEntity)e, next_element_tag++);
  }
  EIter_delete(eit);

  for (auto entity : this->model->get_edges()) {
    auto sg_entity = static_cast<pGEntity>(entity->s_model_item);
    auto s_classified_verts =
        M_classifiedVertexIter(this->s_mesh, sg_entity, 0);

    std::vector<size_t> node_tags;
    std::vector<double> node_coords;
    while (auto s_vertex = VIter_next(s_classified_verts)) {
      auto s_vertex_id = EN_id(s_vertex);
      node_tags.push_back(s_vertex_id);
      double coord[3];
      V_coord(s_vertex, coord);
      node_coords.push_back(coord[0] * scale_factor);
      node_coords.push_back(coord[1] * scale_factor);
      node_coords.push_back(coord[2] * scale_factor);
    }

    auto sg_tag = GEN_tag(sg_entity);

    // Add boundary vertices to the discrete edge
    std::vector<int> boundary_tags;
    auto s_vertices = GEN_vertices(sg_entity);
    void *iter = 0;
    while (auto s_vertex = (pGEntity)PList_next(s_vertices, &iter)) {
      boundary_tags.push_back(GEN_tag(s_vertex));
    }
    PList_delete(s_vertices);

    gmsh::model::addDiscreteEntity(dim, sg_tag, boundary_tags);
    gmsh::model::mesh::addNodes(dim, sg_tag, node_tags, node_coords);

    std::vector<int> element_types(1, gmsh_element_type);
    std::vector<std::vector<size_t>> element_tags(1);
    std::vector<std::vector<size_t>> element_nodes(1);
    auto sm_elements = M_classifiedEdgeIter(this->s_mesh, sg_entity, 0);
    while (auto sm_element = EIter_next(sm_elements)) {
      auto sm_tag = EN_id(sm_element);
      element_tags[0].push_back(sm_tag);

      element_nodes[0].push_back(EN_id(E_vertex(sm_element, 0)));
      element_nodes[0].push_back(EN_id(E_vertex(sm_element, 1)));
    }
    gmsh::model::mesh::addElements(dim, sg_tag, element_types, element_tags,
                                   element_nodes);
    EIter_delete(sm_elements);
  }

  // Face
  dim = 2;
  gmsh_element_type = 2;

  auto fit = M_faceIter(this->s_mesh);
  while (auto f = FIter_next(fit)) {
    EN_setID((pEntity)f, next_element_tag++);
  }
  FIter_delete(fit);

  for (auto entity : this->model->get_faces()) {
    auto sg_entity = static_cast<pGEntity>(entity->s_model_item);
    auto sg_tag = GEN_tag(sg_entity);
    auto s_classified_verts =
        M_classifiedVertexIter(this->s_mesh, sg_entity, 0);

    std::vector<size_t> node_tags;
    std::vector<double> node_coords;
    while (auto s_vertex = VIter_next(s_classified_verts)) {
      auto s_vertex_id = EN_id(s_vertex);
      node_tags.push_back(s_vertex_id);
      double coord[3];
      V_coord(s_vertex, coord);
      node_coords.push_back(coord[0] * scale_factor);
      node_coords.push_back(coord[1] * scale_factor);
      node_coords.push_back(coord[2] * scale_factor);
    }

    // NEW: Add boundary edges to the discrete face
    std::vector<int> boundary_tags;
    auto s_edges = GEN_edges(sg_entity);
    void *iter = 0;
    while (auto s_edge = (pGEntity)PList_next(s_edges, &iter)) {
      boundary_tags.push_back(GEN_tag(s_edge));
    }
    PList_delete(s_edges);

    gmsh::model::addDiscreteEntity(dim, sg_tag, boundary_tags);
    gmsh::model::mesh::addNodes(dim, sg_tag, node_tags, node_coords);

    auto sg_forward_volume = GF_region(static_cast<pGFace>(sg_entity), 0);
    std::string forward_volume =
        sg_forward_volume == nullptr
            ? "None"
            : std::to_string(GEN_tag(sg_forward_volume));

    auto sg_reverse_volume = GF_region(static_cast<pGFace>(sg_entity), 1);
    std::string reverse_volume =
        sg_reverse_volume == nullptr
            ? "None"
            : std::to_string(GEN_tag(sg_reverse_volume));

    std::stringstream physical_name;
    physical_name << "tag=" << sg_tag << "&forward_volume=" << forward_volume
                  << "&reverse_volume=" << reverse_volume;

    auto physical_tag = gmsh::model::addPhysicalGroup(dim, {sg_tag}, sg_tag,
                                                      physical_name.str());

    std::vector<int> element_types(1, gmsh_element_type);
    std::vector<std::vector<size_t>> element_tags(1);
    std::vector<std::vector<size_t>> element_nodes(1);
    auto sm_elements = M_classifiedFaceIter(this->s_mesh, sg_entity, 0);
    while (auto sm_element = FIter_next(sm_elements)) {
      auto s_mesh_face_tag = EN_id(sm_element);
      element_tags[0].push_back(s_mesh_face_tag);

      auto s_face_verts = F_vertices(sm_element, 1);
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
    gmsh::model::mesh::addElements(dim, sg_tag, element_types, element_tags,
                                   element_nodes);

    FIter_delete(sm_elements);
  }
  // Region
  dim = 3;

  // TODO(akoen): Support hexahedral meshing
  gmsh_element_type = 4;

  for (auto entity : this->model->get_regions()) {
    auto sg_entity = static_cast<pGEntity>(entity->s_model_item);
    auto sg_tag = GEN_tag(sg_entity);

    auto s_classified_verts =
        M_classifiedVertexIter(this->s_mesh, sg_entity, 0);

    std::vector<size_t> node_tags;
    std::vector<double> node_coords;
    while (auto s_vertex = VIter_next(s_classified_verts)) {
      auto s_vertex_id = EN_id(s_vertex);
      node_tags.push_back(s_vertex_id);
      double coord[3];
      V_coord(s_vertex, coord);
      node_coords.push_back(coord[0] * scale_factor);
      node_coords.push_back(coord[1] * scale_factor);
      node_coords.push_back(coord[2] * scale_factor);
    }

    std::vector<int> boundary_tags;
    auto s_faces = GEN_faces(sg_entity);
    void *iter = 0;
    while (auto s_edge = (pGEntity)PList_next(s_faces, &iter)) {
      boundary_tags.push_back(GEN_tag(s_edge));
    }
    PList_delete(s_faces);

    gmsh::model::addDiscreteEntity(dim, sg_tag, boundary_tags);
    gmsh::model::mesh::addNodes(dim, sg_tag, node_tags, node_coords);

    // Create physical group with material name
    auto related_parts = entity->get_related_parts();
    if (related_parts.size() == 0) {
      spdlog::warn("Region {} has no related parts", sg_tag);
    } else if (related_parts.size() > 1) {
      spdlog::warn("Region {} has more than one related part", sg_tag);
    } else {
      auto part = related_parts.at(0);
      auto s_part = (pGIPart)part->s_model_item;
      auto material_name = get_native_string_attr(s_part, "NX_MATERIAL");
      if (material_name.empty()) {
        // Non-NX fallback: leaf assembly name + region tag
        auto parent = part->get_parent_assembly();
        if (parent.has_value()) {
          auto aname = parent->get()->get_name();
          if (aname.has_value())
            material_name = aname.value() + ".b" + std::to_string(sg_tag);
        }
        if (material_name.empty())
          material_name = "unknown_" + std::to_string(sg_tag);
      }

      std::stringstream physical_name;
      physical_name << "tag=" << sg_tag << "&material=" << material_name;
      gmsh::model::addPhysicalGroup(dim, {sg_tag}, sg_tag,
                                    physical_name.str());
    }

    std::vector<int> element_types(1, gmsh_element_type);
    std::vector<std::vector<size_t>> element_tags(1);
    std::vector<std::vector<size_t>> element_nodes(1);
    auto sm_elements = M_classifiedRegionIter(this->s_mesh, sg_entity);
    while (auto sm_element = RIter_next(sm_elements)) {
      auto s_mesh_face_tag = EN_id(sm_element);
      element_tags[0].push_back(s_mesh_face_tag);

      auto s_element = R_vertices(sm_element, 1);
      void *it2 = 0;
      std::vector<size_t> region_nodes;
      while (auto s_vertex = (pVertex)PList_next(s_element, &it2)) {
        auto s_vertex_tag = EN_id(s_vertex);
        region_nodes.push_back(s_vertex_tag);
      }
      PList_delete(s_element);

      element_nodes[0].insert(element_nodes[0].end(), region_nodes.begin(),
                              region_nodes.end());
    }
    gmsh::model::mesh::addElements(dim, sg_tag, element_types, element_tags,
                                   element_nodes);

    RIter_delete(sm_elements);
  }

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
  // M_write(s_new_mesh, "meshv.sms", 0, nullptr);
  VolumeMesher_delete(s_volume_mesher);

  return {
      new VolumeMesh(s_new_mesh, surface_mesh->model, surface_mesh->mesh_case)};
}