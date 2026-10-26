#!/system/bin/sh
MODDIR=${0%/*}
echo 'K40 Adaptive experimental module status'
if [ -f "$MODDIR/disable" ]; then echo 'Disabled for next boot'; fi
if [ -f "$MODDIR/boot_pending" ]; then echo 'Boot completion not confirmed'; fi
tail -n 25 "$MODDIR/boot.log" 2>/dev/null
echo 'Disable in Magisk and reboot to remove the overlays.'
