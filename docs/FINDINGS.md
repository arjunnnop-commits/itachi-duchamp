# Verified findings (POCO X6 Pro / MT6897, ColorOS 16 target)

Everything here was read from the user's own ROM files (framework jars, vendor/odm), not guessed,
unless marked **unverified**.

## What the ColorOS framework needs
Service `vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default`, AIDL v1,
hash `39afca22ac77253421ad681eeb2bae0c6ff62c13`.

| code | method |
|---|---|
| 1 | `int getDisplayPanelFeatureValue(int id, out int[] v)` (caller sends array length; reply array must match it) |
| 2 | `int setDisplayPanelFeatureValue(int id, in int[] v)` |
| 3 | `int getDisplayPanelInfo(int id, out List<String>)` |
| 16777215 / 16777214 | interface version / hash |

Feature IDs: HBM 12, DIM_ALPHA 13, DIMLAY_HBM 22, PANEL_BLANK 24, **FPPRESS 28**, AOD 7, FFL 6,
MAX_BRIGHTNESS 8, PANEL_INFO 9, **capabilities 211**, PULSE_STATE 264.

Framework logic for FPPRESS (28): if capability 211 has bit 4 (HW dimlayer) it calls our `set(28,v)` only;
if bit 1024 it calls `set(28,v)` and also notifies SurfaceFlinger (MTK path, transaction 20004,
value inverted: press=0, release=1); with neither bit it only does the SurfaceFlinger part.

Fingerprint press itself reaches the HAL through standard AIDL `ISession.onPointerDown/Up`.
Oplus additionally sends a raw transaction (code 1019, cmd 1008) to the fingerprint binder;
`mfp-daemon` will not understand it (**unverified** whether that matters).

## What the Xiaomi stack provides
* `mfp-daemon` (Goodix optical FOD) -> `fingerprint.goodix_fod.so`, AIDL IFingerprint V3.
* `IXiaomiFingerprint/default`, method `extCmd(int cmd, int param) -> int` (transaction 1).
  HiOS sequence per unlock:
  `2=0,3=0,1=1` (down) -> `2=x,3=y,10=1` (HBM + green spot ready) -> scan -> `2=x,3=y,10=0`
  -> `2=0,3=0,1=0` (up). x,y ~ 1.6 x display pixels.
* `mfp-daemon` has its own `setLocalHBM` path (`/dev/mi_display/disp_feature`) and the property
  `persist.vendor.sys.fp.fod.lhbmoffafterresult`; it logs "skip enable lhbm as fingerprint is idle".
  So LHBM is probably lit by the HAL itself during an auth session (**unverified** for ColorOS).
* `IDisplayFeature/default` method order (from the proxy vtable, **moderately verified**):
  notifyBrightness 1, registerCallback 2, sendMessage 3, sendPanelCommand 4,
  sendPostProcCommand 5, sendRefreshCommand 6, setFeature 7, setFunction 8.
* Doze/AOD nodes: `/sys/class/mi_display/disp_feature/disp-DSI-0/doze_brightness`,
  `/sys/class/drm/card0-DSI-1/doze_brightness`, touch `.../goodix_ts.0/aod_enable`.

## Still unknown (needs on-device testing)
1. Whether ColorOS ever sends set(28,..) with `cap211=4` and when relative to the auth start.
2. The right `doze_brightness` values for AOD on/off/low.
3. Whether LHBM needs `lhbm_mode` (setFeature) or is handled by mfp-daemon.
4. SELinux denials.


## SystemUI findings (read from the ROM's SystemUI.apk)
* `getUdfpsType()` = AIDL `get(211)` (HIDL V1_0 fallback). `-1` = unsupported.
  `isLocalHBM` = bit 16; `isLocalHBMAccel` = bit 16 and bit 1024. => **cap211 = 0x410**.
* SystemUI also uses: `set(22,[1|0])` (OnScreenHighLightControl.hbmControl, lights the icon),
  `get/set(217)` (smooth AOD transition), `set(7,[mode])` (setAODLightMode),
  `set(213,[state])` (setFakeAodState), `set(195,[..])` (LTPO 1Hz AOD), `get(69)` (panel bit depth, 8 = 8-bit).
* FOD icon geometry comes from properties: `persist.vendor.fingerprint.sensor_type`
  (`optical`), `...optical.iconsize` (px, default 196), `...optical.iconlocation` (margin from bottom, px).
* So the FOD sequence in ColorOS is: finger press -> `set(28,1)` (services.jar) ... SystemUI lights
  the icon -> `set(22,1)` -> we send extCmd(10,1) -> HAL scans -> `set(22,0)` / `set(28,0)`.

## framework-res findings (this ROM)
* `config_dozeComponent` is **empty** and `config_dozeAlwaysOnDisplayAvailable=false` => doze/AOD pipeline does not start.
  Doze service class (SystemUI manifest): `com.android.systemui/com.android.systemui.doze.DozeService`
  -> handled by `overlay/DuchampOverlay`.
* `config_displayLightSensorType` / `config_dozeTapSensorType` / `config_dozeDoubleTapSensorType` /
  `config_dozeUdfpsLongPressSensorType` / `config_quickPickupSensorType` are empty;
  `config_dozePickupGestureEnabled=true`, `config_screenBrightnessDoze=1`.
  Sensor-related overlay values depend on which sensors the MT6897 HAL exposes (next step).
