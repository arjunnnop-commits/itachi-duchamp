# itachid - FOD + AOD glue for ColorOS on a Xiaomi/MediaTek vendor (POCO X6 Pro)

A small native daemon that makes the ColorOS framework's "display panel feature" HAL calls work on top of
the Xiaomi HyperOS vendor/odm (Goodix optical FOD, MTK panel).

* Registers `vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default` (hand-written AIDL, no generated code).
* Turns ColorOS "finger pressed/released" (feature 28) into the `IXiaomiFingerprint::extCmd` sequence the Goodix HAL expects.
* Drives the AOD/doze nodes when ColorOS asks for AOD (feature 7).
* Everything is configurable in `/odm/etc/itachi.conf` and logged (`logcat -s itachid`).

See `docs/FINDINGS.md` for what was verified from the ROM and what is still unknown.

## Build (GitHub)
1. Push this folder to a new GitHub repository.
2. Actions -> **build** -> download the artifact `itachid-arm64`.

## Install (into your own odm)
| from artifact | to |
|---|---|
| `odm/bin/hw/itachid` | `/odm/bin/hw/itachid` (mode 0755) |
| `odm/etc/init/itachid.rc` | `/odm/etc/init/itachid.rc` |
| `odm/etc/itachi.conf` | `/odm/etc/itachi.conf` |
| `odm/etc/itachi.lic` | `/odm/etc/itachi.lic` (**required**, see Authorization) |
| `odm/etc/vintf/manifest/itachid.xml` | `/odm/etc/vintf/manifest/itachid.xml` |
| `odm/overlay/DuchampOverlay.apk` | `/odm/overlay/DuchampOverlay.apk` (static RRO, mode 0644, label `vendor_overlay_file`-style like your other overlays) |
| `props/duchamp_build.prop.append` | append to `/odm/etc/build.prop` (FOD icon size/location etc.) |
| `sepolicy-snippets/*.add` | append to `odm_service_contexts`, `odm_file_contexts`, `odm_sepolicy.cil` (from the duchamp_* files) |

## Bring-up order
1. Boot with `setenforce 0` first. `logcat -s itachid` should show `registered vendor.oplus...`.
2. Test the Xiaomi side without ColorOS: `itachid ext 10 1`, `itachid aod 1`, `itachid aod 0`.
3. Lock the screen, unlock with FOD, watch `itachid` log: you should see `get(211)`, `set(28,[1])`, `extCmd(...)`.
4. Tune `cap211`, `ready_delay_ms`, `aod_on_value`, `lhbm_mode` in `itachi.conf` (no rebuild needed).
5. Collect `dmesg | grep "avc: denied"` and extend the sepolicy, then go back to enforcing.

## Test the odm half on HiOS first (same vendor/odm as ColorOS)
You do not need ColorOS to validate everything below the framework:
1. Install the odm files on HiOS (see Install), boot. If `odm_sepolicy.cil` has a syntax error the device will
   not boot - this is the safest place to find out.
2. `su -c 'setenforce 0'`, then `logcat -s itachid` -> `registered vendor.oplus...`.
3. `itachid svc get 211 1` should print `vals=[1040]`.
4. `itachid aod 1` / `itachid aod 0` (with the screen in doze) and watch the panel.
5. `itachid sim-fod 400` replays what ColorOS sends (get 211, set 28, set 22) and shows the `extCmd` calls
   reaching the Goodix HAL. (HiOS's own framework also talks to the HAL, so do this with the screen locked and
   watch `logcat | grep -i -E "itachid|GF_HAL"`.)

## Authorization (name: `joditachi`)
`itachid` refuses to run (service and every CLI command) unless `/odm/etc/itachi.lic` contains
`HMAC-SHA256(secret, "joditachi|duchamp")` for the secret that was compiled into that exact build.
* GitHub: add a repository secret `ITACHI_SECRET_HEX` (64 hex chars, e.g. `openssl rand -hex 32`).
  The workflow bakes it into the binary and ships the matching `itachi.lic` in the artifact.
  Without the secret a random one is used per build (still consistent inside that artifact).
* Local build: `tools/make_license.py <secret-hex> > itachi.lic`.
* Check on device: `itachid whoami`.
* Be realistic: this is a client-side check. Anyone with the binary can patch it out; it is meant to tie a
  build to its owner, not to be tamper-proof.

## One-command apply / remove (unpacked odm folder)
```
sh tools/itachi_ctl.sh apply  /path/to/odm   # copy files, add marked blocks, drop precompiled sepolicy
sh tools/itachi_ctl.sh status /path/to/odm
sh tools/itachi_ctl.sh remove /path/to/odm   # undo everything, restore the precompiled sepolicy
```
* Run it from the extracted GitHub artifact (it finds `odm/`, `sepolicy-snippets/`, `props/` next to `tools/`),
  or pass `--src <artifact dir>`.
* Originals are backed up once to `<odm>.itachi_backup`; apply twice is safe (blocks are replaced, not duplicated).
* `--dry-run` shows what would happen. `--force-conf` overwrites your tuned `itachi.conf`.
* It refuses to touch `/odm` itself (live partition is read-only) - use your unpacked copy, then repack and flash.
* A `<odm>.itachi_repack_hints.txt` is written with fs_config lines for your repack tool.
