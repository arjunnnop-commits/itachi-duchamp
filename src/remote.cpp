#include "remote.h"

#include <android/binder_manager.h>

#include "log.h"

static void* noopCreate(void* args) { return args; }
static void noopDestroy(void*) {}
static binder_status_t noopTransact(AIBinder*, transaction_code_t, const AParcel*, AParcel*) {
  return STATUS_UNKNOWN_TRANSACTION;
}

RemoteService::RemoteService(std::string descriptor, std::string instance)
    : desc_(std::move(descriptor)), inst_(std::move(instance)) {
  cls_ = AIBinder_Class_define(desc_.c_str(), noopCreate, noopDestroy, noopTransact);
}

bool RemoteService::ensure() {
  if (bin_) return true;
  AIBinder* b = AServiceManager_checkService(inst_.c_str());
  if (!b) {
    LOGW("service %s not available", inst_.c_str());
    return false;
  }
  if (!AIBinder_associateClass(b, cls_)) {
    LOGE("descriptor mismatch for %s", inst_.c_str());
    AIBinder_decStrong(b);
    return false;
  }
  bin_ = b;
  return true;
}

binder_status_t RemoteService::transact(transaction_code_t code, const Writer& wr, const Reader& rd) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (!ensure()) return STATUS_NAME_NOT_FOUND;

  AParcel* in = nullptr;
  binder_status_t st = AIBinder_prepareTransaction(bin_, &in);
  if (st != STATUS_OK) return st;
  if (wr) wr(in);

  AParcel* out = nullptr;
  st = AIBinder_transact(bin_, code, &in, &out, 0);
  if (st == STATUS_DEAD_OBJECT) {
    AIBinder_decStrong(bin_);
    bin_ = nullptr;
  }
  if (st != STATUS_OK) return st;

  AStatus* s = nullptr;
  binder_status_t r = AParcel_readStatusHeader(out, &s);
  bool ok = (r == STATUS_OK) && s && AStatus_isOk(s);
  if (s) AStatus_delete(s);
  if (ok && rd) rd(out);
  AParcel_delete(out);
  return ok ? STATUS_OK : STATUS_FAILED_TRANSACTION;
}
