#!/system/bin/sh
[ "$BOOTMODE" = true ] || abort 'Install from the Magisk app, not recovery.'
[ "$ARCH" = arm64 ] && [ "$API" = 37 ] || abort 'Requires alioth Android 17 ARM64.'
MODDIR=$MODPATH
. "$MODDIR/common.sh"
K40_UPGRADE=1
k40_identity || abort 'Requires alioth / Android SDK 37.'
k40_verify || abort 'ROM/payload hash mismatch. Disable an older version and reboot before installing.'
ui_print "Component checks passed for build $(getprop ro.build.version.incremental)."
set_perm_recursive "$MODDIR" 0 0 0755 0644
for script in post-fs-data.sh service.sh action.sh; do
  set_perm "$MODDIR/$script" 0 0 0755
done
while IFS='|' read -r source target baseline payload context; do
  set_perm "$MODDIR/$source" 0 0 0644 "$context"
done < "$MODDIR/files.manifest"
ui_print 'Experimental package: successful boot and Adaptive audio are not yet verified.'
ui_print 'To recover: disable this module in Magisk or create its disable file from recovery.'
ui_print 'A failed boot attempt will be skipped on the next reboot.'
