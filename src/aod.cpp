#include "aod.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <vector>

#include "config.h"
#include "log.h"

AodController& AodController::get() {
  static AodController c;
  return c;
}

bool AodController::writeNode(const std::string& path, const std::string& value) {
  int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
  if (fd < 0) return false;
  ssize_t n = write(fd, value.c_str(), value.size());
  close(fd);
  return n == static_cast<ssize_t>(value.size());
}

void AodController::setLightMode(int mode) {
  // ColorOS sends an "AOD light mode" number, not a plain on/off. Its meaning is
  // calibrated through the config: aod_map = <mode>:<doze_brightness>,...
  const Config& c = Config::get();
  for (const auto& kv : c.getStrList("aod_map")) {
    size_t colon = kv.find(':');
    if (colon == std::string::npos) continue;
    if (atoi(kv.substr(0, colon).c_str()) == mode) {
      writeValue(atoi(kv.substr(colon + 1).c_str()));
      return;
    }
  }
  set(mode != 0);
}

void AodController::set(bool on) {
  const Config& c = Config::get();
  writeValue(on ? c.getInt("aod_on_value", 1) : c.getInt("aod_off_value", 0));
  std::string tnode = c.getStr("touch_aod_node", "/sys/devices/platform/goodix_ts.0/aod_enable");
  if (!tnode.empty()) writeNode(tnode, on ? "1" : "0");
}

void AodController::writeValue(int value) {
  const Config& c = Config::get();
  std::string v = std::to_string(value);
  bool on = value != 0;

  // Panel doze brightness: first node that accepts the write wins.
  std::vector<std::string> nodes = c.getStrList("aod_nodes");
  if (nodes.empty()) {
    nodes = {"/sys/class/mi_display/disp_feature/disp-DSI-0/doze_brightness",
             "/sys/class/drm/card0-DSI-1/doze_brightness"};
  }
  bool done = false;
  for (const auto& n : nodes) {
    if (writeNode(n, v)) {
      LOGI("AOD %s: wrote %s to %s", on ? "on" : "off", v.c_str(), n.c_str());
      done = true;
      break;
    }
  }
  if (!done) LOGW("AOD %s: no doze node accepted the write", on ? "on" : "off");

}
