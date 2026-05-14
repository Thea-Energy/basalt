// basalt: NX journal that restructures the upstream Proxima open_stellarator_models
// scaled W7-X model into the layout required by basalt's test-fixture suite.
//
// Driven by nx-bridge (JournalManager.PlayDotNetJournal). One-shot use during
// B.1 implementation; re-runnable for fixture refreshes if the upstream STEP
// is updated.
//
// Operations (per spec section 2.1, option B — nested-assembly):
//   1. Create a new parent sub-assembly "coils" containing the 50 existing
//      COIL_*_STEP components, with their world transforms preserved.
//      Tag each child with NX_MATERIAL="coils".
//   2. Create a new parent sub-assembly "blanket_module" containing the
//      existing firstwall / blanket / vessel components, renamed (lowercase,
//      no _STEP). Each retains its world transform.
//   3. Rename top-level singletons PLASMA_STEP -> plasma, GAP_STEP -> gap.
//
// This avoids cross-part body merging (Parasolid round-trip + WAVE link both
// hit NX-API depth in earlier iterations). The fixture exercises nested-
// assembly traversal in PHNX/basalt; the multi-body GAM pattern is left to a
// future fixture.
//
// Pre-condition: the upstream scaled_w7x_stellarator part is the display
// part and fully loaded in NX.
// Post-condition: the NX session holds the restructured assembly, ready for
// export_attrs.cs to emit .x_t + _attrs.json.
//
// NX 2406 .NET Framework constraints (same as export_attrs.cs):
//   - Out parameters instead of value tuples
//   - No HashSet<T>; Dictionary<T,bool> as a set
//   - FileNew with UseBlankTemplate=true fails with error 3815003 here;
//     using model-plain-1-mm-template.prt instead.

using System;
using System.Collections.Generic;
using System.IO;
using NXOpen;
using NXOpen.Assemblies;

public class RestructureW7X
{
    // -------------------------------------------------------------------
    // Name helpers
    // -------------------------------------------------------------------

    // Normalize a component name to a canonical lowercase form for matching.
    // "COIL_0_STEP" -> "coil_0"; "PLASMA_STEP" -> "plasma"; "coil_1" -> "coil_1".
    static string NormalizeName(string name)
    {
        if (string.IsNullOrEmpty(name)) return "";
        string lower = name.ToLowerInvariant();
        if (lower.EndsWith("_step"))
            lower = lower.Substring(0, lower.Length - 5);
        return lower;
    }

    static bool IsCoilName(string normalizedName)
    {
        if (!normalizedName.StartsWith("coil_")) return false;
        string suffix = normalizedName.Substring(5);
        if (suffix.Length == 0) return false;
        foreach (char c in suffix) if (!char.IsDigit(c)) return false;
        return true;
    }

    // -------------------------------------------------------------------
    // Per-component snapshot: what we need to recreate a component as a
    // child of a different parent assembly with the same world placement.
    // -------------------------------------------------------------------

    struct CompSnapshot
    {
        public string PartPath;     // The .prt the component instantiates
        public Point3d Origin;
        public Matrix3x3 Orient;
        public string NewName;      // Name to assign in the new parent
        public string NxMaterial;   // Optional NX_MATERIAL value (empty = don't set)
    }

    static CompSnapshot Snapshot(Component c, string newName, string nxMaterial)
    {
        Point3d origin;
        Matrix3x3 orient;
        c.GetPosition(out origin, out orient);
        var owningPart = c.Prototype.OwningPart;
        return new CompSnapshot
        {
            PartPath = owningPart.FullPath,
            Origin = origin,
            Orient = orient,
            NewName = newName,
            NxMaterial = nxMaterial,
        };
    }

    // -------------------------------------------------------------------
    // Output directory: prefer BASALT_NX_EXPORT_DIR; fall back to display
    // part's directory. Both common cases (.prt in user's Downloads, .step
    // under the worktree) work either way.
    // -------------------------------------------------------------------

    static string ResolveOutDir(Part displayPart)
    {
        string exportDir = Environment.GetEnvironmentVariable("BASALT_NX_EXPORT_DIR");
        if (!string.IsNullOrEmpty(exportDir)) return exportDir;
        var dp = displayPart.FullPath ?? "";
        var parent = Path.GetDirectoryName(dp);
        if (!string.IsNullOrEmpty(parent)) return parent;
        throw new InvalidOperationException(
            "restructure_w7x: cannot determine output dir. " +
            "Set BASALT_NX_EXPORT_DIR or ensure display part has a valid FullPath.");
    }

    // -------------------------------------------------------------------
    // Create a new empty assembly part using the standard mm modeling
    // template. UseBlankTemplate=true fails with NX error 3815003 in this
    // install; the template-based path works.
    // -------------------------------------------------------------------

    static Part CreateEmptyAssemblyPart(Session session, string newPath)
    {
        // Close any existing Part the session has cached for this path. Without
        // this, re-runs after a partial failure hit NX error 3815003 because
        // FileNew sees the cached part and refuses to create. Iterate
        // session.Parts looking for a path match; close immediately.
        var toClose = new List<Part>();
        foreach (Part p in session.Parts)
        {
            string pp = p.FullPath ?? "";
            if (string.Equals(pp, newPath, StringComparison.OrdinalIgnoreCase))
                toClose.Add(p);
        }
        foreach (Part p in toClose)
        {
            session.LogFile.WriteLine(
                "restructure_w7x: closing cached part " + p.FullPath);
            p.Close(BasePart.CloseWholeTree.True, BasePart.CloseModified.CloseModified, null);
        }
        if (File.Exists(newPath))
        {
            try { File.Delete(newPath); }
            catch (Exception ex)
            {
                session.LogFile.WriteLine(
                    "restructure_w7x: File.Delete(" + newPath + ") failed: " + ex.Message +
                    " — proceeding; FileNew may still succeed.");
            }
        }
        FileNew fileNew = session.Parts.FileNew();
        fileNew.TemplateFileName = "model-plain-1-mm-template.prt";
        fileNew.UseBlankTemplate = false;
        fileNew.NewFileName = newPath;
        fileNew.MasterFileName = "";
        fileNew.Application = FileNewApplication.Modeling;
        fileNew.Units = Part.Units.Millimeters;
        fileNew.MakeDisplayedPart = false;
        NXObject newObj = fileNew.Commit();
        fileNew.Destroy();
        var part = newObj as Part;
        if (part == null)
            throw new InvalidOperationException(
                "restructure_w7x: FileNew did not return a Part. Got: " +
                (newObj != null ? newObj.GetType().Name : "<null>"));
        return part;
    }

    // -------------------------------------------------------------------
    // Move a set of components from one parent (currently displayPart's
    // root) into a freshly-created parent sub-assembly. World transforms
    // preserved. Each new child gets an NX_MATERIAL attribute if provided.
    // -------------------------------------------------------------------

    static void RegroupUnderNewParent(
        Session session,
        Part displayPart,
        Component[] oldComps,
        IList<CompSnapshot> snapshots,
        string parentPartPath,
        string parentInstanceName)
    {
        // Create the new parent .prt
        Part parentPart = CreateEmptyAssemblyPart(session, parentPartPath);
        session.LogFile.WriteLine("restructure_w7x: created " + parentPartPath);

        // Add each snapshotted component as a child of the new parent.
        // No SetWork: parts created via FileNew with MakeDisplayedPart=false
        // throw on SetWork. AddComponent operates on the explicit
        // parentPart.ComponentAssembly target regardless of work part.
        session.LogFile.WriteLine("restructure_w7x: iterating " + snapshots.Count + " snapshots");
        try
        {
            int idx = 0;
            foreach (var s in snapshots)
            {
                idx++;
                session.LogFile.WriteLine(
                    "restructure_w7x:   add[" + idx + "] name=" + s.NewName +
                    " part=" + s.PartPath);
                NXOpen.PartLoadStatus loadStatus;
                Component newChild;
                try
                {
                    newChild = parentPart.ComponentAssembly.AddComponent(
                        s.PartPath, "MODEL", s.NewName,
                        s.Origin, s.Orient, -1, out loadStatus);
                }
                catch (Exception ex)
                {
                    session.LogFile.WriteLine(
                        "restructure_w7x:   add[" + idx + "] EXCEPTION " +
                        ex.GetType().Name + ": " + ex.Message);
                    throw;
                }
                if (newChild == null)
                    throw new InvalidOperationException(
                        "restructure_w7x: AddComponent returned null for " + s.NewName);
                // NX_MATERIAL skipped intentionally: PHNX derives material slugs
                // from comp_key.bN at sidecar-load time and writes NX_MATERIAL
                // itself; no need to set it from the journal here.
            }
            session.LogFile.WriteLine(
                "restructure_w7x: all " + snapshots.Count + " AddComponent calls succeeded");
        }
        finally
        {
            // No SetWork undo needed; we never changed the work part.
        }

        // Save parent .prt so subsequent journals see it on disk
        parentPart.Save(BasePart.SaveComponents.True, BasePart.CloseAfterSave.False);
        session.LogFile.WriteLine(
            "restructure_w7x: saved " + parentPartPath + " with " + snapshots.Count + " children");

        // Remove the original components from displayPart
        foreach (Component c in oldComps)
            displayPart.ComponentAssembly.RemoveComponent(c);
        session.LogFile.WriteLine(
            "restructure_w7x: removed " + oldComps.Length + " original components from root");

        // Add the new parent .prt as a single component at identity transform
        Matrix3x3 identity;
        identity.Xx = 1.0; identity.Xy = 0.0; identity.Xz = 0.0;
        identity.Yx = 0.0; identity.Yy = 1.0; identity.Yz = 0.0;
        identity.Zx = 0.0; identity.Zy = 0.0; identity.Zz = 1.0;
        Point3d origin0;
        origin0.X = 0.0; origin0.Y = 0.0; origin0.Z = 0.0;

        NXOpen.PartLoadStatus addStatus;
        Component newParentInstance = displayPart.ComponentAssembly.AddComponent(
            parentPartPath, "MODEL", parentInstanceName,
            origin0, identity, -1, out addStatus);
        if (newParentInstance == null)
            throw new InvalidOperationException(
                "restructure_w7x: AddComponent for parent " + parentInstanceName + " returned null");
        session.LogFile.WriteLine(
            "restructure_w7x: added " + parentInstanceName + " parent component at root");
    }

    // -------------------------------------------------------------------
    // Rename a single component (and the underlying part if writeable) to
    // a new logical name. Used for plasma/gap singletons.
    // -------------------------------------------------------------------

    static void RenameComponent(Session session, Component c, string newName)
    {
        // Component-level name (what export_attrs reads via Component.Name)
        c.SetName(newName);
        session.LogFile.WriteLine(
            "restructure_w7x: renamed component to '" + newName + "'");
    }

    // -------------------------------------------------------------------
    // Main
    // -------------------------------------------------------------------

    public static int Main(string[] args)
    {
        var session = Session.GetSession();
        session.LogFile.WriteLine("restructure_w7x: === start ===");

        var displayPart = session.Parts.Display;
        if (displayPart == null)
            throw new InvalidOperationException("restructure_w7x: no display part");

        var root = displayPart.ComponentAssembly.RootComponent;
        if (root == null)
            throw new InvalidOperationException(
                "restructure_w7x: display part has no root component (Type 2 flat part). " +
                "Expected a Type 1 assembly.");

        string outDir = ResolveOutDir(displayPart);
        session.LogFile.WriteLine("restructure_w7x: outDir=" + outDir);

        // -------------------------------------------------------------
        // Phase 1: classify the existing direct children of root
        // -------------------------------------------------------------

        var coilComps = new List<Component>();
        var coilSnaps = new List<CompSnapshot>();
        Component fwComp = null, blComp = null, vsComp = null;
        Component plasmaComp = null, gapComp = null;

        foreach (Component child in root.GetChildren())
        {
            string norm = NormalizeName(child.Name);
            session.LogFile.WriteLine("  child: " + child.Name + " (norm=" + norm + ")");
            if (IsCoilName(norm))
            {
                coilComps.Add(child);
                coilSnaps.Add(Snapshot(child, norm, "coils"));
            }
            else if (norm == "firstwall") fwComp = child;
            else if (norm == "blanket")   blComp = child;
            else if (norm == "vessel")    vsComp = child;
            else if (norm == "plasma")    plasmaComp = child;
            else if (norm == "gap")       gapComp = child;
            // Anything else: leave in place; restructure ignores extras.
        }

        session.LogFile.WriteLine(
            "restructure_w7x: classified " + coilComps.Count + " coils, " +
            (fwComp != null ? 1 : 0) + " firstwall, " +
            (blComp != null ? 1 : 0) + " blanket, " +
            (vsComp != null ? 1 : 0) + " vessel, " +
            (plasmaComp != null ? 1 : 0) + " plasma, " +
            (gapComp != null ? 1 : 0) + " gap");

        if (coilComps.Count != 50)
            throw new InvalidOperationException(
                "restructure_w7x: expected 50 coils, found " + coilComps.Count);
        if (fwComp == null || blComp == null || vsComp == null)
            throw new InvalidOperationException(
                "restructure_w7x: missing one of firstwall/blanket/vessel components");
        if (plasmaComp == null || gapComp == null)
            throw new InvalidOperationException(
                "restructure_w7x: missing plasma or gap component");

        // -------------------------------------------------------------
        // Phase 2: build the coils sub-assembly
        // -------------------------------------------------------------

        string coilsPath = Path.Combine(outDir, "coils.prt");
        RegroupUnderNewParent(session, displayPart,
            coilComps.ToArray(), coilSnaps, coilsPath, "coils");

        // -------------------------------------------------------------
        // Phase 3: build the blanket_module sub-assembly
        // -------------------------------------------------------------

        var bmComps = new[] { fwComp, blComp, vsComp };
        var bmSnaps = new List<CompSnapshot>
        {
            Snapshot(fwComp, "firstwall", ""),
            Snapshot(blComp, "blanket",   ""),
            Snapshot(vsComp, "vessel",    ""),
        };
        string blanketModulePath = Path.Combine(outDir, "blanket_module.prt");
        RegroupUnderNewParent(session, displayPart,
            bmComps, bmSnaps, blanketModulePath, "blanket_module");

        // -------------------------------------------------------------
        // Phase 4: rename plasma/gap singletons
        // -------------------------------------------------------------

        RenameComponent(session, plasmaComp, "plasma");
        RenameComponent(session, gapComp, "gap");

        // -------------------------------------------------------------
        // Save the restructured displayPart
        // -------------------------------------------------------------

        displayPart.Save(BasePart.SaveComponents.True, BasePart.CloseAfterSave.False);
        session.LogFile.WriteLine("restructure_w7x: saved displayPart");

        // -------------------------------------------------------------
        // Verification
        // -------------------------------------------------------------

        var rootKids = new List<string>();
        foreach (Component c in root.GetChildren()) rootKids.Add(c.Name);
        session.LogFile.WriteLine(
            "restructure_w7x: root children after restructure: [" +
            string.Join(", ", rootKids.ToArray()) + "]");

        session.LogFile.WriteLine("restructure_w7x: === end ===");
        return 0;
    }
}
