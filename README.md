# ITACHI — FOD / SOFOD / AOD / DT2W support for POCO X6 Pro (duchamp, MT6897) on ColorOS ports
Author & license: @yooitachiijod. All code written from scratch.

## Parts
| Part | File | Status |
|---|---|---|
| IOplusTouch HAL (DT2W) | hal/itachi_hal.c | phase 1, needs device test |
| IDisplayPanelFeature HAL: AOD map, **211 (udfps/local-HBM type)**, **217 (smooth AOD)**, **22 (HBM off)**, 28/20004 (finger layer) | hal/itachi_hal.c | phase 2, new |
| FOD touch arm/disarm, instant disarm after unlock | hal/itachi_fod.c | phase 2, new |
| SOFOD hint: screen-off touch -> panel lit N sec -> back to AOD mode | hal/itachi_fod.c | phase 2, new |
| Proximity fix (smali patcher, scan mode first) | tools/patch_proximity.py | phase 2, new |
| SELinux starter + avc->cil converter | sepolicy/, tools/avc2cil.py | phase 2, iterate with logs |
| Sensor/device probe | tools/probe_sensors.sh | run once, send back report |
| props / rc node permissions | rootfs/ | phase 2 |

## Build
    NDK=/path/to/ndk ./build_all.sh
    ./tools/bake_into_vendor.sh <vendor dir> <system dir>
## Test order
1. tools/bringup.sh (permissive) -> send itachi_report.txt
2. tools/probe_sensors.sh -> send itachi_sensors.txt
3. Tune itachi.conf (fod_nodes, touch_name, screen_off_bl_max, hint_*) from `logcat -s itachi-hal`.
4. Bake, enable seclabel, loop avc2cil until clean.

## Not done yet (needs your device data)
- Rise-to-wake / sensor HAL wrapper (needs probe_sensors output + vendor sensors lib)
- Auth-result LHBM-off inside mfp-daemon (only touch-side disarm exists; confirm on logs if enough)
- Tap-to-AOD framework bridge

## Findings from the duchamp vendor report (2026-11-26)
- Fingerprint: Xiaomi `mfp-daemon` (AIDL IFingerprint v3, Goodix FOD). It has its own FodEngine and drives
  `/dev/mi_display/disp_feature` + `ITouchFeature`; it reads `/sys/class/touch/touch_dev/fod_press_status`.
- There is **no `fod_enable` sysfs node**. FOD touch arming is ITouchFeature.setModeValue(dev, mode, value),
  so `fod_arm` is OFF by default and the binder txn code is read from conf (`touch_txn_set`).
- AOD node confirmed: `/sys/class/mi_display/disp-DSI-0/doze_brightness` (chown'd by vendor init.project.rc).
- Sensors HAL = `sensors.mediatek.V2.0.so` (mt6897 symlink). It already exposes `Fod Wakeup`, `Aod Wakeup`,
  `pickup Wakeup`, `tilt` and `android.sensor.tilt_detector`, plus Elliptic Proximity/Doubletap/Wake Gesture.
  So a full sensor wrapper may be unnecessary: probe_sensors.sh output decides.

## Verified from the vendor binaries (disassembled, 2026-11-26)
- ITouchFeature binder codes: getModeCurValue=1, setModeValue=9, getModeCurValueString=10.
  setModeValue wire = int dev, int mode, int value -> reply status + int32 result.
- mfp-daemon's TouchFeatureCtl calls setModeValue(dev=0, mode=10, value=<state 1..4 from FodEngine>).
  So the FOD touch mode is 10 and ITACHI's `touch_mode_fod=10`, `touch_txn_set=9` are now correct.
- Values 1..4 are FodEngine states; ITACHI arms with 1 and disarms with 0. `fod_arm` stays 0 until logs say otherwise.

## Verified against OxygenOS 16 (services.jar / oplus-services.jar / oplus-framework.jar, 2026-11-26)
- Framework talks to BOTH HALs: AIDL `vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default` (v1, hash 39afca22...)
  and `vendor.oplus.hardware.touch.IOplusTouch/default` (v2, hash ef41c5fa...), HIDL @1.0 only as fallback.
  Transaction numbers + versions + hashes in itachi_hal.c match these classes exactly.
- Feature IDs (services): AOD=7 (value 0/1 = doze brightness), MAX_BRIGHTNESS=8, PANEL_INFO=9, HBM=12, DIMLAY_HBM=22,
  PANEL_BLANK=24, FPPRESS=28, NORMAL_MAXBRIGHTNESS=247, NOTIFY_DISP_FINGER_LAYER=20004.
- SystemUI (verified): feature 211 = UDFPS type, read raw by KeyguardFeatureOption.getUdfpsType(); -1 means unsupported,
  bit 0x10 -> local-HBM flag, bit 0x10 AND 0x400 -> second flag. ITACHI answers 0x410 (both bits set).
  Feature 217 = smooth-AOD capability (read by SmoothTransitionController as int[]), 213 = setFakeAodState,
  195 = enableLTPOAod, 69 = is8BitScreen. Feature 22 (OnScreenHighLightControl.hbmControl) = HBM on(1)/off(0), matches fod disarm on 0.
- IOplusTouch is also used for node I/O: touchWriteNodeFile(node, dev, str) / touchReadNodeFile(node, dev).

## License lock (phase 2b)
Ed25519-signed license.lic + name in system/build.prop + device/serial/expiry checks, and the HAL's service names / interface
hashes are sealed with a key derived from the license signature. See INSTALLATION.md section 0 and tests/run.sh.
