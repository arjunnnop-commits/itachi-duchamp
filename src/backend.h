#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Feature IDs the ColorOS framework sends (extracted from its DEX constants).
namespace fid {
constexpr int kFfl = 6;
constexpr int kAod = 7;
constexpr int kMaxBrightness = 8;
constexpr int kPanelInfo = 9;
constexpr int kHbm = 12;
constexpr int kDimAlpha = 13;
constexpr int kDimlayHbm = 22;
constexpr int kPanelBlank = 24;
constexpr int kFpPress = 28;
constexpr int kCapabilities = 211;  // bit 2 (4) = HW dimlayer, bit 10 (1024) = local HBM accel
constexpr int kPulseState = 264;
}  // namespace fid

struct Backend {
  static int32_t get(int32_t id, std::vector<int32_t>* vals);
  static int32_t set(int32_t id, const std::vector<int32_t>& vals);
  static int32_t info(int32_t id, std::vector<std::string>* out);
};
