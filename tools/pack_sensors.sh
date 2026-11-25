#!/system/bin/sh
# Run as root on the phone (HiOS is fine). Output: /sdcard/sensors_pack.tgz
W=/sdcard/sensors_work; rm -rf $W; mkdir -p $W
dumpsys sensorservice > $W/sensorservice_dump.txt 2>&1
getprop | grep -iE "sensor|als|prox|aod|doze|wake|tilt|pickup|lcd|light" > $W/props_sensor.txt
{
  ls /vendor/lib64/hw 2>/dev/null | grep -i sensor
  find /vendor/etc /odm/etc -maxdepth 3 \( -ipath "*sensor*" -o -iname "*hals.conf" -o -iname "*multihal*" \) 2>/dev/null
  find /vendor/lib64 -maxdepth 1 -iname "*sensor*" 2>/dev/null
} > $W/list.txt
cat $W/list.txt | grep "^/" | while read f; do [ -f "$f" ] && [ "$(stat -c %s "$f")" -lt 15000000 ] && echo "$f"; done > $W/files.txt
ls /vendor/lib64/hw/sensors*.so 2>/dev/null >> $W/files.txt
grep -rIl -iE "sensor" /vendor/etc/vintf /odm/etc/vintf 2>/dev/null >> $W/files.txt
cat /proc/bus/input/devices > $W/input_devices.txt
ls -l /sys/class/sensors /sys/bus/iio/devices 2>/dev/null > $W/sysfs_sensors.txt
sort -u $W/files.txt > $W/files_u.txt
tar czf /sdcard/sensors_pack.tgz -C / $(sed 's|^/||' $W/files_u.txt | tr '\n' ' ') -C $W sensorservice_dump.txt props_sensor.txt list.txt input_devices.txt sysfs_sensors.txt 2>/dev/null
ls -lh /sdcard/sensors_pack.tgz
