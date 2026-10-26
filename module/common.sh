#!/system/bin/sh
# Manifest fields: payload-relative-path|target|baseline-sha256|payload-sha256|SELinux-context
k40_hash() {
  k40_sum=$(sha256sum "$1" 2>/dev/null) || return 1
  [ "${k40_sum%% *}" = "$2" ]
}
k40_identity() {
  [ "$(getprop ro.product.device)" = alioth ] &&
  [ "$(getprop ro.build.version.sdk)" = 37 ]
}
k40_baseline_hash() {
  k40_hash "$1" "$2" && return 0
  # Installer-only upgrade: accept a running overlay only when its recorded
  # original hash matches this package, and its live bytes match the old payload.
  # Boot activation never sets K40_UPGRADE and always requires original files.
  [ "$K40_UPGRADE" = 1 ] || return 1
  [ -f /data/adb/modules/k40_pixelos_adaptive/files.manifest ] || return 1
  while IFS='|' read -r old_source old_target old_baseline old_payload old_context; do
    if [ "$old_target" = "$1" ] && [ "$old_baseline" = "$2" ]; then
      k40_hash "$1" "$old_payload" && return 0
    fi
  done < /data/adb/modules/k40_pixelos_adaptive/files.manifest
  return 1
}
k40_dependencies() {
  [ -s "$MODDIR/compatibility.manifest" ] || return 1
  while IFS='|' read -r target expected; do
    case "$target" in
      /apex/com.android.bt/app/*/Bluetooth.apk|/vendor/lib/*.so|/system/lib/*.so|/system/lib64/*.so|/apex/com.android.runtime/lib/bionic/*.so|/apex/com.android.runtime/lib64/bionic/*.so) ;;
      *) return 1 ;;
    esac
    case "$target" in *..*|*' '*) return 1 ;; esac
    [ "${#expected}" -eq 64 ] || return 1
    case "$expected" in *[!0-9a-f]*) return 1 ;; esac
    k40_baseline_hash "$target" "$expected" || return 1
  done < "$MODDIR/compatibility.manifest"
}
k40_verify() {
  k40_dependencies || return 1
  [ -s "$MODDIR/files.manifest" ] || return 1
  k40_count=0
  k40_seen='|'
  while IFS='|' read -r source target baseline payload context; do
    case "$source" in payload/*) ;; *) return 1 ;; esac
    case "$source" in *..*|*' '*) return 1 ;; esac
    case "$target" in
      /system/framework/framework.jar|/apex/com.android.bt/javalib/framework-bluetooth.jar|/apex/com.android.bt/lib64/libbluetooth_jni.so|/vendor/lib/btaudio_offload_if.so|/vendor/lib/libbluetooth_audio_session_aidl.so|/vendor/etc/audio_policy_configuration.xml) ;;
      *) return 1 ;;
    esac
    case "$k40_seen" in *"|$target|"*) return 1 ;; esac
    k40_seen="$k40_seen$target|"
    [ "${#baseline}" -eq 64 ] && [ "${#payload}" -eq 64 ] || return 1
    case "$baseline$payload" in *[!0-9a-f]*) return 1 ;; esac
    case "$target:$context" in
      /system/framework/framework.jar:u:object_r:system_file:s0|/apex/com.android.bt/javalib/framework-bluetooth.jar:u:object_r:system_file:s0|/apex/com.android.bt/lib64/libbluetooth_jni.so:u:object_r:system_lib_file:s0|/vendor/lib/*:u:object_r:vendor_file:s0|/vendor/etc/audio_policy_configuration.xml:u:object_r:vendor_configs_file:s0) ;;
      *) return 1 ;;
    esac
    [ -f "$target" ] || return 1
    k40_label=$(ls -Zd "$target") || return 1
    [ "${k40_label%% *}" = "$context" ] || return 1
    k40_baseline_hash "$target" "$baseline" || return 1
    k40_hash "$MODDIR/$source" "$payload" || return 1
    k40_count=$((k40_count + 1))
  done < "$MODDIR/files.manifest"
  [ "$k40_count" -eq 6 ]
}
