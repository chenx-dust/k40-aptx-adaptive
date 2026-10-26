#!/system/bin/sh
MODDIR=${0%/*}
unset K40_UPGRADE
[ -f "$MODDIR/disable" ] && exit 0
exec >> "$MODDIR/boot.log" 2>&1
namespace_fail() {
  : > "$MODDIR/disable"
  echo 'Cannot enter init mount namespace; module disabled.'
  exit 1
}
# APEX overlays must be inherited by init's future services and Zygote.
self_ns=$(readlink /proc/self/ns/mnt) || namespace_fail
init_ns=$(readlink /proc/1/ns/mnt) || namespace_fail
if [ "$self_ns" != "$init_ns" ]; then
  [ "$K40_NAMESPACE_ENTERED" != 1 ] || namespace_fail
  export K40_NAMESPACE_ENTERED=1
  /data/adb/magisk/busybox nsenter -t 1 -m -- /data/adb/magisk/busybox sh "$0" || namespace_fail
  exit 0
fi
echo "Starting experimental Adaptive module"
. "$MODDIR/common.sh"
mounted=
properties_changed=0
rollback() {
  for target in $mounted; do
    umount "$target" || echo "Could not unmount $target"
  done
  if [ "$properties_changed" = 1 ]; then
    while IFS='|' read -r key value; do
      if [ -n "$value" ]; then
        resetprop -n "$key" "$value"
      else
        resetprop -n --delete "$key"
      fi
    done < "$MODDIR/boot.properties"
  fi
  : > "$MODDIR/disable"
  echo 'Activation failed; module disabled for the next boot.'
}
fail() { rollback; exit 1; }
trap 'fail' HUP INT TERM
if [ -e "$MODDIR/boot_pending" ]; then
  echo 'Previous boot did not complete; skipping all overlays.'
  : > "$MODDIR/disable"
  exit 0
fi
k40_identity && k40_verify || fail
# Never replace bootclasspath/native files after their consumers have started.
if pidof zygote zygote64 audioserver android.hardware.audio.service >/dev/null 2>&1; then
  echo 'Consumers already running; refusing late activation.'
  fail
fi
: > "$MODDIR/boot_pending" || fail
while IFS='|' read -r source target baseline payload context; do
  chcon "$context" "$MODDIR/$source" || fail
  mount -o bind "$MODDIR/$source" "$target" || fail
  mounted="$target $mounted"
  k40_hash "$target" "$payload" || fail
done < "$MODDIR/files.manifest"
: > "$MODDIR/boot.properties" || fail
for key in ro.bluetooth.a2dp_offload.supported persist.bluetooth.a2dp_offload.disabled persist.vendor.audio.a2dp_codec_extensibility; do
  printf '%s|%s\n' "$key" "$(getprop "$key")" >> "$MODDIR/boot.properties" || fail
done
properties_changed=1
# -n modifies the live property area without invoking init/property triggers.
# Do not write persistent property storage or use setprop in post-fs-data.
resetprop -n ro.bluetooth.a2dp_offload.supported true || fail
resetprop -n persist.bluetooth.a2dp_offload.disabled false || fail
resetprop -n persist.vendor.audio.a2dp_codec_extensibility false || fail
trap - HUP INT TERM
echo 'All six overlays activated. Waiting for boot completion.'
