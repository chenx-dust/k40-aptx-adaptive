# Build and packaging

## Source baselines

| Directory | Upstream | Commit |
|---|---|---|
| `packages/modules/Bluetooth` | `PixelOS-AOSP/android_packages_modules_Bluetooth` | `04618720b1bcefe990ab2d14e713a39393ae1072` |
| `hardware/interfaces` | `PixelOS-AOSP/android_hardware_interfaces` | `7447ce416bbc527a1e7100196eddd568d084b413` |
| `frameworks/base` | PixelOS Android 17 | `044b965aa2d30d056b930e5ff013ba5303cde9fa` |

1. Prepare a PixelOS `seventeen` source tree and an alioth build environment using the baselines above.
2. Run `git apply --check`, then apply `patches/bluetooth-adaptive-port.patch`, `hardware-interfaces-adaptive-r2.patch` and `framework-adaptive-audio-format.patch` in their respective repositories.
3. Copy `src/bridge` to `vendor/k40-adaptive/bridge` in the Android tree.
4. Configure the product and build `libbluetooth_jni libk40_btaudio_offload_if libbluetooth_audio_session_aidl k40_adaptive_aidl_test`. Framework source changes also require the corresponding system build.

Development used `aosp_arm64_plus_armv7-trunk_staging-eng` to build the ARM64 JNI and ARMv7 vendor libraries. Use the resulting components for module assembly; use an alioth product configuration for a full ROM build. The development tree also required local build-graph and memory adjustments.

## Module inputs

Copy the `module/` scripts into a staging directory and add:

- `payload/framework.jar` and `payload/framework-bluetooth.jar`: patched JARs derived from the target ROM.
- `payload/libbluetooth_jni.so`: ARM64 Bluetooth APEX JNI library.
- `payload/btaudio_offload_if.so`: the built ARM32 `libk40_btaudio_offload_if.so`, renamed for its mount target.
- `payload/libbluetooth_audio_session_aidl.so`: the matching ARM32 session library.
- `payload/audio_policy_configuration.xml`: the target ROM's policy with Adaptive primary DSP routes.
- `files.manifest`: six lines of `payload-path|absolute-target|original-sha256|payload-sha256|selinux-context`.
- `compatibility.manifest`: one `absolute-dependency-or-Bluetooth.apk-path|sha256` entry per audited input.

## Framework and policy generation

Build `smali-baksmali android-smali zipalign` in the Android tree, then run:

```sh
python3 tools/patch_framework.py --build 1792813374 \
  --input-dir /path/to/original-jars --output-dir /path/to/patched-jars \
  --android-tree /path/to/android --scratch-dir /path/to/empty-scratch
python3 tools/generate_policy.py /path/to/audio_policy_configuration.xml /path/to/policy-output
```

The framework tool uses content profiles for the two included baselines. Adding another baseline requires checking its DEX methods and related interfaces. The policy tool preserves existing routes and adds Adaptive-specific ports.

## Packaging

```sh
python3 tools/package.py /path/to/staging /path/to/output.zip
```

The packager checks manifests, payload hashes and ZIP integrity. Before creating a compatibility profile, check JNI registration, dependency exports and target SELinux labels. The installer checks the device files against that profile.
