#include "auth.h"

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

#include "log.h"

#ifndef ITACHI_SECRET_HEX
// Local/dev builds only. CI injects a real per-owner secret (see .github/workflows/build.yml).
#define ITACHI_SECRET_HEX "6974616368692d6465762d6b65792d6368616e67652d6d65"
#endif

namespace auth {
namespace {

struct Sha256 {
  uint32_t h[8];
  uint8_t buf[64];
  uint64_t total = 0;
  size_t n = 0;

  Sha256() {
    static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(h, init, sizeof(h));
  }

  static uint32_t rotr(uint32_t x, int c) { return (x >> c) | (x << (32 - c)); }

  void block(const uint8_t* p) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = (uint32_t(p[4 * i]) << 24) | (uint32_t(p[4 * i + 1]) << 16) | (uint32_t(p[4 * i + 2]) << 8) |
             uint32_t(p[4 * i + 3]);
    for (int i = 16; i < 64; ++i) {
      uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t t1 = hh + S1 + ch + k[i] + w[i];
      uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = S0 + mj;
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }

  void update(const uint8_t* p, size_t len) {
    total += len;
    while (len) {
      size_t take = std::min(len, sizeof(buf) - n);
      memcpy(buf + n, p, take);
      n += take; p += take; len -= take;
      if (n == 64) { block(buf); n = 0; }
    }
  }

  void finish(uint8_t out[32]) {
    uint64_t bits = total * 8;
    uint8_t pad = 0x80;
    update(&pad, 1);
    uint8_t z = 0;
    while (n != 56) update(&z, 1);
    uint8_t lenb[8];
    for (int i = 0; i < 8; ++i) lenb[i] = static_cast<uint8_t>(bits >> (56 - 8 * i));
    update(lenb, 8);
    for (int i = 0; i < 8; ++i) {
      out[4 * i] = h[i] >> 24; out[4 * i + 1] = h[i] >> 16; out[4 * i + 2] = h[i] >> 8; out[4 * i + 3] = h[i];
    }
  }
};

std::vector<uint8_t> sha256(const std::vector<uint8_t>& in) {
  Sha256 s;
  s.update(in.data(), in.size());
  std::vector<uint8_t> out(32);
  s.finish(out.data());
  return out;
}

std::vector<uint8_t> hmac(const std::vector<uint8_t>& key, const std::vector<uint8_t>& msg) {
  std::vector<uint8_t> k = key;
  if (k.size() > 64) k = sha256(k);
  k.resize(64, 0);
  std::vector<uint8_t> inner(64), outer(64);
  for (int i = 0; i < 64; ++i) { inner[i] = k[i] ^ 0x36; outer[i] = k[i] ^ 0x5c; }
  inner.insert(inner.end(), msg.begin(), msg.end());
  std::vector<uint8_t> ih = sha256(inner);
  outer.insert(outer.end(), ih.begin(), ih.end());
  return sha256(outer);
}

std::string toHex(const std::vector<uint8_t>& v) {
  static const char* d = "0123456789abcdef";
  std::string s;
  for (uint8_t b : v) { s += d[b >> 4]; s += d[b & 15]; }
  return s;
}

std::vector<uint8_t> fromHex(const std::string& s) {
  std::vector<uint8_t> out;
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i + 1 < s.size(); i += 2) {
    int a = nib(s[i]), b = nib(s[i + 1]);
    if (a < 0 || b < 0) break;
    out.push_back(static_cast<uint8_t>(a * 16 + b));
  }
  return out;
}

bool constEq(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  unsigned char r = 0;
  for (size_t i = 0; i < a.size(); ++i) r |= static_cast<unsigned char>(a[i] ^ b[i]);
  return r == 0;
}

std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

}  // namespace

std::string hmacSha256Hex(const std::string& key, const std::string& msg) {
  return toHex(hmac(std::vector<uint8_t>(key.begin(), key.end()), std::vector<uint8_t>(msg.begin(), msg.end())));
}

bool isAuthorized(std::string* why) {
  std::vector<uint8_t> secret = fromHex(ITACHI_SECRET_HEX);
  std::string msg = std::string(kAuthorizedName) + "|" + kDevice;
  std::string expected = toHex(hmac(secret, std::vector<uint8_t>(msg.begin(), msg.end())));

  const char* paths[] = {"/odm/etc/itachi.lic", "/vendor/etc/itachi.lic"};
  for (const char* p : paths) {
    std::ifstream f(p);
    if (!f.is_open()) continue;
    std::string got;
    std::getline(f, got);
    if (constEq(trim(got), expected)) return true;
    if (why) *why = std::string("license file ") + p + " does not match this build";
    return false;
  }
  if (why) *why = "no itachi.lic found in /odm/etc or /vendor/etc";
  return false;
}

}  // namespace auth
