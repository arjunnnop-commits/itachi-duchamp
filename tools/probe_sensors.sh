#!/system/bin/sh
# ITACHI: dump what the sensor stack on duchamp exposes (proximity / pickup / ALS types). (c) @yooitachiijod
# Run as root. Output: /sdcard/Download/itachi_sensors.txt  -> send it back.
R=/sdcard/Download/itachi_sensors.txt
{
 echo "== sensor list"; dumpsys sensorservice | sed -n '1,/Recent Sensor events/p' | grep -iE "^0x|type|vendor|proxim|pick|tilt|light|als|wake|doze|fod|aod|hand"
 echo "== vendor sensors lib"; ls -l /vendor/lib64/hw/ | grep -i sensors
 echo "== input devices"; cat /proc/bus/input/devices | grep -E "Name=|Handlers="
 echo "== touch_dev nodes"; ls -l /sys/class/touch/touch_dev/ 2>&1
 echo "== mi_display nodes"; ls -l /sys/class/mi_display/disp-DSI-0/ 2>&1
 echo "== fp hal"; ls -l /vendor/bin/hw/ | grep -iE "fingerprint|mfp|biometric"; getprop | grep -iE "fp|fingerprint|fod"
 echo "== proximity smali candidates (system)"; ls /system/framework/ | grep -i oplus-services
} > $R 2>&1
echo "saved $R"
