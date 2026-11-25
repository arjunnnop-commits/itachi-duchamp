#include "config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "log.h"

Config& Config::get() {
  static Config c;
  return c;
}

static std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

void Config::load() {
  std::vector<std::string> paths;
  if (const char* env = getenv("ITACHI_CONF")) paths.push_back(env);
  paths.push_back("/odm/etc/itachi.conf");
  paths.push_back("/vendor/etc/itachi.conf");
  for (const auto& p : paths) {
    std::ifstream f(p);
    if (!f.is_open()) continue;
    std::string line;
    while (std::getline(f, line)) {
      size_t h = line.find('#');
      if (h != std::string::npos) line.erase(h);
      size_t eq = line.find('=');
      if (eq == std::string::npos) continue;
      std::string k = trim(line.substr(0, eq));
      std::string v = trim(line.substr(eq + 1));
      if (!k.empty()) kv_[k] = v;
    }
    LOGI("config loaded from %s (%zu keys)", p.c_str(), kv_.size());
    return;
  }
  LOGW("no config file found, using built-in defaults");
}

bool Config::has(const std::string& key) const { return kv_.count(key) > 0; }

std::string Config::getStr(const std::string& key, const std::string& def) const {
  auto it = kv_.find(key);
  return it == kv_.end() ? def : it->second;
}

int Config::getInt(const std::string& key, int def) const {
  auto it = kv_.find(key);
  if (it == kv_.end() || it->second.empty()) return def;
  return static_cast<int>(strtol(it->second.c_str(), nullptr, 0));
}

double Config::getDouble(const std::string& key, double def) const {
  auto it = kv_.find(key);
  if (it == kv_.end() || it->second.empty()) return def;
  return strtod(it->second.c_str(), nullptr);
}

std::vector<std::string> Config::getStrList(const std::string& key) const {
  std::vector<std::string> out;
  auto it = kv_.find(key);
  if (it == kv_.end()) return out;
  std::stringstream ss(it->second);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    tok = trim(tok);
    if (!tok.empty()) out.push_back(tok);
  }
  return out;
}

std::vector<int> Config::getIntList(const std::string& key) const {
  std::vector<int> out;
  for (const auto& s : getStrList(key)) out.push_back(static_cast<int>(strtol(s.c_str(), nullptr, 0)));
  return out;
}
