#pragma once
#include <cstdint>

namespace xiaomi {
// IXiaomiFingerprint::extCmd(int cmd, int param) -> int
int extCmd(int cmd, int param);
// IDisplayFeature::setFeature(int display, int mode, int value, int cookie)
bool setFeature(int display, int mode, int value, int cookie);
}  // namespace xiaomi
