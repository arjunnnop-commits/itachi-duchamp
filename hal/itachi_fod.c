/*
 * ITACHI duchamp FOD/SOFOD helper - written from scratch.  (c) @yooitachiijod
 *
 * 1) FOD arm/disarm : ColorOS tells the HAL when the finger layer is shown/hidden (feature 20004/28)
 *    and when HBM must drop (feature 22 = 0). We mirror that onto the Xiaomi touch IC's fod_enable
 *    node so the touch IC only reports FOD presses while ColorOS wants them, and stops right
 *    after unlock (instant LHBM-off, no lit spot while the thumb rests).
 * 2) SOFOD hint     : when the screen is off and the touch IC sees a touch, light the panel for a few
 *    seconds (via doze_brightness) so ColorOS' fingerprint icon is actually visible, then put the
 *    panel back to whatever AOD mode ColorOS last asked for.
 * Every node/value is in itachi.conf so it can be tuned from device logs without rebuilding.
 */
#define _GNU_SOURCE
#include "itachi_fod.h"
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static struct {
    int  udfps_type;
    int  layer_show_val;                /* raw value meaning "show" (1 normal, 0 for MTK-inverted) */
    char fod_nodes[512];                /* comma separated candidates */
    char backlight_node[160];
    char touch_name[64];                /* substring of input device name */
    char touch_path[96];                /* explicit /dev/input/eventN, overrides name */
    int  screen_off_bl_max;
    int  hint_ms;
    int  hint_doze;
    int  hint_enable;
    int  fod_arm;                       /* 1 = ITACHI arms the touch IC itself */
    char touch_service[96];
    int  touch_txn_set;                 /* binder txn code of ITouchFeature.setModeValue (0 = unknown) */
    int  touch_dev_id, touch_mode_fod, touch_arm_val;
} c = {
    .udfps_type = 0x410, .layer_show_val = 1,
    .fod_nodes = "",                    /* duchamp has no fod_enable sysfs node; arming goes through ITouchFeature */
    .backlight_node = "/sys/class/mi_display/disp-DSI-0/backlight",
    .touch_name = "touch", .touch_path = "",
    .screen_off_bl_max = 0, .hint_ms = 7000, .hint_doze = 1, .hint_enable = 1,
    .fod_arm = 0, .touch_service = "vendor.xiaomi.hw.touchfeature.ITouchFeature/default",
    .touch_txn_set = 0, .touch_dev_id = 0, .touch_mode_fod = 10, .touch_arm_val = 1,
};

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static char fod_node[200];
static int  armed = -1;
static volatile int hint_active = 0;
static long long hint_until_ms = 0;

static long long now_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static int wr(const char *p, const char *v) {
    int fd = open(p, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -errno;
    ssize_t n = write(fd, v, strlen(v)); int e = errno; close(fd);
    return n < 0 ? -e : 0;
}
static int rd_int(const char *p, int def) {
    char b[48]; int fd = open(p, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return def;
    ssize_t n = read(fd, b, sizeof b - 1); close(fd);
    if (n <= 0) return def;
    b[n] = 0; return atoi(b);
}

static void pick_fod_node(void) {
    if (fod_node[0] && access(fod_node, W_OK) == 0) return;
    char tmp[512]; snprintf(tmp, sizeof tmp, "%s", c.fod_nodes);
    for (char *s = tmp, *tok; (tok = strsep(&s, ",")); )
        if (*tok && access(tok, F_OK) == 0) { snprintf(fod_node, sizeof fod_node, "%s", tok); itachi_trace("fod node = %s", fod_node); return; }
    fod_node[0] = 0;
}

/* ITouchFeature.setModeValue(int dev, int mode, int value) -> int  (raw NDK binder call, code from conf) */
static AIBinder *tf;
static int touch_set_mode(int dev, int mode, int val, int *ret) {
    if (c.touch_txn_set <= 0) return -ENOSYS;
    if (!tf) {
        tf = AServiceManager_checkService(c.touch_service);
        if (!tf) { itachi_trace("touchfeature service %s not found", c.touch_service); return -ENODEV; }
    }
    AParcel *in = NULL, *out = NULL;
    if (AIBinder_prepareTransaction(tf, &in) != STATUS_OK) return -EIO;
    AParcel_writeInt32(in, dev); AParcel_writeInt32(in, mode); AParcel_writeInt32(in, val);
    binder_status_t st = AIBinder_transact(tf, (transaction_code_t)c.touch_txn_set, &in, &out, 0);
    if (st != STATUS_OK) { AIBinder_decStrong(tf); tf = NULL; return -EIO; }   /* HAL died: re-acquire next time */
    AStatus *as = NULL; int r = -1;
    if (AParcel_readStatusHeader(out, &as) == STATUS_OK && as && AStatus_isOk(as)) AParcel_readInt32(out, &r);
    if (as) AStatus_delete(as);
    AParcel_delete(out);
    if (ret) *ret = r;
    return 0;
}

static void arm(int on) {
    if (!c.fod_arm) return;                  /* disabled until logs show mfp-daemon does not arm by itself */
    pthread_mutex_lock(&lk);
    if (armed != on) {
        int ret = -99, r = touch_set_mode(c.touch_dev_id, c.touch_mode_fod, on ? c.touch_arm_val : 0, &ret);
        if (r == -ENOSYS) { pick_fod_node(); r = fod_node[0] ? wr(fod_node, on ? "1" : "0") : -ENOENT; }
        itachi_trace("FOD arm(%d) mode=%d -> r=%d ret=%d", on, c.touch_mode_fod, r, ret);
        if (r == 0) armed = on;
    }
    pthread_mutex_unlock(&lk);
}

int  itachi_udfps_type(void) { return c.udfps_type; }
void itachi_fod_layer(int raw) { arm(raw == c.layer_show_val); }
void itachi_fod_hbm_off(void)  { arm(0); }
void itachi_fod_aod_changed(void) {
    /* a new AOD request while a hint is running must not be undone by the hint's restore step */
    pthread_mutex_lock(&lk); if (hint_active) hint_until_ms = now_ms(); pthread_mutex_unlock(&lk);
}

static int screen_off(void) { return rd_int(c.backlight_node, 1) <= c.screen_off_bl_max; }

/* locate the touch input node from /proc/bus/input/devices */
static int find_touch(char *out, size_t sz) {
    if (c.touch_path[0]) { snprintf(out, sz, "%s", c.touch_path); return 0; }
    FILE *f = fopen("/proc/bus/input/devices", "re"); if (!f) return -1;
    char line[256]; int match = 0, found = -1;
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "N: Name=", 8)) match = strcasestr(line, c.touch_name) != NULL;
        else if (match && !strncmp(line, "H: Handlers=", 12)) {
            char *e = strstr(line, "event");
            if (e) { snprintf(out, sz, "/dev/input/event%d", atoi(e + 5)); found = 0; break; }
        }
    }
    fclose(f); return found;
}

static void *hint_thread(void *a) {
    (void)a;
    for (;;) {
        char path[96];
        if (find_touch(path, sizeof path) != 0) { sleep(5); continue; }
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) { itachi_trace("hint: open %s failed: %s", path, strerror(errno)); sleep(5); continue; }
        itachi_trace("hint: watching %s", path);
        for (;;) {
            struct pollfd p = { fd, POLLIN, 0 };
            int r = poll(&p, 1, 250);
            if (r < 0 && errno != EINTR) break;
            long long t = now_ms();
            if (r > 0 && (p.revents & POLLIN)) {
                struct input_event ev[32];
                ssize_t n = read(fd, ev, sizeof ev);
                if (n < 0 && errno != EAGAIN) break;
                if (n > 0 && screen_off()) {
                    pthread_mutex_lock(&lk);
                    if (!hint_active) { hint_active = 1; itachi_write_doze(c.hint_doze); itachi_trace("hint: ON"); }
                    hint_until_ms = t + c.hint_ms;
                    pthread_mutex_unlock(&lk);
                }
            }
            pthread_mutex_lock(&lk);
            if (hint_active && t >= hint_until_ms) {
                hint_active = 0; itachi_write_doze(itachi_saved_doze()); itachi_trace("hint: OFF");
            }
            pthread_mutex_unlock(&lk);
        }
        close(fd);
    }
    return NULL;
}

static void conf_line(char *k, char *v) {
    if (!strcmp(k, "udfps_type")) c.udfps_type = (int)strtol(v, NULL, 0);
    else if (!strcmp(k, "layer_show_val")) c.layer_show_val = atoi(v);
    else if (!strcmp(k, "fod_nodes")) snprintf(c.fod_nodes, sizeof c.fod_nodes, "%s", v);
    else if (!strcmp(k, "backlight_node")) snprintf(c.backlight_node, sizeof c.backlight_node, "%s", v);
    else if (!strcmp(k, "touch_name")) snprintf(c.touch_name, sizeof c.touch_name, "%s", v);
    else if (!strcmp(k, "touch_path")) snprintf(c.touch_path, sizeof c.touch_path, "%s", v);
    else if (!strcmp(k, "screen_off_bl_max")) c.screen_off_bl_max = atoi(v);
    else if (!strcmp(k, "hint_ms")) c.hint_ms = atoi(v);
    else if (!strcmp(k, "hint_doze")) c.hint_doze = atoi(v);
    else if (!strcmp(k, "hint_enable")) c.hint_enable = atoi(v);
    else if (!strcmp(k, "fod_arm")) c.fod_arm = atoi(v);
    else if (!strcmp(k, "touch_service")) snprintf(c.touch_service, sizeof c.touch_service, "%s", v);
    else if (!strcmp(k, "touch_txn_set")) c.touch_txn_set = atoi(v);
    else if (!strcmp(k, "touch_dev_id")) c.touch_dev_id = atoi(v);
    else if (!strcmp(k, "touch_mode_fod")) c.touch_mode_fod = atoi(v);
    else if (!strcmp(k, "touch_arm_val")) c.touch_arm_val = atoi(v);
}

void itachi_fod_init(const char *conf_path) {
    FILE *f = fopen(conf_path, "re");
    if (f) {
        char line[512];
        while (fgets(line, sizeof line, f)) {
            char *eq = strchr(line, '='); if (!eq || line[0] == '#') continue;
            *eq = 0; eq[1 + strcspn(eq + 1, "\r\n")] = 0; conf_line(line, eq + 1);
        }
        fclose(f);
    }
    itachi_trace("fod init: udfps_type=0x%x hint=%d/%dms", c.udfps_type, c.hint_enable, c.hint_ms);
    if (c.hint_enable) { pthread_t t; pthread_create(&t, NULL, hint_thread, NULL); pthread_detach(t); }
}
