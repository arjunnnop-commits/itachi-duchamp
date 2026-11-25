#!/system/bin/sh
# Usage: sh check_features.sh /data/DNA/DNA_colorosxx   -> /sdcard/features_report.txt
ROOT="${1:-/data/DNA/DNA_colorosxx}"
LIST="$(dirname "$0")/../props/oplus_features_to_check.txt"
[ -f "$LIST" ] || LIST="$2"
OUT=/sdcard/features_report.txt
: > "$OUT"
grep -v '^#' "$LIST" | while read -r f; do
  [ -z "$f" ] && continue
  hit=$(grep -rIl --include=*.xml --include=*.prop --include=*.conf -- "$f" "$ROOT" 2>/dev/null | head -2 | tr '\n' ' ')
  if [ -n "$hit" ]; then echo "PRESENT  $f  ($hit)" >> "$OUT"; else echo "MISSING  $f" >> "$OUT"; fi
done
# also dump the small feature xml folders for analysis
cd "$ROOT" && find . -path "*my_*/etc/permissions/*.xml" -o -path "*my_*/etc/extension/*.xml" 2>/dev/null | head -400 > /sdcard/features_files.txt
tar czf /sdcard/features_xml.tgz -T /sdcard/features_files.txt 2>/dev/null
cat "$OUT"; ls -lh /sdcard/features_xml.tgz
