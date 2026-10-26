#!/system/bin/sh
MODDIR=${0%/*}
[ -f "$MODDIR/disable" ] && exit 0
[ -f "$MODDIR/boot_pending" ] || exit 0
attempt=0
while [ "$attempt" -lt 90 ]; do
  if [ "$(getprop sys.boot_completed)" = 1 ]; then
    rm -f "$MODDIR/boot_pending"
    echo 'Boot completed; this does not confirm Adaptive playback.' >> "$MODDIR/boot.log"
    exit 0
  fi
  sleep 2
  attempt=$((attempt + 1))
done
: > "$MODDIR/disable"
echo 'Boot timeout: module disabled for next reboot; no forced reboot.' >> "$MODDIR/boot.log"
