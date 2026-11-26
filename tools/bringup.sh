#!/system/bin/sh
# Run on the phone as root (Termux: su -c "sh /sdcard/Download/bringup.sh").
# Starts itachi-hal by hand (SELinux permissive), waits for you to test, then writes ONE report.
D=/data/local/tmp/itachi
mkdir -p $D
cp /sdcard/Download/itachi-hal $D/ && cp /sdcard/Download/itachi.conf $D/ 2>/dev/null
chmod 755 $D/itachi-hal
setenforce 0
pkill itachi-hal 2>/dev/null
logcat -c
nohup $D/itachi-hal >/dev/null 2>&1 &
sleep 3
echo "itachi-hal started. Now: toggle Double-tap-to-wake in Settings, turn the screen off/on,"
echo "enable Always-on display, lock and unlock with the fingerprint. Then press Enter."
read x
R=/sdcard/Download/itachi_report.txt
{
 echo "== services"; service list 2>/dev/null | grep -iE "oplus.*(touch|displaypanel)|itachi"
 echo "== touch_dev"; ls -l /sys/class/touch/touch_dev/
 echo "== mi_display"; ls -l /sys/class/mi_display/disp-DSI-0/
 echo "== doze_brightness"; cat /sys/class/mi_display/disp-DSI-0/doze_brightness
 echo "== getenforce"; getenforce
 echo "== itachi-hal log"; logcat -d -s itachi-hal:I | tail -150
 echo "== binder/avc"; logcat -d | grep -iE "avc:.*itachi|IOplusTouch|DisplayPanelFeature|waitForDeclared" | tail -60
} > $R 2>&1
echo "Report: $R"
