#pragma once
#include <string>

class AodController {
 public:
  static AodController& get();
  void set(bool on);
  void setLightMode(int mode);  // feature 7 value from SystemUI/framework
  void writeValue(int value);
  static bool writeNode(const std::string& path, const std::string& value);
};
