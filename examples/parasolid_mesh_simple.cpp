/*******************************************************************
 * This document is confidential information.
 * Copyright 1997-2025 Simmetrix Inc. All rights reserved. This
 * document is an unpublished work fully protected by the United
 * States copyright laws and is considered a trade secret belonging
 * to the copyright holder. Disclosure, use, or reproduction without
 * the written authorization of Simmetrix Inc. is prohibited.
 *******************************************************************/
#include "MeshSim.h"
#include "SimInfo.h"
#include "SimInfoCodes.h"
#include "SimMeshingInfoCodes.h"
#include "SimMessages.h"
#include "SimParasolidKrnl.h"
#include <iostream>
using namespace std;
void messageHandler(int type, const char *msg);
void progressHandler(const char *what, int level, int startVal, int endVal,
                     int currentVal, void *);
int main(int argc, char *argv[]) {

  pParasolidNativeModel nModel;
  pGModel model;
  pMesh mesh;

  // You will want to place a try/catch around all SimModSuite calls
  // as errors are thrown.
  try {
    Sim_logOn("exGenMesh.log");
    MS_init(); // Call before calling Sim_readLicenseFile
    // NOTE: Sim_readLicenseFile() is for internal testing only.  To use,
    // pass in the location of a file containing your keys.  For a release
    // product, use Sim_registerKey()
    Sim_readLicenseFile("TheaEnergyEval2025");
    SimParasolid_start(1);
    Sim_setMessageHandler(messageHandler);
    pProgress progress = Progress_new();
    Progress_setCallback(progress, progressHandler);
    nModel = ParasolidNM_createFromFile(
        // "doc/Simmetrix/GeomSim/input/block100.xmt_txt", 0);
        "A001-12MC-75PC C1.x_t", 0);
    model = GM_createFromNativeModel(nModel, progress);
    NM_release(nModel);

    // check that the input model is topologically valid before meshing
    pPList modelErrors = PList_new();
    if (!GM_isValid(model, 0, modelErrors)) {
      cerr << "Input model not valid" << endl;
      cerr << "Number of errors returned: " << PList_size(modelErrors) << endl;
      GM_release(model);
      return 1;
    }
    PList_delete(modelErrors);
    pModelItem modelDomain = GM_domain(model);
    mesh = M_new(0, model);
    pACase meshCase = MS_newMeshCase(model); // create a meshing case
    MS_setMeshSize(meshCase, modelDomain, 2, 0.1,
                   0); // set a relative mesh size on the model
    // create the meshers and run
    pSurfaceMesher surfaceMesher = SurfaceMesher_new(meshCase, mesh);
    SurfaceMesher_execute(surfaceMesher, progress);
    pVolumeMesher volumeMesher = VolumeMesher_new(meshCase, mesh);
    VolumeMesher_execute(volumeMesher, progress);
    GM_write(model, "nonman.smd", 0,
             progress); // write out the model before the mesh!
    M_write(mesh, "gen_vol.sms", 0,
            progress); // writes out the mesh to SimModSuite format
    // M_write(mesh, "gen_vol.stl", 0,
    //         progress); // writes out the mesh to SimModSuite format

    // clean up meshers, meshing case, and mesh
    SurfaceMesher_delete(surfaceMesher);
    VolumeMesher_delete(volumeMesher);
    MS_deleteMeshCase(meshCase);
    M_release(mesh);
    // cleanup
    GM_release(model);
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