#include "MeshSim.h"
#include "ModelTypes.h"
#include "SimInfo.h"
#include "SimInfoCodes.h"
#include "SimMeshingInfoCodes.h"
#include "SimMessages.h"
#include "SimParasolidKrnl.h"
#include <iostream>
#include <string.h>
using namespace std;
void messageHandler(int type, const char *msg);
void progressHandler(const char *what, int level, int startVal, int endVal,
                     int currentVal, void *);
int main(int argc, char *argv[]) {
  pParasolidNativeModel nModel;
  pGModel Amodel;
  pGModel model;
  pMesh mesh;

  // You will want to place a try/catch around all SimModSuite calls,
  // as errors are thrown.
  try {
    Sim_logOn("relations.log");
    MS_init(); // Call before Sim_readLicenseFile
    // NOTE: Sim_readLicenseFile() is for internal testing only.  To use,
    // pass in the location of a file containing your keys.  For a release
    // product, use Sim_registerKey()
    Sim_readLicenseFile("TheaEnergyEval2025");
    SimParasolid_start(1);
    Sim_setMessageHandler(messageHandler);
    pProgress progress = Progress_new();
    Progress_setCallback(progress, progressHandler);

    int numAtts;
    int i;
    char **values;
    // The string "SimTestName" was placed on the parts, faces, and edges of the
    // model using GIP_setNativeStringAttribute and GEN_setNativeStringAttribute
    char *attName = "SimTestName";

    // Load the model as an assembly model
    nModel = ParasolidNM_createFromFile(
        "data/model_G1600-with-3-blanket-layers_flattened.x_t", 0);
    Amodel = GAM_createFromNativeModel(nModel, progress);
    cout << "Number of assemblies in Assembly Model: "
         << GAM_numAssemblies(Amodel) << endl;
    cout << "Number of parts in Assembly Model: " << GAM_numParts(Amodel)
         << endl;

    // create the assembly model - non-manifold model connector
    pMConnector connector = MC_new();
    // Create the non-manifold model
    model = GM_createFromAssemblyModel(Amodel, connector, progress);
    pModelItem modelDomain = GM_domain(model);
    pANMConnection ModelConnection = ANMConnection_new(connector);

    // If you are not interested in creating a mesh, you could still loop over
    // the non-manifold model entities and trace them to the assembly model
    // entities from which they originate using MC_relatedModelItems.  Or you
    // could trace them to the assembly parts using ANMConnection_relatedParts.
    // These functions are demonstrated below as part of the process of tracing
    // mesh faces back to the assembly model.

    // now write out mesh regions classified on model regions
    auto modelRegions = GM_regionIter(model);
    while (auto modelRegion = GRIter_next(
               modelRegions)) { // loops over all model regions in model
      pPList assemRegions = MC_relatedModelItems(connector, modelRegion);
      pPList assemParts =
          ANMConnection_relatedParts(ModelConnection, modelRegion);

      cout << "Region " << GEN_tag(modelRegion)
           << " regions: " << PList_size(assemRegions) << " ->"
           << " parts: " << PList_size(assemParts) << " ->";

      void *iter = 0; // must initialize to 0
      while (auto part = (pGEntity)PList_next(assemParts, &iter)) {
        // Sanity check - the entity should be a part
        if (GEN_type(part) == 1007) {
          // Cast it to a part as that is what we need
          pGIPart assemPart = (pGIPart)part;
          // Check if the part has the attribute "SimTestName"
          numAtts = GIP_numNativeStringAttribute(assemPart, "Name");
          values = new char *[numAtts]; // array to hold the values
          GIP_nativeStringAttribute(assemPart, attName, values);
          for (i = 0; i < numAtts; i++) {
            cout << " " << values[i];
            Sim_deleteString(values[i]);
          }
          delete[] values;
          char *name = GIP_nativeName(assemPart);
          if (name != 0) {
            cout << " " << name;
            Sim_deleteString(name);
          } else
            cout << "No name for this part" << endl;
        }
      }
      PList_delete(assemParts); // Clean up the list
      cout << "\n" << endl;
    }
    GRIter_delete(modelRegions);

    // Create the mesh
    pPList modelErrors = PList_new();
    if (GM_isValid(model, 0, modelErrors)) {
      mesh = M_new(0, model);
      pACase meshCase = MS_newMeshCase(model); // create a meshing case
      // MS_setMeshSize(meshCase, modelDomain, 2, 0.01, 0);
      MS_setMeshCurv(meshCase, modelDomain, 1, 0.1, 3);
      // MS_setProximityRefinement(meshCase, modelDomain, 1.0);
      // create the meshers and run
      pSurfaceMesher surfaceMesher = SurfaceMesher_new(meshCase, mesh);
      SurfaceMesher_execute(surfaceMesher, progress);
      pNativeModel nonManNative = GM_nativeModel(model);
      NM_write(nonManNative, "native.x_t", progress);
      GM_write(model, "nonman.smd", 0,
               progress); // write out the model before the mesh!
      M_write(mesh, "mesh.sms", 0, progress);

      // clean up meshers and meshing case
      SurfaceMesher_delete(surfaceMesher);
      MS_deleteMeshCase(meshCase);

      M_release(mesh);
      NM_release(nonManNative);
    } else {
      cerr << "Input model not valid" << endl;
      cerr << "Number of errors returned: " << PList_size(modelErrors) << endl;
    }
    PList_delete(modelErrors);
    ANMConnection_delete(ModelConnection);
    MC_release(connector);
    GM_release(Amodel);
    GM_release(model);
    NM_release(nModel);
    Progress_delete(progress);
    SimParasolid_stop(1);
    Sim_unregisterAllKeys();
    MS_exit();
    Sim_logOff();
  } catch (pSimInfo err) {
    cerr << "SimModSuite error caught:" << endl;
    cerr << "  Error code: " << SimInfo_code(err) << endl;
    cerr << "  Error string: " << SimInfo_toString(err) << endl;
    SimInfo_delete(err);
    return 1;
  } catch (...) {
    cerr << "Unhandled exception caught" << endl;
    return 1;
  }
  return 0;
}
void messageHandler(int type, const char *msg) {
  switch (type) {
  case Sim_InfoMsg:
    cout << "Info: " << msg << endl;
    break;
  case Sim_DebugMsg:
    cout << "Debug: " << msg << endl;
    break;
  case Sim_WarningMsg:
    cout << "Warning: " << msg << endl;
    break;
  case Sim_ErrorMsg:
    cout << "Error: " << msg << endl;
    break;
  }
  return;
}
void progressHandler(const char *what, int level, int startVal, int endVal,
                     int currentVal, void *) {
  cout << "Progress: " << what << ", level: " << level;
  cout << ", startVal: " << startVal << ", endVal: " << endVal;
  cout << ", currentVal: " << currentVal << endl;
  return;
}