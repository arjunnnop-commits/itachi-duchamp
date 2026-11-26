/*
 * ITACHI license verification - (c) @yooitachiijod
 * license.lic = text fields + "sig=<hex>" ; signature = Ed25519 over the text, made with the author's private key.
 * Checks: signature | name == ro.itachi.author (system/build.prop) | device | optional serial binding | expiry.
 * The signature bytes also derive the key that unseals the HAL's service names / interface hashes, so
 * a binary patched to skip the check still cannot register working services.
 */
#include "itachi_license.h"
#include "itachi_sealed.h"
#include "tweetnacl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/system_properties.h>

#ifndef LICENSE_PATH
#define LICENSE_PATH "/vendor/etc/itachi/license.lic"
#endif

void randombytes(unsigned char *x, unsigned long long n) { memset(x, 0, (size_t)n); } /* tweetnacl needs it; never used for verify */

static unsigned char g_sig[64];
static int g_ok;

static void sha512(unsigned char *out, const unsigned char *in, unsigned long long n) { crypto_hash(out, in, n); }

static int unhex(const char *s, unsigned char *o, int n) {
    for (int i = 0; i < n; i++) {
        unsigned v = 0;
        for (int k = 0; k < 2; k++) {
            char c = s[2 * i + k]; v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return -1;
        }
        o[i] = (unsigned char)v;
    }
    return 0;
}
static void prop(const char *k, char *v) { v[0] = 0; __system_property_get(k, v); }

static int field(const char *body, const char *key, char *out, size_t n) {
    size_t kl = strlen(key); const char *p = body;
    while (*p) {
        if (!strncmp(p, key, kl) && p[kl] == '=') {
            p += kl + 1; size_t i = 0;
            while (*p && *p != '\n' && i + 1 < n) out[i++] = *p++;
            out[i] = 0; return 0;
        }
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
    return -1;
}

int itachi_license_check(char *why, size_t wn) {
#define FAIL(code, msg) do { snprintf(why, wn, "%s", msg); g_ok = 0; return (code); } while (0)
    FILE *f = fopen(LICENSE_PATH, "re");
    if (!f) FAIL(-1, "license.lic missing");
    char *buf = malloc(4096); if (!buf) { fclose(f); FAIL(-2, "oom"); }
    size_t len = fread(buf, 1, 4095, f); fclose(f); buf[len] = 0;
    char *sp = NULL;
    for (char *p = buf; (p = strstr(p, "sig=")); p++) if (p == buf || p[-1] == '\n') sp = p;
    if (!sp) { free(buf); FAIL(-3, "license has no signature"); }
    size_t blen = (size_t)(sp - buf);
    unsigned char sig[64];
    if (strlen(sp) < 4 + 128 || unhex(sp + 4, sig, 64) != 0) { free(buf); FAIL(-3, "bad signature encoding"); }
    unsigned char *sm = malloc(64 + blen), *m = malloc(64 + blen);
    if (!sm || !m) { free(buf); free(sm); free(m); FAIL(-2, "oom"); }
    memcpy(sm, sig, 64); memcpy(sm + 64, buf, blen);
    unsigned long long mlen = 0;
    int vr = crypto_sign_open(m, &mlen, sm, 64 + blen, ITACHI_PUBKEY);
    free(sm); free(m);
    if (vr != 0) { free(buf); FAIL(-4, "signature INVALID (license not issued by author)"); }
    buf[blen] = 0;
    char name[96], dev[64], id[40], exp[24], pv[PROP_VALUE_MAX];
    if (field(buf, "name", name, sizeof name) || field(buf, "device", dev, sizeof dev) ||
        field(buf, "id", id, sizeof id) || field(buf, "expires", exp, sizeof exp)) { free(buf); FAIL(-5, "license fields missing"); }
    free(buf);
    prop("ro.itachi.author", pv);
    if (strcmp(pv, name) != 0) FAIL(-6, "ro.itachi.author in system/build.prop does not match licensed name");
    if (strcmp(dev, "*") != 0) {
        char d1[PROP_VALUE_MAX], d2[PROP_VALUE_MAX];
        prop("ro.product.vendor.device", d1); prop("ro.product.device", d2);
        if (strcmp(dev, d1) != 0 && strcmp(dev, d2) != 0) FAIL(-7, "license is for another device");
    }
    if (strcmp(id, "*") != 0) {
        char sn[PROP_VALUE_MAX]; prop("ro.boot.serialno", sn); if (!sn[0]) prop("ro.serialno", sn);
        unsigned char h[64]; char hex[17];
        sha512(h, (const unsigned char *)sn, (unsigned long long)strlen(sn));
        for (int i = 0; i < 8; i++) snprintf(hex + 2 * i, 3, "%02x", h[i]);
        if (strcmp(hex, id) != 0) FAIL(-8, "license is bound to another device serial");
    }
    long long e = atoll(exp);
    if (e > 0 && (long long)time(NULL) > e) FAIL(-9, "license expired");
    memcpy(g_sig, sig, 64); g_ok = 1;
    snprintf(why, wn, "licensed to %s", name);
    return 0;
}

static const unsigned char *const BLOB[4] = { SEALED_TOUCH_DESC, SEALED_DPF_DESC, SEALED_TOUCH_HASH, SEALED_DPF_HASH };
static const unsigned BLEN[4] = { sizeof SEALED_TOUCH_DESC, sizeof SEALED_DPF_DESC, sizeof SEALED_TOUCH_HASH, sizeof SEALED_DPF_HASH };
static const unsigned char BTAG[4] = { SEALED_TOUCH_DESC_TAG, SEALED_DPF_DESC_TAG, SEALED_TOUCH_HASH_TAG, SEALED_DPF_HASH_TAG };

const char *itachi_sealed(int which) {
    static char out[4][96]; static int done[4];
    if (which < 0 || which > 3) return "";
    if (done[which]) return out[which];
    unsigned char in[64 + 9], key[64], blk[66], h[64];
    memcpy(in, g_sig, 64); memcpy(in + 64, "itachi-v1", 9);
    sha512(key, in, sizeof in);                         /* key = SHA512(sig || "itachi-v1") */
    unsigned n = BLEN[which];
    for (unsigned i = 0; i < n; i++) {
        if (i % 64 == 0) { memcpy(blk, key, 64); blk[64] = BTAG[which]; blk[65] = (unsigned char)(i / 64); sha512(h, blk, 66); }
        out[which][i] = (char)(BLOB[which][i] ^ h[i % 64]);
    }
    out[which][n] = 0; done[which] = 1;
    return out[which];
}
