#include "init.h"
#include "MeshSim.h"
#include "SimMessages.h"
#include "SimParasolidKrnl.h"
#include "SimPartitionedMesh.h"
#include "SimUtil.h"
#include "spdlog/spdlog.h"
#include <atomic>
#include <iostream>

void messageHandler(int type, const char *msg);
void progressHandler(const char *what, int level, int startVal, int endVal,
                     int currentVal, void *);

static std::atomic<bool> g_sms_initialized{false};

void init() {
  SimPartitionedMesh_start(nullptr, nullptr);
  Sim_logOn("sms.log");
  MS_init();
  Sim_readLicenseFile("TheaEnergyEval2025_Extension");
  SimParasolid_start(1);
  Sim_setMessageHandler(messageHandler);
  g_sms_initialized.store(true, std::memory_order_release);
}

void terminate() {
  g_sms_initialized.store(false, std::memory_order_release);
  SimParasolid_stop(1);
  Sim_unregisterAllKeys();
  MS_exit();
  Sim_logOff();
  SimPartitionedMesh_start(nullptr, nullptr);
}

bool sms_is_initialized() {
  return g_sms_initialized.load(std::memory_order_acquire);
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
    throw_exception(msg);
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