#include "MeshSim.h"
#include "SimMessages.h"
#include "SimParasolidKrnl.h"
#include "SimUtil.h"
#include "spdlog/spdlog.h"
#include <iostream>

void messageHandler(int type, const char *msg);
void progressHandler(const char *what, int level, int startVal, int endVal,
                     int currentVal, void *);

void init() {
  Sim_logOn("sms.log");
  MS_init();
  Sim_readLicenseFile("TheaEnergyEval2025");
  SimParasolid_start(1);
  Sim_setMessageHandler(messageHandler);
}

void terminate() {
  SimParasolid_stop(1);
  Sim_unregisterAllKeys();
  MS_exit();
  Sim_logOff();
}

void messageHandler(int type, const char *msg) {
  switch (type) {
  case Sim_InfoMsg:
    spdlog::info(msg);
    break;
  case Sim_DebugMsg:
    spdlog::debug(msg);
    break;
  case Sim_WarningMsg:
    spdlog::warn(msg);
    break;
  case Sim_ErrorMsg:
    spdlog::error(msg);
    break;
  }
  return;
}
void progressHandler(const char *what, int level, int startVal, int endVal,
                     int currentVal, void *) {
  std::cout << "Progress: " << what << ", level: " << level;
  std::cout << ", startVal: " << startVal << ", endVal: " << endVal;
  std::cout << ", currentVal: " << currentVal << std::endl;
  return;
}