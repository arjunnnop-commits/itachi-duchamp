# ITACHI for POCO X6 Pro (duchamp) — Installation
Author / License: **@yooitachiijod** (all rights reserved, see LICENSE). Written from scratch.

> **License lock:** HAL tabhi chalta hai jab (1) `/vendor/etc/itachi/license.lic` tere private key se sign hui ho,
> (2) `system/build.prop` mein `ro.itachi.author=@yooitachiijod` licence ke naam se match kare, (3) device match kare,
> (4) expiry na ho. Warna HAL idle rehta hai aur logcat mein `NOT AUTHORIZED (...)` + wajah likhta hai.

## 0. License banana (ek baar, PC ya Termux, sirf python3 chahiye)
```
python3 tools/itachi_license.py keygen        # itachi_private.key (SECRET, offline backup!) + itachi_public.key
python3 tools/itachi_license.py sign --name @yooitachiijod --device duchamp      # license.lic
python3 tools/itachi_license.py seal          # hal/itachi_sealed.h  (public key + sealed constants)
python3 tools/itachi_license.py verify        # VALID
```
- Serial se bind karna ho: `sign ... --serial <phone serial>`; expiry ke liye `--expires 2027-12-31`.
- `itachi_private.key` kabhi kisi ko mat dena, repo mein mat daalna. Kho gaya to nayi license nahi ban sakti (dobara keygen + seal + rebuild).
- Binary sirf usi `license.lic` ke saath chalega jis se seal hua. License badli to `seal` + rebuild.
- Self-test: `bash tests/run.sh` (genuine, galat naam, galat device, tamper, expired, forged, doosri key, serial bind).

---
## 1. PROPS — copy/paste

### `system/build.prop` (OxygenOS16 side) — ye block end mein add kar
```
# ===== ITACHI duchamp =====
ro.itachi.author=@yooitachiijod
persist.vendor.fingerprint.optical.iconsize=210
persist.vendor.fingerprint.optical.iconlocation=170
```
`iconlocation` panel ke bottom se doori hai. Lockscreen pe icon sensor ke upar sahi na baithe to upar-neeche karke dekh.

### `vendor/build.prop` — **kuch add nahi karna**
Xiaomi vendor mein FOD ke props pehle se hain (`ro.hardware.fp.fod=true`, `persist.vendor.sys.fp.fod.location.X_Y=504,2332`,
`persist.vendor.sys.fp.fod.size.width_height=210,210`, `ro.vendor.localhbm.enable=true`, `ro.hardware.fp.fod.touch.ctl.version=2.0`).
```
# (vendor/build.prop me koi change nahi)
```

### Ye pehle se tere OOS16 system tree mein hain — dubara mat daalna
```
persist.vendor.fingerprint.sensor_type=optical
vendor.fingerprint.aidl.support=1
ro.oplus.biometrics.fingerprint.optical=true
ro.oplus.aod.fod.support=true
persist.sys.duchamp.aod_keep_doze=1
ro.itachi.fod.compat=1
```

---
## 2. Step 0 — ROM check (verified)
OOS16 `services.jar` dono HALs call karta hai: `IDisplayPanelFeature/default` (v1) aur `IOplusTouch/default` (v2).
HiOS ka framework nahi, isliye poora test OOS16 port pe karna.

## 3. Build (license step 0 ke baad)
- PC: `NDK=/path/to/ndk ./build_all.sh` -> `out/itachi-hal` + `out/license.lic`
- PC nahi: repo GitHub pe daal (`hal/itachi_sealed.h` aur `license.lic` commit kar, private key NAHI), Actions `build-itachi-hal` chala, artifact download kar.

## 4. Live test (ROM chhede bina)
1. `itachi-hal`, `license.lic`, `itachi.conf`, `tools/bringup.sh` rakh `/sdcard/Download` mein (license.lic ko `/vendor/etc/itachi/` mein bhi chahiye, ya bringup se pehle wahan copy kar).
2. Gate ke liye prop runtime pe set kar (Magisk/KernelSU): `su -c "resetprop ro.itachi.author @yooitachiijod"`.
   (resetprop na ho to pehle `system/build.prop` mein prop daal ke reboot kar.)
3. `su -c "sh /sdcard/Download/bringup.sh"` -> DT2W/AOD/FOD try kar -> `itachi_report.txt` bhej.

## 5. ROM mein bake
1. Backup le.
2. `./tools/bake_into_vendor.sh /data/DNA/DNA_pocovendor/vendor /data/DNA/DNA_oxygenos16/system/system` (ya haath se):
   - `itachi-hal` -> `vendor/bin/hw/` (0755 root)
   - `itachi-hal.rc` -> `vendor/etc/init/`
   - `itachi-hal.xml` -> `vendor/etc/vintf/manifest/`
   - `itachi.conf` -> `vendor/etc/itachi/`
   - `license.lic` -> `vendor/etc/itachi/` (0644)
3. SELinux: `sepolicy/itachi.cil` -> `vendor/etc/selinux/vendor_sepolicy.cil` ke end mein;
   `vendor_file_contexts.append` aur `vendor_service_contexts.append` apni files ke end mein.
   `vendor/etc/selinux/precompiled_sepolicy` + `.sha256` delete kar, warna cil apply nahi hota.
4. `system/build.prop` mein section 1 ka block jod. `fs_config`/`file_contexts` mein `itachi-hal` entry daal, repack.

## 6. Boot ke baad
- `logcat -s itachi-hal` -> `licensed to @yooitachiijod`, `starting`, addService dono HAL ke liye. `NOT AUTHORIZED (code): wajah` dikhe to wajah wahi hai (license.lic missing / prop mismatch / device / expiry).
- Denials: `logcat -d | grep avc: | python3 tools/avc2cil.py` -> output `itachi.cil` mein jod, repack, repeat. Phir Enforcing.

## 7. Check
DT2W toggle · AOD 3 modes · lockscreen pe dimming/black box nahi · FOD unlock + unlock ke baad spot band · SOFOD: screen off pe touch -> ~7 sec icon.
Na chale to `itachi.conf` mein `fod_arm=1`, `touch_name`, `screen_off_bl_max`, `hint_ms` tune kar.

## Verified facts
ITouchFeature setModeValue txn 9, mode 10 (mfp-daemon) · IOplusTouch v2 / DPF v1 hashes match OOS16 · feature IDs 7, 22, 28, 211 (0x410), 217, 20004 confirmed in services/SystemUI.
