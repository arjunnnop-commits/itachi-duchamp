#include "fod.h"

#include <chrono>
#include <cmath>
#include <thread>

#include "config.h"
#include "log.h"
#include "xiaomi.h"

FodController& FodController::get() {
  static FodController c;
  return c;
}

void FodController::press(bool down) {
  uint64_t g = ++gen_;
  std::thread([this, down, g] { runPress(down, g); }).detach();
}

void FodController::hbm(bool on) {
  uint64_t g = ++gen_;
  std::thread([this, on, g] { runHbm(on, g); }).detach();
}

void FodController::sendCoords(bool zero) {
  const Config& c = Config::get();
  int x = 0, y = 0;
  if (!zero) {
    double scale = c.getDouble("touch_scale", 1.6);
    x = c.has("fod_x_raw") ? c.getInt("fod_x_raw", 0)
                           : static_cast<int>(std::lround(c.getInt("fod_center_x", 609) * scale));
    y = c.has("fod_y_raw") ? c.getInt("fod_y_raw", 0)
                           : static_cast<int>(std::lround(c.getInt("fod_center_y", 2437) * scale));
  }
  xiaomi::extCmd(2, x);
  xiaomi::extCmd(3, y);
}

void FodController::spotReady(bool on) {
  sendCoords(false);
  xiaomi::extCmd(10, on ? 1 : 0);  // "HBM and green spot ready" / "vanished"
}

void FodController::runPress(bool down, uint64_t gen) {
  const Config& c = Config::get();
  std::lock_guard<std::mutex> lock(seq_);
  if (gen != gen_.load()) return;
  bool sendFinger = c.getInt("send_finger_cmd", 1) != 0;
  bool readyFromPress = c.getStr("ready_mode", "hbm") == "press";

  if (down) {
    LOGI("FOD press");
    if (sendFinger) {
      sendCoords(true);
      xiaomi::extCmd(1, 1);
    }
    if (readyFromPress) {
      std::this_thread::sleep_for(std::chrono::milliseconds(c.getInt("ready_delay_ms", 60)));
      if (gen == gen_.load()) spotReady(true);
    }
  } else {
    LOGI("FOD release");
    if (readyFromPress) spotReady(false);
    if (sendFinger) {
      sendCoords(true);
      xiaomi::extCmd(1, 0);
    }
  }
}

void FodController::runHbm(bool on, uint64_t gen) {
  const Config& c = Config::get();
  std::lock_guard<std::mutex> lock(seq_);
  if (gen != gen_.load()) return;
  int lhbmMode = c.getInt("lhbm_mode", -1);
  int lhbmCookie = c.getInt("lhbm_cookie", 0);
  bool readyFromHbm = c.getStr("ready_mode", "hbm") == "hbm";

  LOGI("FOD hbm %s", on ? "on" : "off");
  if (on) {
    if (lhbmMode >= 0) xiaomi::setFeature(0, lhbmMode, 1, lhbmCookie);
    if (readyFromHbm) {
      std::this_thread::sleep_for(std::chrono::milliseconds(c.getInt("hbm_ready_delay_ms", 20)));
      if (gen == gen_.load()) spotReady(true);
    }
  } else {
    if (readyFromHbm) spotReady(false);
    if (lhbmMode >= 0) xiaomi::setFeature(0, lhbmMode, 0, lhbmCookie);
  }
}
