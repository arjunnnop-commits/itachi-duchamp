/* host test for itachi_license.c (mock props). Build: see tests/run.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../hal/itachi_license.h"
int __system_property_get(const char *k, char *v) { const char *e = getenv(k[0]=='r'?(!strcmp(k,"ro.itachi.author")?"P_AUTHOR":!strcmp(k,"ro.product.vendor.device")?"P_VDEV":!strcmp(k,"ro.product.device")?"P_DEV":!strcmp(k,"ro.boot.serialno")?"P_SN":"P_X"):"P_X"); if (!e) { v[0]=0; return 0; } strcpy(v, e); return (int)strlen(v); }
int main(void) {
    char why[128]; int r = itachi_license_check(why, sizeof why);
    printf("check=%d (%s)\n", r, why);
    if (r == 0) printf("S0=%s\nS1=%s\nS2=%s\nS3=%s\n", itachi_sealed(0), itachi_sealed(1), itachi_sealed(2), itachi_sealed(3));
    else printf("S0=%.12s\n", itachi_sealed(0));
    return r != 0;
}
