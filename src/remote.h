#pragma once
#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>

#include <functional>
#include <mutex>
#include <string>

// Minimal hand-written AIDL client: lets us call a vendor AIDL service by
// transaction code without needing its generated headers.
class RemoteService {
 public:
  RemoteService(std::string descriptor, std::string instance);
  using Writer = std::function<void(AParcel*)>;
  using Reader = std::function<void(const AParcel*)>;
  // Returns STATUS_OK on success.
  binder_status_t transact(transaction_code_t code, const Writer& wr, const Reader& rd);

 private:
  bool ensure();
  std::string desc_, inst_;
  AIBinder_Class* cls_ = nullptr;
  AIBinder* bin_ = nullptr;
  std::mutex mtx_;
};
