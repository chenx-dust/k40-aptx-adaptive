# K40 aptX Adaptive for PixelOS

[中文](README-zh.md)

aptX Adaptive DSP offload for Redmi K40 / POCO F3 (`alioth`) running PixelOS Android 17.

## Platform

| Component | Configuration |
|---|---|
| Device | Redmi K40 / POCO F3 (`alioth`) |
| System | PixelOS Android 17 / SDK 37, build `1792813374` |
| Root | Magisk 31.1 |
| Codec | aptX Adaptive R2, 24-bit, 48 / 96 kHz negotiation |
| Transport | Single device, non-TWS+, HQ mode |
| Current version | 0.5 — Adaptive audio playback working |

## Implementation

- **Bluetooth stack:** Adaptive negotiation, AIDL configuration, Qualcomm `0xFC0A` start/stop commands and HQ mode selection.
- **ARM32 bridge:** Connects AIDL audio sessions to the device's DSP offload interface using its existing firmware.
- **Framework:** Adds codec-to-format mappings through source patches or three DEX method patches applied to the ROM's JARs.
- **Magisk module:** Validates and bind-mounts six files during boot, with rollback on activation failure. System partitions remain unchanged.

The vendor transport format is `0x25000000`; the Android framework audio format is `0x27000000`.

## Compatibility

Installation checks the device, SDK, component hashes and dependencies rather than requiring a particular build number. The on-device installer currently checks whole JAR/APK hashes, so re-signing or repacking requires updating the package profile.

The host-side `tools/archive_identity.py` compares archive contents independently of ZIP layout and signature records. It retains DEX, resource and meaningful Manifest checks. Integration with the on-device installer is pending.

## Build and test

- [Build and packaging](docs/BUILD.md)
- [Protocol changes and tests](docs/VALIDATION.md)
- [Third-party sources and licenses](NOTICE.md)

Run the host tests on Linux or WSL with Python 3 and a C++17-capable `g++`:

```sh
python3 tools/run_tests.py
```

The repository contains source, patches, module scripts and tests. Supply matching ROM files to build a module package.

## Installation and removal

Install the built module ZIP through the Magisk app, then reboot. To remove it, disable or uninstall the module in Magisk and reboot.

For recovery, create `/data/adb/modules/k40_pixelos_adaptive/disable` from an environment with access to `/data`, then reboot. Activation logs are stored in the module's `boot.log`.
