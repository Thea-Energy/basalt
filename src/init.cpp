#include "init.h"
#include "MeshSim.h"
#include "SimLicense.h"
#include "SimMessages.h"
#include "SimParasolidKrnl.h"
#include "SimPartitionedMesh.h"
#include "SimUtil.h"
#include "spdlog/spdlog.h"
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <string>
#include <unistd.h>

void messageHandler(int type, const char *msg);
void progressHandler(const char *what, int level, int startVal, int endVal,
                     int currentVal, void *);

static std::atomic<bool> g_sms_initialized{false};

// Path of a pointer license file basalt synthesized for a `port@host` spec, if
// any. Kept alive for the process lifetime (RLM may re-read it when reconnecting
// to the server) and removed in terminate().
static std::string g_license_pointer_path;

// Returns the value of the first non-empty environment variable in `names`, or
// nullptr if none are set. Lets basalt honor several documented license knobs.
static const char *first_env(std::initializer_list<const char *> names) {
  for (const char *name : names) {
    const char *value = std::getenv(name);
    if (value != nullptr && value[0] != '\0') {
      return value;
    }
  }
  return nullptr;
}

// A `port@host` license spec has an all-digit (or empty, meaning the RLM default
// port) prefix before the '@' and a non-empty host after it. This distinguishes
// a network license from a filesystem path that merely contains an '@'.
static bool is_port_at_host(const std::string &spec) {
  auto at = spec.find('@');
  if (at == std::string::npos || at + 1 >= spec.size()) {
    return false;
  }
  for (std::size_t i = 0; i < at; ++i) {
    if (!std::isdigit(static_cast<unsigned char>(spec[i]))) {
      return false;
    }
  }
  return true;
}

// SimLicense_start() only opens its argument as a file, so a raw `port@host`
// string fails with "License file ... could not be accessed". Synthesize a
// minimal RLM client pointer file that directs the RLM client at the server and
// return its path. The file is removed in terminate().
static std::string write_license_pointer_file(const std::string &port_at_host) {
  auto at = port_at_host.find('@');
  std::string port = port_at_host.substr(0, at);
  std::string host = port_at_host.substr(at + 1);

  char path_template[] = "/tmp/basalt_license_XXXXXX";
  int fd = mkstemp(path_template);
  if (fd < 0) {
    throw_exception("basalt could not create a temporary license pointer file "
                    "for the 'port@host' license specification.");
  }

  // An empty port lets the RLM client fall back to its default server port.
  std::string contents = "HOST " + host + " ANY";
  if (!port.empty()) {
    contents += " " + port;
  }
  contents += "\nISV simmetrix\n";

  ssize_t written = ::write(fd, contents.c_str(), contents.size());
  ::close(fd);
  if (written != static_cast<ssize_t>(contents.size())) {
    ::unlink(path_template);
    throw_exception("basalt failed to write the temporary license pointer file "
                    "for the 'port@host' license specification.");
  }

  g_license_pointer_path = path_template;
  return g_license_pointer_path;
}

void init() {
  // Bake-time default; setenv with overwrite=0 preserves any customer override
  // already in the environment.
  setenv("P_SCHEMA", PSKRNL_SCHEMA_DIR, 0);

  SimPartitionedMesh_start(nullptr, nullptr);
  Sim_logOn("sms.log");
  MS_init();
  // Resolve the license specification. Precedence (first non-empty wins):
  //   1. SimModSuite_licenseFile — basalt's historical, case-sensitive knob
  //   2. RLM_LICENSE             — RLM standard (IT-recommended)
  //   3. simmetrix_LICENSE       — RLM ISV-specific standard
  // The value may be a path to a license file OR a 'port@host' string pointing
  // at a network license server (e.g. "2800@license-server").
  const char *license_spec =
      first_env({"SimModSuite_licenseFile", "RLM_LICENSE", "simmetrix_LICENSE"});
  if (license_spec == nullptr) {
    throw_exception(
        "No Simmetrix license configured. Set one of SimModSuite_licenseFile, "
        "RLM_LICENSE, or simmetrix_LICENSE (case-sensitive) before importing "
        "basalt. The value may be a license file path or a 'port@host' string "
        "for a network license server (e.g. '2800@license-server'). "
        "See docs/install.rst.");
  }

  // SimLicense_start() treats its argument strictly as a file path, so a
  // 'port@host' spec is turned into a synthesized RLM client pointer file.
  std::string license_file =
      is_port_at_host(license_spec) ? write_license_pointer_file(license_spec)
                                    : std::string(license_spec);
  SimLicense_start(
      "geomsim_core,geomsim_parasolid,meshsim_surface,meshsim_volume",
      license_file.c_str());

  SimParasolid_start(1);
  Sim_setMessageHandler(messageHandler);
  g_sms_initialized.store(true, std::memory_order_release);
}

void terminate() {
  g_sms_initialized.store(false, std::memory_order_release);
  SimParasolid_stop(1);
  SimLicense_stop();
  MS_exit();
  Sim_logOff();
  SimPartitionedMesh_start(nullptr, nullptr);

  if (!g_license_pointer_path.empty()) {
    ::unlink(g_license_pointer_path.c_str());
    g_license_pointer_path.clear();
  }
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