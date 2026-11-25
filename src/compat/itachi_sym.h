#pragma once
#include <dlfcn.h>
namespace itachi_compat {
inline void* sym(const char* name) {
  void* p = dlsym(RTLD_DEFAULT, name);
  if (!p) {
    void* h = dlopen("libbinder_ndk.so", RTLD_NOW);
    if (h) p = dlsym(h, name);
  }
  return p;
}
}  // namespace itachi_compat
