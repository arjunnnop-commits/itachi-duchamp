#pragma once
#include <android/binder_ibinder.h>
#include <android/binder_status.h>
#include "../itachi_sym.h"

inline binder_exception_t AServiceManager_addService(AIBinder* binder, const char* instance) {
  using Fn = binder_exception_t (*)(AIBinder*, const char*);
  static Fn fn = reinterpret_cast<Fn>(itachi_compat::sym("AServiceManager_addService"));
  return fn ? fn(binder, instance) : EX_UNSUPPORTED_OPERATION;
}

inline AIBinder* AServiceManager_checkService(const char* instance) {
  using Fn = AIBinder* (*)(const char*);
  static Fn fn = reinterpret_cast<Fn>(itachi_compat::sym("AServiceManager_checkService"));
  return fn ? fn(instance) : nullptr;
}
