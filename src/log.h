#pragma once
#include <android/log.h>
#define ITACHI_TAG "itachi"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, ITACHI_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, ITACHI_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, ITACHI_TAG, __VA_ARGS__)
