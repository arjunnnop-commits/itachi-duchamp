#pragma once
#include <string>

// Build-time authorization. The binary only runs when /odm/etc/itachi.lic holds
// HMAC-SHA256(secret, "<authorized name>|<device>") for the secret baked into this build.
namespace auth {
constexpr const char* kAuthorizedName = "joditachi";
constexpr const char* kDevice = "duchamp";

bool isAuthorized(std::string* why);

// exposed for the host-side self test
std::string hmacSha256Hex(const std::string& key, const std::string& msg);
}  // namespace auth
