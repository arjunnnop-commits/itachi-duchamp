#include "backend.h"

#include "aod.h"
#include "config.h"
#include "fod.h"
#include "log.h"

static std::string join(const std::vector<int32_t>& v) {
  std::string s;
  for (size_t i = 0; i < v.size(); ++i) s += (i ? "," : "") + std::to_string(v[i]);
  return s;
}

int32_t Backend::get(int32_t id, std::vector<int32_t>* vals) {
  const Config& c = Config::get();
  size_t n = vals->size();
  int def = c.getInt("get_default", 0);
  for (auto& x : *vals) x = def;

  std::string key = "get." + std::to_string(id);
  if (c.has(key)) {
    std::vector<int> cfg = c.getIntList(key);
    for (size_t i = 0; i < n && i < cfg.size(); ++i) (*vals)[i] = cfg[i];
  } else if (id == fid::kCapabilities && n > 0) {
    (*vals)[0] = c.getInt("cap211", 0x410);
  }
  if (c.getInt("probe", 1)) LOGI("get(%d, n=%zu) -> [%s]", id, n, join(*vals).c_str());
  return 0;
}

int32_t Backend::set(int32_t id, const std::vector<int32_t>& vals) {
  if (Config::get().getInt("probe", 1)) LOGI("set(%d,[%s])", id, join(vals).c_str());
  int32_t v = vals.empty() ? 0 : vals[0];
  switch (id) {
    case fid::kFpPress:  // from the framework (services.jar) when a finger is pressed/released
      FodController::get().press(v != 0);
      break;
    case fid::kDimlayHbm:  // from SystemUI OnScreenHighLightControl.hbmControl(boolean)
      FodController::get().hbm(v != 0);
      break;
    case fid::kAod:  // SystemUI setAODLightMode(mode)
      AodController::get().setLightMode(v);
      break;
    default:
      break;  // accepted and ignored (logged above)
  }
  return 0;
}

int32_t Backend::info(int32_t id, std::vector<std::string>* out) {
  const Config& c = Config::get();
  *out = c.getStrList("info." + std::to_string(id));
  if (c.getInt("probe", 1)) LOGI("info(%d) -> %zu strings", id, out->size());
  return 0;
}
