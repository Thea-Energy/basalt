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
#include "SimOpenCascade.h"
#include <iostream>
using namespace std;
void messageHandler(int type, const char *msg);
void progressHandler(const char *what, int level, int startVal, int endVal,
                     int currentVal, void *);
int main(int argc, char *argv[]) {

  pOpenCascadeNativeModel nModel;
  pNativeModel nativeUnioned;
  pGModel assemModel1;
  pGModel assemModel2;
  pMesh mesh;
  pACase meshCase;
  // You will want to place a try/catch around all SimModSuite calls
  // as errors are thrown.
  try {
    Sim_logOn("exMeshAssemblies.log");
    MS_init(); // Call before calling Sim_readLicenseFile
    // NOTE: Sim_readLicenseFile() is for internal testing only.  To use,
    // pass in the location of a file containing your keys.  For a release
    // product, use Sim_registerKey()
    Sim_readLicenseFile("TheaEnergyEval2025");
    SimOpenCascade_start(1);
    Sim_setMessageHandler(messageHandler);
    pProgress progress = Progress_new();
    Progress_setCallback(progress, progressHandler);

    // nModel = OpenCascadeNM_createFromFile("A001-12MC-75PC C1.x_t", 0);
    nModel = OpenCascadeNM_createFromBrepFile("blanket.brep");
    assemModel1 = GAM_createFromNativeModel(nModel, progress);
    // check that the input model is topologically valid before meshing
    pPList modelErrors = PList_new();
    if (!GM_isValid(assemModel1, 0, modelErrors)) {
      cerr << "Input model not valid" << endl;
      cerr << "Number of errors returned: " << PList_size(modelErrors) << endl;
      GM_release(assemModel1);
      NM_release(nModel);
      return 1;
    }
    PList_delete(modelErrors);
    // For the first assembly model, we will mesh it as-is.  This will cause
    // non-conforming meshes to be created on the faces where the parts of the
    // assembly touch.  This way of meshing may be useful when you wish to mesh
    // a single part of an assembly (you would call MS_setNoMesh on the regions
    // of the other parts)
    pModelItem modelDomain = GM_domain(assemModel1);
    mesh = M_new(0, assemModel1);
    meshCase = MS_newMeshCase(assemModel1); // create a meshing case
    MS_setMeshSize(meshCase, modelDomain, 2, 0.02,
                   0); // set a relative mesh size on the model
    // create the meshers and run
    GM_write(assemModel1, "noImprintingAssembly.smd", 0,
             progress); // write out the model before the mesh!
    pSurfaceMesher surfaceMesher = SurfaceMesher_new(meshCase, mesh);
    SurfaceMesher_execute(surfaceMesher, progress);
    pVolumeMesher volumeMesher = VolumeMesher_new(meshCase, mesh);
    VolumeMesher_execute(volumeMesher, progress);
    M_write(mesh, "noImprinting.sms", 0,
            progress); // writes out the mesh to SimModSuite format
    // clean up meshers, meshing case, and mesh
    SurfaceMesher_delete(surfaceMesher);
    VolumeMesher_delete(volumeMesher);
    MS_deleteMeshCase(meshCase);
    M_release(mesh);
    // For the second assembly model, we will first imprint the assembly, which
    // will create additional faces where the parts of the assembly model touch.
    // This then allows mesh matching to be used to create conforming meshes for
    // the separate parts of the assembly.
    assemModel2 = GAM_createImprintedAssemblyModel(assemModel1, 0, progress);
    // check that the input model is topologically valid before meshing
    modelErrors = PList_new();
    if (!GM_isValid(assemModel2, 0, modelErrors)) {
      cerr << "Input model not valid" << endl;
      cerr << "Number of errors returned: " << PList_size(modelErrors) << endl;
      GM_release(assemModel2);
      NM_release(nModel);
      return 1;
    }
    PList_delete(modelErrors);
    pModelItem modelDomain2 = GM_domain(assemModel2);
    mesh = M_new(0, assemModel2);
    pACase meshCase = MS_newMeshCase(assemModel2); // create a meshing case
    // Now find the faces that contact other faces, and use mesh match on those
    // faces Mesh matching can only be set once for a set of faces, so use a
    // PList to manage what faces have already been used.
    pPList contactErrors = PList_new();
    pModelContactInfo contactInfo =
        ModelContactInfo_create(assemModel2, contactErrors, progress);
    if (PList_size(contactErrors)) {
      cerr << "Input model has contact errors" << endl;
      cerr << "Number of errors returned: " << PList_size(contactErrors)
           << endl;
      GM_release(assemModel2);
      NM_release(nModel);
      return 1;
    }
    PList_delete(contactErrors);
    pPList destFaces = PList_new(); // empty list to hold faces used as
                                    // destination faces in mesh matching
    GFIter modelFaces = GM_faceIter(assemModel2);
    pGFace sourceFace;
    while (sourceFace = GFIter_next(modelFaces)) {
      // check if this face is already a destination face
      if (!PList_contains(destFaces, sourceFace)) {
        pPList contacts =
            ModelContactInfo_getContactList(contactInfo, sourceFace);
        if (contacts) {
          int contactSize = PList_size(contacts);
          for (int i = 0; i < contactSize; i++) {
            pGFace destinationFace = (pGFace)PList_item(contacts, i);
            // add the destination face to our list, then mesh match
            PList_append(destFaces, destinationFace);
            MS_setMeshMatch(meshCase, sourceFace, destinationFace, 0, 0, 0, 0,
                            0);
          }
        }
        PList_delete(contacts);
      }
    }
    ModelContactInfo_delete(contactInfo);
    PList_delete(destFaces);
    GFIter_delete(modelFaces);

    MS_setMeshSize(meshCase, modelDomain2, 2, 0.02,
                   0); // set a relative mesh size on the model
    // create the meshers and run
    surfaceMesher = SurfaceMesher_new(meshCase, mesh);
    SurfaceMesher_execute(surfaceMesher, progress);
    volumeMesher = VolumeMesher_new(meshCase, mesh);
    VolumeMesher_execute(volumeMesher, progress);

    // Imprinting caused a new native model to be created.  We need to write
    // that native model out to file
    nativeUnioned = GM_nativeModel(assemModel2);
    // NM_write(nativeUnioned, "ImprintedNativeModel.x_t", progress);
    NM_write(nativeUnioned, "ImprintedNativeModel.brep", progress);
    GM_write(assemModel2, "ImprintedAssemblyModel.smd", 0,
             progress); // write out the model before the mesh!
    M_write(mesh, "ImprintedMesh.sms", 0,
            progress); // writes out the mesh to SimModSuite format
    // clean up meshers, meshing case, and mesh
    SurfaceMesher_delete(surfaceMesher);
    VolumeMesher_delete(volumeMesher);
    MS_deleteMeshCase(meshCase);
    M_release(mesh);
    // cleanup
    GM_release(assemModel1);
    GM_release(assemModel2);
    NM_release(nModel);
    NM_release(nativeUnioned);

    Progress_delete(progress);
    SimOpenCascade_stop(1);
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