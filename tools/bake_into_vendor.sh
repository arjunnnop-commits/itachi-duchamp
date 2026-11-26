#!/usr/bin/env bash
# Copy ITACHI files into an unpacked vendor tree. Usage: ./tools/bake_into_vendor.sh <unpacked vendor dir> [system dir]
# (c) @yooitachiijod
set -euo pipefail
V="${1:?path to unpacked vendor dir}"; S="${2:-}"
[ -f out/itachi-hal ] || { echo "build first: NDK=... ./build_all.sh"; exit 1; }
install -D -m 0755 out/itachi-hal                          "$V/bin/hw/itachi-hal"
install -D -m 0644 rootfs/vendor/etc/init/itachi-hal.rc     "$V/etc/init/itachi-hal.rc"
install -D -m 0644 rootfs/vendor/etc/vintf/manifest/itachi-hal.xml "$V/etc/vintf/manifest/itachi-hal.xml"
install -D -m 0644 rootfs/vendor/etc/itachi/itachi.conf     "$V/etc/itachi/itachi.conf"
[ -f license.lic ] || { echo "license.lic missing (tools/itachi_license.py sign)"; exit 1; }
install -D -m 0644 license.lic                              "$V/etc/itachi/license.lic"
[ -n "$S" ] && cat rootfs/system/build.prop.append >> "$S/build.prop" && echo "build.prop appended"
echo "Now append to the vendor policy/contexts and repack (fs_config too):"
echo "  sepolicy/itachi.cil                      -> vendor/etc/selinux/vendor_sepolicy.cil"
echo "  sepolicy/vendor_file_contexts.append     -> vendor/etc/selinux/vendor_file_contexts"
echo "  sepolicy/vendor_service_contexts.append  -> vendor/etc/selinux/vendor_service_contexts"
echo "Then in itachi-hal.rc enable the seclabel line. Fix denials: logcat -d|grep avc:|python3 tools/avc2cil.py"
