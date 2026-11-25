#include "panel_service.h"

#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>

#include <cstring>
#include <string>
#include <vector>

#include "backend.h"
#include "log.h"

namespace {

constexpr const char* kDescriptor = "vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature";
constexpr const char* kInstance = "vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default";
// Verified from the ColorOS framework's generated Stub.
constexpr transaction_code_t kGetValue = 1;      // int getDisplayPanelFeatureValue(int id, out int[] v)
constexpr transaction_code_t kSetValue = 2;      // int setDisplayPanelFeatureValue(int id, in int[] v)
constexpr transaction_code_t kGetInfo = 3;       // int getDisplayPanelInfo(int id, out List<String> info)
constexpr transaction_code_t kGetHash = 16777214;
constexpr transaction_code_t kGetVersion = 16777215;
constexpr int32_t kVersion = 1;
constexpr const char* kHash = "39afca22ac77253421ad681eeb2bae0c6ff62c13";

void writeOk(AParcel* out) {
  AStatus* s = AStatus_newOk();
  AParcel_writeStatusHeader(out, s);
  AStatus_delete(s);
}

bool i32Alloc(void* arrayData, int32_t length, int32_t** outBuffer) {
  auto* v = static_cast<std::vector<int32_t>*>(arrayData);
  if (length < 0) {
    v->clear();
    *outBuffer = nullptr;
    return true;
  }
  v->assign(static_cast<size_t>(length), 0);
  *outBuffer = v->data();
  return true;
}

const char* strGetter(const void* arrayData, size_t index, int32_t* outLength) {
  auto* v = static_cast<const std::vector<std::string>*>(arrayData);
  *outLength = static_cast<int32_t>((*v)[index].size());
  return (*v)[index].c_str();
}

binder_status_t handleGet(const AParcel* in, AParcel* out) {
  int32_t id = 0, len = 0;
  AParcel_readInt32(in, &id);
  AParcel_readInt32(in, &len);  // size of the caller's out array (-1 = null)
  std::vector<int32_t> vals(len > 0 ? static_cast<size_t>(len) : 0, 0);
  int32_t ret = Backend::get(id, &vals);
  vals.resize(len > 0 ? static_cast<size_t>(len) : 0, 0);  // must match the caller's length exactly
  writeOk(out);
  AParcel_writeInt32(out, ret);
  return AParcel_writeInt32Array(out, len < 0 ? nullptr : vals.data(), len);
}

binder_status_t handleSet(const AParcel* in, AParcel* out) {
  int32_t id = 0;
  std::vector<int32_t> vals;
  AParcel_readInt32(in, &id);
  AParcel_readInt32Array(in, &vals, i32Alloc);
  int32_t ret = Backend::set(id, vals);
  writeOk(out);
  return AParcel_writeInt32(out, ret);
}

binder_status_t handleInfo(const AParcel* in, AParcel* out) {
  int32_t id = 0;
  AParcel_readInt32(in, &id);
  std::vector<std::string> info;
  int32_t ret = Backend::info(id, &info);
  writeOk(out);
  AParcel_writeInt32(out, ret);
  return AParcel_writeStringArray(out, &info, static_cast<int32_t>(info.size()), strGetter);
}

void* onCreate(void* args) { return args; }
void onDestroy(void*) {}

binder_status_t onTransact(AIBinder*, transaction_code_t code, const AParcel* in, AParcel* out) {
  switch (code) {
    case kGetValue: return handleGet(in, out);
    case kSetValue: return handleSet(in, out);
    case kGetInfo: return handleInfo(in, out);
    case kGetVersion:
      writeOk(out);
      return AParcel_writeInt32(out, kVersion);
    case kGetHash:
      writeOk(out);
      return AParcel_writeString(out, kHash, static_cast<int32_t>(strlen(kHash)));
    default:
      LOGW("unknown transaction %u", static_cast<unsigned>(code));
      return STATUS_UNKNOWN_TRANSACTION;
  }
}

}  // namespace

bool registerPanelService() {
  AIBinder_Class* cls = AIBinder_Class_define(kDescriptor, onCreate, onDestroy, onTransact);
  if (!cls) {
    LOGE("AIBinder_Class_define failed");
    return false;
  }
  AIBinder* b = AIBinder_new(cls, nullptr);
  binder_exception_t ex = AServiceManager_addService(b, kInstance);
  if (ex != EX_NONE) {
    LOGE("addService(%s) failed: %d", kInstance, static_cast<int>(ex));
    return false;
  }
  LOGI("registered %s", kInstance);
  return true;
}
