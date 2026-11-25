#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>

// Translates ColorOS FOD requests into the extCmd sequence the Goodix HAL expects
// (cmd 1 = finger down/up, 2/3 = x/y, 10 = HBM + spot ready).
//   set(28, v)  FPPRESS   -> press(v): finger markers
//   set(22, v)  HBM ctrl  -> hbm(v):   LHBM on/off + "spot ready" (SystemUI sends it when the icon is lit)
class FodController {
 public:
  static FodController& get();
  void press(bool down);  // non-blocking
  void hbm(bool on);      // non-blocking

 private:
  void runPress(bool down, uint64_t gen);
  void runHbm(bool on, uint64_t gen);
  void sendCoords(bool zero);
  void spotReady(bool on);
  std::atomic<uint64_t> gen_{0};
  std::mutex seq_;
};
