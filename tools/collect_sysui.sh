#!/system/bin/sh
# ITACHI: pull only the SystemUI dex files that hold the FOD/AOD classes (not the whole 105MB apk). (c) @yooitachiijod
# su -c "sh /sdcard/Download/collect_sysui.sh"  -> /sdcard/Download/itachi_sysui.tar
A=/data/DNA/DNA_oxygenos16/system_ext/priv-app/SystemUI/SystemUI.apk
T=/sdcard/Download/itachi_sysui; rm -rf $T; mkdir -p $T/all
unzip -q -o $A 'classes*.dex' -d $T/all
mkdir -p $T/keep
for d in $T/all/classes*.dex; do
  grep -q "OnScreenFingerprintUiMech" "$d" && cp "$d" $T/keep/ && echo "keep $(basename $d)"
done
rm -rf $T/all
cd /sdcard/Download && tar -cf itachi_sysui.tar itachi_sysui && ls -l itachi_sysui.tar
