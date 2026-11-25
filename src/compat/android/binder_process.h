#pragma once
#include <stdint.h>
#include "../itachi_sym.h"

inline bool ABinderProcess_setThreadPoolMaxThreadCount(uint32_t n) {
  using Fn = bool (*)(uint32_t);
  static Fn fn = reinterpret_cast<Fn>(itachi_compat::sym("ABinderProcess_setThreadPoolMaxThreadCount"));
  return fn ? fn(n) : false;
}

inline void ABinderProcess_startThreadPool() {
  using Fn = void (*)();
  static Fn fn = reinterpret_cast<Fn>(itachi_compat::sym("ABinderProcess_startThreadPool"));
  if (fn) fn();
}

inline void ABinderProcess_joinThreadPool() {
  using Fn = void (*)();
  static Fn fn = reinterpret_cast<Fn>(itachi_compat::sym("ABinderProcess_joinThreadPool"));
  if (fn) fn();
}
