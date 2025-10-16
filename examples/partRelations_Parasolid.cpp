/*******************************************************************
 * This document is confidential information.
 * Copyright 1997-2025 Simmetrix Inc. All rights reserved. This
 * document is an unpublished work fully protected by the United
 * States copyright laws and is considered a trade secret belonging
 * to the copyright holder. Disclosure, use, or reproduction without
 * the written authorization of Simmetrix Inc. is prohibited.
 *******************************************************************/
#include "MeshEnums.h"
#include "MeshTypes.h"
#include "ModelEnums.h"
#include "ModelTypes.h"
#include "SimInfo.h"
#include "SimInfoCodes.h"
#include "SimMessages.h"
#include "SimModel.h"
#include "SimParasolidKrnl.h"
#include "SimUtil.h"
#include <iostream>
using namespace std;

void messageHandler(int type, const char *msg);
void progressHandler(const char *what, int level, int startVal, int endVal,
                     int currentVal, void *);

int main(int argc, char *argv[]) {

  pParasolidNativeModel nModel;
  pGModel assemblyModel;
  pGModel model;

  // You will want to place a try/catch around all SimModSuite calls
  // as errors are thrown.
  try {
    Sim_logOn("relations.log");
    SimModel_start(); // Call before Sim_readLicenseFile
    // NOTE: Sim_readLicenseFile() is for internal testing only.  To use,
    // pass in the location of a file containing your keys.  For a release
    // product, use Sim_registerKey()
    Sim_readLicenseFile("TheaEnergyEval2025");
    SimParasolid_start(1);

    Sim_setMessageHandler(messageHandler);
    pProgress progress = Progress_new();
    Progress_setCallback(progress, progressHandler);

    // Load the model as an assembly model
    // nModel = ParasolidNM_createFromFile(
    // "doc/Simmetrix/GeomSim/input/relationBoxes.xmt_txt", 0);
    nModel = ParasolidNM_createFromFile(
        "data/model_G1600-with-3-blanket-layers_flattened.x_t", 0);
    assemblyModel = GAM_createFromNativeModel(nModel, progress);
    NM_release(nModel);

    // Build the non-manifold model
    pModelBuilder modelBuild = ModelBuilder_new(assemblyModel);
    // We want to union the assembly
    ModelBuilder_setOperation(modelBuild, ModelBuilder_unite);
    // If there are failed unions, keep the failed parts separate
    ModelBuilder_setFailureBehavior(modelBuild, 1);
    // Create a connector between the assembly model and the non-manifold model
    // Set the connector in the ModelBuilder options
    pMConnector connector = MC_new();
    ModelBuilder_setConnector(modelBuild, connector);
    // Execute the model builder - returns the non-manifold model
    model = ModelBuilder_execute(modelBuild, progress);
    // Delete the model builder
    ModelBuilder_delete(modelBuild);

    // check that the input model is topologically valid
    pPList modelErrors = PList_new();
    if (!GM_isValid(model, 0, modelErrors)) {
      cerr << "Input model not valid" << endl;
      cerr << "Number of errors returned: " << PList_size(modelErrors) << endl;
      GM_release(model);
      GM_release(assemblyModel);
      return 1;
    }
    PList_delete(modelErrors);

    pANMConnection ModelConnection = ANMConnection_new(connector);

    cout << "Number of assemblies in Assembly Model: "
         << GAM_numAssemblies(assemblyModel) << endl;
    GAIter assemblies = GAM_AIter(assemblyModel, 1);
    pGAssembly assembly;
    while ((assembly = GAIter_next(assemblies)) != 0) {
      char *nameAssembly = GA_nativeName(assembly);
      if (nameAssembly != 0) {
        cout << "Assembly name: " << nameAssembly << endl;
        Sim_deleteString(nameAssembly);
      } else
        cout << "No name for this assembly" << endl;
    }
    GAIter_delete(assemblies);

    cout << "Number of parts in Assembly Model: " << GAM_numParts(assemblyModel)
         << endl;
    GIPIter assemParts = GAM_IPIter(assemblyModel, 1);
    pGIPart assemPart;
    while ((assemPart = GIPIter_next(assemParts)) != 0) {
      char *assemName = GIP_nativeName(assemPart);
      if (assemName != 0) {
        cout << "Part name: " << assemName << endl;
        Sim_deleteString(assemName);
      } else
        cout << "No name for this part" << endl;
    }
    GIPIter_delete(assemParts);

    cout << endl;
    cout << "Non-manifold model has " << GM_numRegions(model) << " regions"
         << endl;
    GRIter modelRegions = GM_regionIter(model);
    pGRegion modelRegion;
    while ((modelRegion = GRIter_next(modelRegions)) != 0) {
      int regTag = GEN_tag(modelRegion);
      pPList list = ANMConnection_relatedParts(ModelConnection, modelRegion);
      int size = PList_size(list);
      void *iter = 0;
      pGIPart part;
      cout << "Region: " << regTag << ": is connected to " << size << " parts:";
      while ((part = (pGIPart)PList_next(list, &iter)) != 0) {
        char *name = GIP_nativeName(part);
        if (name != 0) {
          cout << " " << name;
          Sim_deleteString(name);
        } else
          cout << "No name for this part" << endl;
      }
      cout << endl;
      PList_delete(list);
    }
    GRIter_delete(modelRegions);

    ANMConnection_delete(ModelConnection);
    MC_release(connector);
    GM_release(assemblyModel);
    GM_release(model);
    Progress_delete(progress);

    SimParasolid_stop(1);
    Sim_unregisterAllKeys();
    SimModel_stop();
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
