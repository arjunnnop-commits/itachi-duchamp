/* ITACHI license verification. (c) @yooitachiijod */
#ifndef ITACHI_LICENSE_H
#define ITACHI_LICENSE_H
#include <stddef.h>
enum { S_TOUCH_DESC = 0, S_DPF_DESC = 1, S_TOUCH_HASH = 2, S_DPF_HASH = 3 };
/* 0 = authorized. Otherwise negative; `why` gets a short reason for logcat. */
int itachi_license_check(char *why, size_t n);
/* Sealed constants (service names, interface hashes). Only decode correctly with the genuine license. */
const char *itachi_sealed(int which);
#endif
