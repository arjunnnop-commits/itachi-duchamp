#include <android/binder_process.h>

#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "aod.h"
#include "auth.h"
#include "config.h"
#include "fod.h"
#include "log.h"
#include "panel_service.h"
#include "remote.h"
#include "xiaomi.h"

// ---- self-test client: talks to the running itachid service exactly like ColorOS would ----
static RemoteService& self() {
  static RemoteService s("vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature",
                         "vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default");
  return s;
}

static bool i32Alloc(void* arrayData, int32_t length, int32_t** outBuffer) {
  auto* v = static_cast<std::vector<int32_t>*>(arrayData);
  if (length < 0) { v->clear(); *outBuffer = nullptr; return true; }
  v->assign(static_cast<size_t>(length), 0);
  *outBuffer = v->data();
  return true;
}

static int svcGet(int id, int len) {
  std::vector<int32_t> vals;
  int32_t ret = -99;
  binder_status_t st = self().transact(
      1, [&](AParcel* in) { AParcel_writeInt32(in, id); AParcel_writeInt32(in, len); },
      [&](const AParcel* out) { AParcel_readInt32(out, &ret); AParcel_readInt32Array(out, &vals, i32Alloc); });
  printf("get(%d,len=%d) st=%d ret=%d vals=[", id, len, static_cast<int>(st), ret);
  for (size_t i = 0; i < vals.size(); ++i) printf("%s%d", i ? "," : "", vals[i]);
  printf("]\n");
  return st == STATUS_OK ? 0 : 1;
}

static int svcSet(int id, const std::vector<int32_t>& v) {
  int32_t ret = -99;
  binder_status_t st = self().transact(
      2, [&](AParcel* in) { AParcel_writeInt32(in, id); AParcel_writeInt32Array(in, v.data(), static_cast<int32_t>(v.size())); },
      [&](const AParcel* out) { AParcel_readInt32(out, &ret); });
  printf("set(%d) st=%d ret=%d\n", id, static_cast<int>(st), ret);
  return st == STATUS_OK ? 0 : 1;
}

static void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

static int cli(int argc, char** argv) {
  std::string cmd = argv[1];
  auto arg = [&](int i) { return i < argc ? atoi(argv[i]) : 0; };
  if (cmd == "ext" && argc >= 4) {
    int r = xiaomi::extCmd(arg(2), arg(3));
    printf("extCmd(%d,%d) -> %d\n", arg(2), arg(3), r);
    return 0;
  }
  if (cmd == "aod" && argc >= 3) {
    AodController::get().set(arg(2) != 0);
    return 0;
  }
  if (cmd == "setfeature" && argc >= 6) {
    printf("%s\n", xiaomi::setFeature(arg(2), arg(3), arg(4), arg(5)) ? "ok" : "fail");
    return 0;
  }
  if (cmd == "svc" && argc >= 4) {
    std::string op = argv[2];
    int id = atoi(argv[3]);
    if (op == "get") return svcGet(id, argc >= 5 ? atoi(argv[4]) : 1);
    if (op == "set") {
      std::vector<int32_t> v;
      for (int i = 4; i < argc; ++i) v.push_back(atoi(argv[i]));
      return svcSet(id, v);
    }
  }
  if (cmd == "sim-fod") {  // what ColorOS sends during one FOD attempt
    int hold = argc >= 3 ? atoi(argv[2]) : 400;
    svcGet(211, 1);
    svcSet(28, {1});
    sleepMs(100);
    svcSet(22, {1});
    sleepMs(hold);
    svcSet(22, {0});
    svcSet(28, {0});
    return 0;
  }
  fprintf(stderr,
          "usage: itachid                      run the service\n"
          "       itachid whoami            show authorization state\n"
          "       itachid ext <cmd> <param>    call IXiaomiFingerprint::extCmd\n"
          "       itachid aod <0|1>            test the AOD nodes\n"
          "       itachid svc get <id> [len]   call our own service like ColorOS does\n"
          "       itachid svc set <id> <v..>\n"
          "       itachid sim-fod [hold_ms]    replay one ColorOS FOD attempt through the service\n"
          "       itachid setfeature <disp> <mode> <value> <cookie>\n");
  return 1;
}

int main(int argc, char** argv) {
  Config::get().load();
  {
    std::string why;
    bool ok = auth::isAuthorized(&why);
    if (argc > 1 && std::string(argv[1]) == "whoami") {
      printf("itachid (authorized name: %s, device: %s): %s\n", auth::kAuthorizedName, auth::kDevice,
             ok ? "AUTHORIZED" : ("NOT AUTHORIZED - " + why).c_str());
      return ok ? 0 : 1;
    }
    if (!ok) {
      LOGE("not authorized: %s", why.c_str());
      fprintf(stderr, "itachid: not authorized (%s)\n", why.c_str());
      return 1;
    }
  }
  if (argc > 1) return cli(argc, argv);

  ABinderProcess_setThreadPoolMaxThreadCount(3);
  if (!registerPanelService()) return 1;
  ABinderProcess_startThreadPool();
  ABinderProcess_joinThreadPool();
  return 0;
}
