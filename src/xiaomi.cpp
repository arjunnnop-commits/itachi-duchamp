#include "xiaomi.h"

#include "config.h"
#include "log.h"
#include "remote.h"

namespace xiaomi {

static RemoteService& extSvc() {
  static RemoteService s(
      "vendor.xiaomi.hardware.fingerprintextension.IXiaomiFingerprint",
      Config::get().getStr("ext_service",
                           "vendor.xiaomi.hardware.fingerprintextension.IXiaomiFingerprint/default"));
  return s;
}

static RemoteService& dispSvc() {
  static RemoteService s(
      "vendor.xiaomi.hardware.displayfeature_aidl.IDisplayFeature",
      Config::get().getStr("df_service",
                           "vendor.xiaomi.hardware.displayfeature_aidl.IDisplayFeature/default"));
  return s;
}

int extCmd(int cmd, int param) {
  int32_t ret = -1;
  // extCmd is the first (and only) real method: transaction code 1.
  binder_status_t st = extSvc().transact(
      1,
      [&](AParcel* in) {
        AParcel_writeInt32(in, cmd);
        AParcel_writeInt32(in, param);
      },
      [&](const AParcel* out) { AParcel_readInt32(out, &ret); });
  if (Config::get().getInt("probe", 1))
    LOGI("extCmd(%d,%d) -> st=%d ret=%d", cmd, param, static_cast<int>(st), ret);
  return st == STATUS_OK ? ret : -1;
}

bool setFeature(int display, int mode, int value, int cookie) {
  // Declaration order from the proxy vtable: notifyBrightness=1, registerCallback=2,
  // sendMessage=3, sendPanelCommand=4, sendPostProcCommand=5, sendRefreshCommand=6,
  // setFeature=7, setFunction=8. Overridable via df_setfeature_code in the config.
  int code = Config::get().getInt("df_setfeature_code", 7);
  binder_status_t st = dispSvc().transact(code, [&](AParcel* in) {
    AParcel_writeInt32(in, display);
    AParcel_writeInt32(in, mode);
    AParcel_writeInt32(in, value);
    AParcel_writeInt32(in, cookie);
  }, nullptr);
  LOGI("setFeature(%d,%d,%d,%d) -> st=%d", display, mode, value, cookie, static_cast<int>(st));
  return st == STATUS_OK;
}

}  // namespace xiaomi
