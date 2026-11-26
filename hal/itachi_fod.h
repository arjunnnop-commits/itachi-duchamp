/* ITACHI for POCO X6 Pro (duchamp) - FOD / SOFOD helper.  (c) @yooitachiijod */
#ifndef ITACHI_FOD_H
#define ITACHI_FOD_H
void itachi_fod_init(const char *conf_path);
int  itachi_udfps_type(void);            /* value reported for ColorOS feature 211 */
void itachi_fod_layer(int raw);          /* feature 20004 / 28: finger layer show/hide */
void itachi_fod_hbm_off(void);           /* feature 22 value 0: drop FOD touch arm at once */
void itachi_fod_aod_changed(void);       /* ColorOS changed AOD mode */
/* provided by itachi_hal.c */
int  itachi_write_doze(int v);
int  itachi_saved_doze(void);
void itachi_trace(const char *fmt, ...);
#endif
