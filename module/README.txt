K40 aptX Adaptive for PixelOS — 0.5

Platform
Redmi K40 / POCO F3 (alioth), Android 17 / SDK 37, Magisk.
Adaptive R2, single-device non-TWS+ transport, HQ mode.
Version 0.5 supports Adaptive audio playback on the target PixelOS setup.

Changes in 0.5
Correct non-TWS role encoding, EDR/3 Mbps MTU handling and HQ mode command.
Start packets match CAF serialization across 72 input combinations.

Installation
Install the built module ZIP through Magisk and reboot.
The installer validates component hashes, dependencies and SELinux labels.
Build numbers are not used as a hard requirement.
JAR/APK checks currently use whole-file hashes.

Activation and recovery
The module bind-mounts six files during boot without writing system partitions.
Activation failures trigger rollback and disable the module.
An unfinished boot leaves boot_pending; the next activation skips the overlays.
To remove the module, disable or uninstall it in Magisk and reboot.
For recovery, create /data/adb/modules/k40_pixelos_adaptive/disable from an
environment with access to /data, then reboot.

Logs
See boot.log in the module directory or use the module action button.
