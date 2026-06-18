// Basalt — NX journal: export a Parasolid assembly plus a v6 *_attrs.json
// metadata sidecar for ingestion via
// Model.from_parasolid_file(..., load_nx_attrs=True). Run inside Siemens NX
// against the open assembly; see the Basalt "Geometry sources & NX" docs page.
//
// Reads BASALT_NX_EXPORT_DIR; hard-fails if unset.
//
// Compiler environment: NX 2406 .NET Framework 4.x journal compiler.
// - System.Text.Json is unavailable (.NET Core only).
// - Newtonsoft.Json is present in managed/ but not auto-referenced by the compiler.
// - HashSet<T> from mscorlib is unavailable in this compiler context.
// JSON is written manually via a custom lightweight builder.
// Sets are simulated with Dictionary<Body, bool>.
// C# 7+ value-tuple syntax is also unavailable; out parameters used instead.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using NXOpen;
using NXOpen.Assemblies;

public class ExportAttrs
{
    // Identity transform constants
    static readonly double[][] IdentityRot = new double[][]
    {
        new double[] { 1.0, 0.0, 0.0 },
        new double[] { 0.0, 1.0, 0.0 },
        new double[] { 0.0, 0.0, 1.0 },
    };
    static readonly double[] IdentityOrigin = new double[] { 0.0, 0.0, 0.0 };

    // ---------------------------------------------------------------------------
    // Lightweight JSON builder: insertion-order-preserving, no external deps.
    // Produces correctly escaped, indented JSON.
    // ---------------------------------------------------------------------------

    static string JsonString(string s)
    {
        if (s == null) s = "";
        var sb = new StringBuilder("\"");
        foreach (char c in s)
        {
            switch (c)
            {
                case '"':  sb.Append("\\\""); break;
                case '\\': sb.Append("\\\\"); break;
                case '\n': sb.Append("\\n");  break;
                case '\r': sb.Append("\\r");  break;
                case '\t': sb.Append("\\t");  break;
                default:
                    if (c < 0x20)
                        sb.AppendFormat("\\u{0:x4}", (int)c);
                    else
                        sb.Append(c);
                    break;
            }
        }
        sb.Append('"');
        return sb.ToString();
    }

    static string JsonDouble(double v)
    {
        // Reproduce Python's json.dump behavior: full precision, no trailing zeros.
        return v.ToString("R");
    }

    // A minimal ordered map for JSON-safe string keys -> raw JSON values.
    class JsonObj
    {
        private readonly List<string> _keys = new List<string>();
        private readonly Dictionary<string, string> _vals = new Dictionary<string, string>();

        public void Add(string key, string rawValue)
        {
            if (!_vals.ContainsKey(key))
                _keys.Add(key);
            _vals[key] = rawValue;
        }

        public string GetRaw(string key)
        {
            string v;
            return _vals.TryGetValue(key, out v) ? v : null;
        }

        public bool ContainsKey(string key) { return _vals.ContainsKey(key); }

        public List<string> Keys { get { return _keys; } }

        public string ToJson(int indent)
        {
            if (_keys.Count == 0) return "{}";
            var pad = new string(' ', indent);
            var innerPad = new string(' ', indent + 2);
            var sb = new StringBuilder("{\n");
            for (int i = 0; i < _keys.Count; i++)
            {
                sb.Append(innerPad);
                sb.Append(JsonString(_keys[i]));
                sb.Append(": ");
                // Indent nested objects/arrays that already contain newlines
                string val = _vals[_keys[i]];
                sb.Append(val);
                if (i < _keys.Count - 1) sb.Append(",");
                sb.Append("\n");
            }
            sb.Append(pad);
            sb.Append("}");
            return sb.ToString();
        }
    }

    // Serialize a List<string> of raw JSON values as a JSON array.
    static string JsonArray(List<string> items, int indent)
    {
        if (items.Count == 0) return "[]";
        var pad = new string(' ', indent);
        var innerPad = new string(' ', indent + 2);
        var sb = new StringBuilder("[\n");
        for (int i = 0; i < items.Count; i++)
        {
            sb.Append(innerPad);
            sb.Append(items[i]);
            if (i < items.Count - 1) sb.Append(",");
            sb.Append("\n");
        }
        sb.Append(pad);
        sb.Append("]");
        return sb.ToString();
    }

    // Serialize a double[3] centroid as a compact inline JSON array.
    static string CentroidJson(double x, double y, double z)
    {
        return "[" + JsonDouble(x) + ", " + JsonDouble(y) + ", " + JsonDouble(z) + "]";
    }

    // ---------------------------------------------------------------------------
    // NX attribute collection
    // ---------------------------------------------------------------------------

    static JsonObj CollectAttributes(NXObject.AttributeInformation[] attrInfos)
    {
        var result = new JsonObj();
        foreach (var a in attrInfos)
        {
            switch (a.Type)
            {
                case NXObject.AttributeType.String:
                    result.Add(a.Title, JsonString(a.StringValue));
                    break;
                case NXObject.AttributeType.Integer:
                    result.Add(a.Title, a.IntegerValue.ToString());
                    break;
                case NXObject.AttributeType.Real:
                    result.Add(a.Title, JsonDouble(a.RealValue));
                    break;
                case NXObject.AttributeType.Boolean:
                    result.Add(a.Title, a.BooleanValue ? "true" : "false");
                    break;
                // Skip Null and other types silently
            }
        }
        return result;
    }

    // ---------------------------------------------------------------------------
    // Transform math
    // ---------------------------------------------------------------------------

    static void ComponentTransform(Component component, out double[][] rot, out double[] origin)
    {
        Point3d originPt;
        Matrix3x3 orient;
        component.GetPosition(out originPt, out orient);
        rot = new double[][]
        {
            new double[] { orient.Xx, orient.Xy, orient.Xz },
            new double[] { orient.Yx, orient.Yy, orient.Yz },
            new double[] { orient.Zx, orient.Zy, orient.Zz },
        };
        origin = new double[] { originPt.X, originPt.Y, originPt.Z };
    }

    static double[] TransformPoint(double[] point, double[][] rot, double[] origin)
    {
        double x = point[0], y = point[1], z = point[2];
        return new double[]
        {
            rot[0][0] * x + rot[0][1] * y + rot[0][2] * z + origin[0],
            rot[1][0] * x + rot[1][1] * y + rot[1][2] * z + origin[1],
            rot[2][0] * x + rot[2][1] * y + rot[2][2] * z + origin[2],
        };
    }

    static void ComposeTransforms(
        double[][] parentRot, double[] parentOrigin,
        double[][] childRot, double[] childOrigin,
        out double[][] resultRot, out double[] resultOrigin)
    {
        resultRot = new double[3][];
        for (int i = 0; i < 3; i++)
        {
            resultRot[i] = new double[3];
            for (int j = 0; j < 3; j++)
            {
                double sum = 0.0;
                for (int k = 0; k < 3; k++)
                    sum += parentRot[i][k] * childRot[k][j];
                resultRot[i][j] = sum;
            }
        }
        resultOrigin = TransformPoint(childOrigin, parentRot, parentOrigin);
    }

    // ---------------------------------------------------------------------------
    // Body helpers
    // ---------------------------------------------------------------------------

    static double[] BodyCentroid(MeasureManager measureManager, Unit[] massUnits, Body body)
    {
        var measure = measureManager.NewMassProperties(massUnits, 0.99, new Body[] { body });
        var c = measure.Centroid;
        return new double[] { c.X, c.Y, c.Z };
    }

    /// <summary>
    /// Get solid bodies in the component's active reference set.
    /// Returns null = use all bodies; empty dict = no bodies (refset "Empty").
    /// Uses Dictionary<Body,bool> as a set substitute (HashSet unavailable here).
    /// Wraps GetAllReferenceSets() in try/catch: it can throw on some assemblies.
    /// </summary>
    static Dictionary<Body, bool> GetReferenceSetBodies(Component component)
    {
        try
        {
            string refSetName = component.ReferenceSet;
            if (string.IsNullOrEmpty(refSetName) || refSetName == "Entire Part")
                return null;
            if (refSetName == "Empty")
                return new Dictionary<Body, bool>();

            Part owningPart = (Part)component.Prototype.OwningPart;
            ReferenceSet[] allRefSets;
            try
            {
                allRefSets = owningPart.GetAllReferenceSets();
            }
            catch (Exception)
            {
                return null;
            }

            foreach (var refSet in allRefSets)
            {
                if (refSet.Name == refSetName)
                {
                    var members = refSet.AskAllDirectMembers();
                    var result = new Dictionary<Body, bool>();
                    foreach (var m in members)
                    {
                        Body b = m as Body;
                        if (b != null && b.IsSolidBody)
                            result[b] = true;
                    }
                    return result;
                }
            }
            return null;
        }
        catch (Exception)
        {
            return null;
        }
    }

    // ---------------------------------------------------------------------------
    // Name / path helpers
    // ---------------------------------------------------------------------------

    static string ExtractInstance(string name)
    {
        if (string.IsNullOrEmpty(name))
            return "1";
        int pos = name.LastIndexOf('_');
        if (pos >= 0 && pos + 1 < name.Length)
        {
            string suffix = name.Substring(pos + 1);
            bool allDigits = suffix.Length > 0;
            foreach (char c in suffix)
            {
                if (!char.IsDigit(c)) { allDigits = false; break; }
            }
            if (allDigits)
                return suffix;
        }
        return "1";
    }

    static string PartFileStem(string partFile)
    {
        if (string.IsNullOrEmpty(partFile))
            return "";
        string normalized = partFile.Replace('\\', '/');
        int slashPos = normalized.LastIndexOf('/');
        string basename = slashPos >= 0 ? normalized.Substring(slashPos + 1) : normalized;
        int dotPos = basename.LastIndexOf('.');
        if (dotPos > 0)
            return basename.Substring(0, dotPos);
        return basename;
    }

    static string BodyDisplayName(Body body)
    {
        try
        {
            string name = body.Name;
            if (!string.IsNullOrEmpty(name))
                return name;
        }
        catch (Exception) { }
        return body.JournalIdentifier;
    }

    static string TcItemId(string name)
    {
        if (string.IsNullOrEmpty(name))
            return "";
        int caret = name.IndexOf('^');
        return caret >= 0 ? name.Substring(0, caret) : name;
    }

    // ---------------------------------------------------------------------------
    // Assembly traversal
    // ---------------------------------------------------------------------------

    static void CollectBodies(
        Component component,
        Dictionary<string, JsonObj> components,
        List<JsonObj> bodies,
        MeasureManager measureManager,
        Unit[] massUnits,
        double unitToMeters,
        string parentPath,
        double[][] parentRot,
        double[] parentOrigin)
    {
        var attrs = CollectAttributes(component.GetUserAttributes());

        string pfile = "";
        try { pfile = component.Prototype.OwningPart.FullPath ?? ""; }
        catch (Exception) { }

        double[][] localRot;
        double[] localOrigin;
        ComponentTransform(component, out localRot, out localOrigin);

        double[][] worldRot;
        double[] worldOrigin;
        ComposeTransforms(parentRot, parentOrigin, localRot, localOrigin, out worldRot, out worldOrigin);

        string name = component.Name ?? "";

        string rawDbPartName = attrs.GetRaw("DB_PART_NAME");
        string dbPartName = rawDbPartName != null
            ? rawDbPartName.Trim('"')
            : name;

        string rawDbPartNo = attrs.GetRaw("DB_PART_NO");
        string dbPartNo = rawDbPartNo != null
            ? rawDbPartNo.Trim('"')
            : name;

        string instance = ExtractInstance(name);
        string stem = PartFileStem(pfile);

        string nodePath = string.IsNullOrEmpty(parentPath) ? dbPartNo : parentPath + "/" + dbPartNo;

        string refSetName = "";
        try { refSetName = component.ReferenceSet ?? ""; }
        catch (Exception) { }

        Dictionary<Body, bool> refSetBodies = GetReferenceSetBodies(component);

        string componentKey = dbPartNo + "_" + instance;

        if (components.ContainsKey(componentKey))
        {
            // Validate key uniqueness: same key must come from same component
            string existingName = components[componentKey].GetRaw("component_name");
            if (existingName != null) existingName = existingName.Trim('"');
            if (existingName != name)
            {
                throw new InvalidOperationException(
                    "Duplicate component key " + componentKey +
                    " with different names: " + existingName + " vs " + name);
            }
        }
        else
        {
            var compRecord = new JsonObj();
            compRecord.Add("component_name", JsonString(name));
            compRecord.Add("part_file_stem", JsonString(stem));
            compRecord.Add("path", JsonString(nodePath));
            compRecord.Add("db_part_name", JsonString(dbPartName));
            compRecord.Add("db_part_no", JsonString(dbPartNo));
            compRecord.Add("instance", JsonString(instance));
            compRecord.Add("reference_set", JsonString(refSetName));
            compRecord.Add("attributes", attrs.ToJson(4));
            components[componentKey] = compRecord;
        }

        try
        {
            Part owningPart = (Part)component.Prototype.OwningPart;
            int bodyIndex = 0;
            foreach (Body body in owningPart.Bodies)
            {
                if (!body.IsSolidBody)
                    continue;
                if (refSetBodies != null && !refSetBodies.ContainsKey(body))
                    continue;
                bodyIndex++;
                double[] localCen = BodyCentroid(measureManager, massUnits, body);
                double[] worldCen = TransformPoint(localCen, worldRot, worldOrigin);

                var bodyRecord = new JsonObj();
                bodyRecord.Add("centroid", CentroidJson(
                    worldCen[0] * unitToMeters,
                    worldCen[1] * unitToMeters,
                    worldCen[2] * unitToMeters));
                bodyRecord.Add("component", JsonString(componentKey));
                bodyRecord.Add("body_name", JsonString(BodyDisplayName(body)));
                bodyRecord.Add("body_index", bodyIndex.ToString());
                bodies.Add(bodyRecord);
            }
        }
        catch (Exception) { }

        foreach (Component child in component.GetChildren())
        {
            CollectBodies(
                child, components, bodies,
                measureManager, massUnits, unitToMeters,
                nodePath, worldRot, worldOrigin);
        }
    }

    static void CollectBodiesFlat(
        Part displayPart,
        Dictionary<string, JsonObj> components,
        List<JsonObj> bodies,
        MeasureManager measureManager,
        Unit[] massUnits,
        double unitToMeters)
    {
        var attrs = CollectAttributes(displayPart.GetUserAttributes());

        string stem = TcItemId(displayPart.Name ?? "");
        if (string.IsNullOrEmpty(stem))
            stem = PartFileStem(displayPart.FullPath ?? "");

        string rawDbPartName = attrs.GetRaw("DB_PART_NAME");
        string dbPartName = rawDbPartName != null ? rawDbPartName.Trim('"') : stem;

        string rawDbPartNo = attrs.GetRaw("DB_PART_NO");
        string dbPartNo = rawDbPartNo != null ? rawDbPartNo.Trim('"') : stem;

        string instance = "1";
        string componentKey = dbPartNo + "_" + instance;

        var compRecord = new JsonObj();
        compRecord.Add("component_name", JsonString(stem));
        compRecord.Add("part_file_stem", JsonString(stem));
        compRecord.Add("path", JsonString(dbPartNo));
        compRecord.Add("db_part_name", JsonString(dbPartName));
        compRecord.Add("db_part_no", JsonString(dbPartNo));
        compRecord.Add("instance", JsonString(instance));
        compRecord.Add("reference_set", JsonString(""));
        compRecord.Add("attributes", attrs.ToJson(4));
        components[componentKey] = compRecord;

        int bodyIndex = 0;
        foreach (Body body in displayPart.Bodies)
        {
            if (!body.IsSolidBody)
                continue;
            bodyIndex++;
            double[] localCen = BodyCentroid(measureManager, massUnits, body);

            var bodyRecord = new JsonObj();
            bodyRecord.Add("centroid", CentroidJson(
                localCen[0] * unitToMeters,
                localCen[1] * unitToMeters,
                localCen[2] * unitToMeters));
            bodyRecord.Add("component", JsonString(componentKey));
            bodyRecord.Add("body_name", JsonString(BodyDisplayName(body)));
            bodyRecord.Add("body_index", bodyIndex.ToString());
            bodies.Add(bodyRecord);
        }
    }

    // ---------------------------------------------------------------------------
    // Parasolid export + loader
    // ---------------------------------------------------------------------------

    static void ExportParasolid(Session session, Part workPart, string outPath)
    {
        var exporter = session.DexManager.CreateParasolidExporter();
        try
        {
            exporter.ObjectTypes.Solids = true;
            exporter.ObjectTypes.Surfaces = false;
            exporter.ObjectTypes.Curves = false;
            exporter.ExportSelectionBlock.SelectionScope = ObjectSelector.Scope.EntireAssembly;
            exporter.FlattenAssembly = false;
            session.LogFile.WriteLine("basalt export_parasolid: InputFile=" + (workPart.FullPath ?? ""));
            exporter.InputFile = workPart.FullPath ?? "";
            session.LogFile.WriteLine("basalt export_parasolid: OutputFile=" + outPath);
            exporter.OutputFile = outPath;
            exporter.ParasolidVersion = ParasolidExporter.ParasolidVersionOption.Current;
            session.LogFile.WriteLine("basalt export_parasolid: calling Commit ...");
            exporter.Commit();
            session.LogFile.WriteLine("basalt export_parasolid: Commit OK");
        }
        finally
        {
            exporter.Destroy();
        }
    }

    static void EnsureFullyLoaded(Session session, Part displayPart)
    {
        Component root = displayPart.ComponentAssembly.RootComponent;
        if (root == null)
            return;
        OpenChildren(session, root);
    }

    static void OpenChildren(Session session, Component component)
    {
        // Best-effort: open any lightweight components so bodies are accessible.
        // session.Parts.Open signature is version-specific; just touching OwningPart
        // is enough to trigger load in most cases. Any failure is silently ignored.
        foreach (Component child in component.GetChildren())
        {
            try
            {
                BasePart part = child.Prototype.OwningPart;
                // Accessing Bodies on a not-yet-loaded part forces NX to load it.
                // We do not call session.Parts.Open() — the overload taking a plain
                // string path does not exist on this NX version's PartCollection.
                var _ = part.FullPath;
            }
            catch (Exception) { }
            OpenChildren(session, child);
        }
    }

    // ---------------------------------------------------------------------------
    // Entry point
    // ---------------------------------------------------------------------------

    public static int Main(string[] args)
    {
        string exportDir = System.Environment.GetEnvironmentVariable("BASALT_NX_EXPORT_DIR");
        if (string.IsNullOrEmpty(exportDir))
        {
            throw new InvalidOperationException(
                "BASALT_NX_EXPORT_DIR environment variable is not set. Set it to the " +
                "directory where the journal should write .x_t and _attrs.json files " +
                "(typically your basalt checkout's tests/data folder).");
        }

        var session = Session.GetSession();
        session.LogFile.WriteLine("basalt === journal start ===");

        Part workPart = session.Parts.Work;
        Part displayPart = session.Parts.Display;

        session.LogFile.WriteLine("basalt work_part.Name=" + (workPart.Name ?? "<empty>"));
        session.LogFile.WriteLine("basalt work_part.FullPath=" + (workPart.FullPath ?? "<empty>"));
        session.LogFile.WriteLine("basalt display_part.Name=" + (displayPart.Name ?? "<empty>"));
        session.LogFile.WriteLine("basalt display_part.FullPath=" + (displayPart.FullPath ?? "<empty>"));

        session.LogFile.WriteLine("basalt calling ensure_fully_loaded ...");
        EnsureFullyLoaded(session, displayPart);
        session.LogFile.WriteLine("basalt ensure_fully_loaded OK");

        session.LogFile.WriteLine("basalt reading RootComponent ...");
        Component root = displayPart.ComponentAssembly.RootComponent;
        if (root == null)
            session.LogFile.WriteLine("basalt RootComponent is None (Type 2 flat part)");
        else
            session.LogFile.WriteLine("basalt RootComponent present (Type 1 assembly)");

        session.LogFile.WriteLine("basalt resolving EXPORT_DIR=" + exportDir);
        if (!Directory.Exists(exportDir))
        {
            session.LogFile.WriteLine("basalt EXPORT_DIR missing; creating ...");
            try
            {
                Directory.CreateDirectory(exportDir);
                session.LogFile.WriteLine("basalt makedirs OK");
            }
            catch (Exception e)
            {
                session.LogFile.WriteLine("basalt makedirs failed: " + e.Message);
            }
        }

        string partName = TcItemId(displayPart.Name ?? "");
        if (string.IsNullOrEmpty(partName))
            partName = "exported_part";
        string basePath = Path.Combine(exportDir, partName);
        session.LogFile.WriteLine("basalt base path: " + basePath);

        session.LogFile.WriteLine("basalt setting up measurement units ...");
        var uc = displayPart.UnitCollection;
        var massUnits = new Unit[]
        {
            uc.GetBase("Area"),
            uc.GetBase("Volume"),
            uc.GetBase("Mass"),
            uc.GetBase("Length"),
        };
        var measureManager = displayPart.MeasureManager;
        session.LogFile.WriteLine("basalt measurement setup OK");

        Unit lengthUnit = uc.GetBase("Length");
        double unitToMeters;
        string lengthTypeName = lengthUnit.TypeName ?? "";
        if (lengthTypeName.IndexOf("Milli", StringComparison.OrdinalIgnoreCase) >= 0)
            unitToMeters = 0.001;
        else if (lengthTypeName.IndexOf("Inch", StringComparison.OrdinalIgnoreCase) >= 0)
            unitToMeters = 0.0254;
        else
            unitToMeters = 1.0;
        session.LogFile.WriteLine(
            "basalt length unit: " + lengthTypeName +
            ", scale to meters: " + unitToMeters.ToString());

        string xtPath = basePath + ".x_t";
        session.LogFile.WriteLine("basalt calling export_parasolid -> " + xtPath);
        ExportParasolid(session, displayPart, xtPath);
        session.LogFile.WriteLine("basalt export_parasolid OK");

        session.LogFile.WriteLine("basalt collecting attributes ...");
        var components = new Dictionary<string, JsonObj>();
        var bodies = new List<JsonObj>();

        if (root == null)
        {
            session.LogFile.WriteLine("basalt using collect_bodies_flat (Type 2)");
            CollectBodiesFlat(displayPart, components, bodies, measureManager, massUnits, unitToMeters);
        }
        else
        {
            session.LogFile.WriteLine("basalt using collect_bodies (Type 1)");
            CollectBodies(
                root, components, bodies,
                measureManager, massUnits, unitToMeters,
                "", IdentityRot, IdentityOrigin);
        }
        session.LogFile.WriteLine(
            "basalt collected: components=" + components.Count +
            ", bodies=" + bodies.Count);

        // Build top-level JSON output preserving insertion order.
        // Components object: iterate Dictionary in insertion order.
        var compItems = new List<string>();
        foreach (var key in new List<string>(components.Keys))
        {
            compItems.Add(JsonString(key) + ": " + components[key].ToJson(4));
        }
        string componentsJson = JsonArray(compItems, 2)
            .Replace("[", "{").Replace("]", "}");
        // JsonArray uses [] brackets; replace outer ones with {} for an object.
        // But the inner items already have the key: val form, so replace only outermost.
        // Actually, rebuild as proper object:
        var compObjSb = new StringBuilder("{\n");
        var compKeys = new List<string>(components.Keys);
        for (int i = 0; i < compKeys.Count; i++)
        {
            compObjSb.Append("    ");
            compObjSb.Append(JsonString(compKeys[i]));
            compObjSb.Append(": ");
            compObjSb.Append(components[compKeys[i]].ToJson(4));
            if (i < compKeys.Count - 1) compObjSb.Append(",");
            compObjSb.Append("\n");
        }
        compObjSb.Append("  }");

        // Bodies array
        var bodyItems = new List<string>();
        foreach (var b in bodies)
            bodyItems.Add(b.ToJson(4));
        string bodiesJson = JsonArray(bodyItems, 2);

        var outSb = new StringBuilder();
        outSb.Append("{\n");
        outSb.Append("  \"schema_version\": 6,\n");
        outSb.Append("  \"source_part\": " + JsonString(displayPart.FullPath ?? "") + ",\n");
        outSb.Append("  \"exported_at\": " + JsonString(DateTime.Now.ToString("yyyy-MM-ddTHH:mm:ss")) + ",\n");
        outSb.Append("  \"components\": " + compObjSb.ToString() + ",\n");
        outSb.Append("  \"bodies\": " + bodiesJson + "\n");
        outSb.Append("}");

        string attrsPath = basePath + "_attrs.json";
        session.LogFile.WriteLine("basalt writing JSON sidecar -> " + attrsPath);
        // UTF-8 without BOM: Python's json.dump emits no BOM, and basalt's
        // sidecar parser (nlohmann::json::parse) errors on a leading BOM.
        // .NET's Encoding.UTF8 emits a BOM; new UTF8Encoding(false) suppresses it.
        File.WriteAllText(attrsPath, outSb.ToString(), new UTF8Encoding(false));
        session.LogFile.WriteLine("basalt JSON sidecar written OK");

        session.LogFile.WriteLine("basalt === journal end ===");
        return 0;
    }
}
