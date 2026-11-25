#!/bin/sh
# itachi_ctl.sh - apply / remove / status of itachid inside an UNPACKED odm directory.
#
#   sh itachi_ctl.sh apply  <ODM_DIR> [--src <ARTIFACT_DIR>] [--force-conf] [--dry-run]
#   sh itachi_ctl.sh remove <ODM_DIR> [--dry-run]
#   sh itachi_ctl.sh status <ODM_DIR> [--src <ARTIFACT_DIR>]
#
# ARTIFACT_DIR is the extracted GitHub artifact (default: the folder above this script).
# Works on a directory only (the live /odm partition is read-only). Repack + flash afterwards.

set -e
TAG="itachi-duchamp"
CMD="$1"; ODM="$2"; shift 2 2>/dev/null || true
SRC="$(cd "$(dirname "$0")/.." 2>/dev/null && pwd)"
FORCE_CONF=0; DRY=0
while [ $# -gt 0 ]; do
  case "$1" in
    --src) SRC="$2"; shift ;;
    --force-conf) FORCE_CONF=1 ;;
    --dry-run) DRY=1 ;;
    *) echo "unknown option: $1"; exit 2 ;;
  esac
  shift
done

die() { echo "ERROR: $*" >&2; exit 1; }
say() { echo "[itachi] $*"; }
run() { if [ "$DRY" = 1 ]; then echo "  (dry-run) $*"; else "$@"; fi; }

[ -n "$CMD" ] && [ -n "$ODM" ] || { sed -n '2,10p' "$0"; exit 2; }
ODM="${ODM%/}"
[ "$ODM" = "/odm" ] && die "/odm is the live read-only partition. Point this at an unpacked copy of odm."
[ -d "$ODM" ] || die "not a directory: $ODM"
[ -d "$ODM/etc" ] || die "$ODM does not look like an odm tree (no etc/)"

BK="${ODM}.itachi_backup"
HINTS="${ODM}.itachi_repack_hints.txt"

FILES="bin/hw/itachid etc/init/itachid.rc etc/itachi.conf etc/itachi.lic etc/vintf/manifest/itachid.xml overlay/DuchampOverlay.apk"
SEPOL="etc/selinux/odm_sepolicy.cil"
SVCCTX="etc/selinux/odm_service_contexts"
FILECTX="etc/selinux/odm_file_contexts"
PRECOMP="etc/selinux/precompiled_sepolicy etc/selinux/precompiled_sepolicy.plat_sepolicy_and_mapping.sha256 etc/selinux/precompiled_sepolicy.system_ext_sepolicy_and_mapping.sha256 etc/selinux/precompiled_sepolicy.product_sepolicy_and_mapping.sha256"

# which build.prop does this odm use?
if [ -f "$ODM/etc/build.prop" ]; then PROP="etc/build.prop"
elif [ -f "$ODM/build.prop" ]; then PROP="build.prop"
else PROP="etc/build.prop"; fi

begin_of() { echo "$1 >>> $TAG (managed by itachi_ctl.sh, do not edit)"; }
end_of()   { echo "$1 <<< $TAG"; }

strip_block() {  # file comment-char
  f="$ODM/$1"; c="$2"; [ -f "$f" ] || return 0
  b="$(begin_of "$c")"; e="$(end_of "$c")"
  awk -v b="$b" -v e="$e" '$0==b{skip=1;next} $0==e{skip=0;next} !skip' "$f" > "$f.itachi.tmp" && mv "$f.itachi.tmp" "$f"
}

append_block() {  # file comment-char snippet-file
  f="$ODM/$1"; c="$2"; s="$3"
  [ -f "$s" ] || die "missing snippet: $s"
  run mkdir -p "$(dirname "$f")"
  if [ "$DRY" = 1 ]; then echo "  (dry-run) append block to $1 from $s"; return; fi
  [ -f "$f" ] || : > "$f"
  strip_block "$1" "$c"
  [ -s "$f" ] && [ "$(tail -c1 "$f" | wc -l)" -eq 0 ] && echo >> "$f"
  { begin_of "$c"; cat "$s"; end_of "$c"; } >> "$f"
}

backup_once() {  # relpath
  [ -e "$ODM/$1" ] || return 0
  [ -e "$BK/$1" ] && return 0
  run mkdir -p "$BK/$(dirname "$1")"
  run cp -p "$ODM/$1" "$BK/$1"
}

has_content() { [ -f "$1" ] && grep -q '[^[:space:]]' "$1"; }

do_apply() {
  [ -f "$SRC/odm/bin/hw/itachid" ] || die "artifact not found at $SRC (expected $SRC/odm/bin/hw/itachid). Use --src."
  [ -f "$SRC/odm/etc/itachi.lic" ] || die "itachi.lic missing in artifact"
  say "applying to $ODM (artifact: $SRC)"

  # Back up the ORIGINAL files exactly once (first apply). Later applies must never back up our own edits.
  if [ ! -e "$BK/.initialized" ]; then
    for f in $PRECOMP $SEPOL $SVCCTX $FILECTX $PROP; do backup_once "$f"; done
    if [ "$DRY" = 0 ]; then mkdir -p "$BK" && : > "$BK/.initialized"; fi
  fi

  for f in $FILES; do
    [ -f "$SRC/odm/$f" ] || { say "skip (not in artifact): $f"; continue; }
    if [ "$f" = "etc/itachi.conf" ] && [ -f "$ODM/$f" ] && [ "$FORCE_CONF" = 0 ]; then
      say "keep existing $f (your tuning). Use --force-conf to overwrite."
      continue
    fi
    run mkdir -p "$ODM/$(dirname "$f")"
    run cp "$SRC/odm/$f" "$ODM/$f"
    case "$f" in bin/*) run chmod 0755 "$ODM/$f" ;; *) run chmod 0644 "$ODM/$f" ;; esac
    say "installed $f"
  done

  append_block "$SEPOL"  ";" "$SRC/sepolicy-snippets/duchamp_odm_sepolicy.cil.add"
  append_block "$SVCCTX" "#" "$SRC/sepolicy-snippets/duchamp_odm_service_contexts.add"
  append_block "$FILECTX" "#" "$SRC/sepolicy-snippets/duchamp_odm_file_contexts.add"
  append_block "$PROP"   "#" "$SRC/props/duchamp_build.prop.append"
  say "updated sepolicy / contexts / $PROP"

  for f in $PRECOMP; do
    if [ -e "$ODM/$f" ]; then run rm -f "$ODM/$f"; say "removed $f (forces policy recompile at boot)"; fi
  done

  if [ "$DRY" = 0 ]; then
    {
      echo "# Repack hints for $ODM  (adapt to your tool: fs_config + file_contexts)"
      echo "odm/bin/hw/itachid 0 2000 0755 capabilities=0x0"
      echo "odm/etc/init/itachid.rc 0 0 0644 capabilities=0x0"
      echo "odm/etc/itachi.conf 0 0 0644 capabilities=0x0"
      echo "odm/etc/itachi.lic 0 0 0644 capabilities=0x0"
      echo "odm/etc/vintf/manifest/itachid.xml 0 0 0644 capabilities=0x0"
      echo "odm/overlay/DuchampOverlay.apk 0 0 0644 capabilities=0x0"
      echo "# SELinux labels (already in odm_file_contexts): /odm/bin/hw/itachid -> hal_fingerprint_default_exec"
      echo "# Give DuchampOverlay.apk the same label your other /odm/overlay/*.apk have."
    } > "$HINTS"
    say "repack hints written to $HINTS"
  fi
  say "done. Now repack odm and flash; first boot is slower (policy recompile). Remove with: sh $0 remove $ODM"
}

do_remove() {
  say "removing from $ODM"
  for f in $FILES; do
    if [ -e "$ODM/$f" ]; then run rm -f "$ODM/$f"; say "deleted $f"; fi
  done
  for pair in "$SEPOL ;" "$SVCCTX #" "$FILECTX #" "$PROP #"; do
    set -- $pair
    if [ "$DRY" = 1 ]; then echo "  (dry-run) strip block from $1"; else strip_block "$1" "$2"; fi
  done
  # drop files that only existed because we created them (empty after stripping)
  for f in $SEPOL $SVCCTX $FILECTX; do
    if [ -f "$ODM/$f" ] && ! has_content "$ODM/$f" && [ ! -e "$BK/$f" ] && [ "$DRY" = 0 ]; then rm -f "$ODM/$f"; fi
  done
  # restore the precompiled policy that apply removed
  for f in $PRECOMP; do
    if [ -e "$BK/$f" ] && [ ! -e "$ODM/$f" ]; then
      run mkdir -p "$ODM/$(dirname "$f")"; run cp -p "$BK/$f" "$ODM/$f"; say "restored $f"
    fi
  done
  # remove folders we may have created, only if they are now empty (rmdir refuses otherwise)
  if [ "$DRY" = 0 ]; then
    for d in bin/hw bin etc/vintf/manifest etc/vintf etc/init etc/selinux overlay; do
      rmdir "$ODM/$d" 2>/dev/null || true
    done
  fi
  say "done. Backups kept in $BK (delete it when you no longer need it). Repack odm and flash."
}

do_status() {
  say "status of $ODM"
  for f in $FILES; do [ -e "$ODM/$f" ] && echo "  present  $f" || echo "  missing  $f"; done
  for pair in "$SEPOL ;" "$SVCCTX #" "$FILECTX #" "$PROP #"; do
    set -- $pair
    if [ -f "$ODM/$1" ] && grep -qF "$(begin_of "$2")" "$ODM/$1"; then echo "  block    $1"; else echo "  no block $1"; fi
  done
  for f in $PRECOMP; do [ -e "$ODM/$f" ] && echo "  precompiled policy still present: $f (apply removes it)"; done
  [ -d "$BK" ] && echo "  backup dir: $BK"
}

case "$CMD" in
  apply) do_apply ;;
  remove) do_remove ;;
  status) do_status ;;
  *) die "unknown command: $CMD" ;;
esac
