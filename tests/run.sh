#!/bin/bash
# host self-test of the license system. Needs only gcc + python3.
set -e; W="$(cd "$(dirname "$0")/.." && pwd)"; T=$(mktemp -d); cd "$T"
P="python3 $W/tools/itachi_license.py"
build() { gcc -std=gnu11 -O1 -I"$W/tests/stub" -I"$T" -I"$W/third_party" -DLICENSE_PATH="\"$T/license.lic\"" -o "$T/t" "$W/tests/license_test.c" "$W/hal/itachi_license.c" "$W/third_party/tweetnacl.c"; }
$P keygen >/dev/null; $P sign --name @yooitachiijod --device duchamp >/dev/null
$P seal --out "$T/itachi_sealed.h" >/dev/null; build
export P_AUTHOR=@yooitachiijod P_VDEV=duchamp P_DEV=ossi
echo "--- 1 genuine";                  "$T/t" || true
echo "--- 2 wrong author prop";        P_AUTHOR=@someone "$T/t" | head -1 || true
echo "--- 3 wrong device";             P_VDEV=other P_DEV=other "$T/t" | head -1 || true
cp license.lic ok.lic; sed -i 's/duchamp/other/' license.lic
echo "--- 4 tampered license text";    "$T/t" | head -1 || true
cp ok.lic license.lic
$P sign --name @yooitachiijod --device duchamp --expires 2020-01-01 >/dev/null
echo "--- 5 expired license";          "$T/t" | head -1 || true
cp ok.lic license.lic
# binary patched to skip the check: sealed constants decode to garbage without the genuine signature
sed -i 's/^sig=.*/sig=00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff/' license.lic
echo "--- 6 forged signature";         "$T/t" | head -1 || true
cp ok.lic license.lic
mv itachi_private.key old.key; mv itachi_public.key old.pub; $P keygen >/dev/null
$P sign --name @yooitachiijod --device duchamp >/dev/null
echo "--- 7 license signed with another private key"; "$T/t" | head -1 || true
$P sign --name @yooitachiijod --device duchamp --serial ABC123 --priv old.key >/dev/null; mv old.pub itachi_public.key
$P seal --out "$T/itachi_sealed.h" >/dev/null; build
echo "--- 8 serial-bound, right serial"; P_SN=ABC123 "$T/t" | head -1
echo "--- 9 serial-bound, wrong serial"; P_SN=WRONG "$T/t" | head -1 || true
rm -rf "$T"
