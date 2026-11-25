#pragma once
#include <map>
#include <string>
#include <vector>

// Tiny key=value config reader ("#" starts a comment).
class Config {
 public:
  static Config& get();
  void load();
  int getInt(const std::string& key, int def) const;
  double getDouble(const std::string& key, double def) const;
  std::string getStr(const std::string& key, const std::string& def) const;
  std::vector<int> getIntList(const std::string& key) const;
  std::vector<std::string> getStrList(const std::string& key) const;
  bool has(const std::string& key) const;

 private:
  std::map<std::string, std::string> kv_;
};
