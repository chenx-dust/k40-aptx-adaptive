# 构建说明

这是从已验证实验工作区整理的源码快照，不是完整 Android 源码树，也不是一键适配任意 ROM 的工具。

## 源码基线

| 目录 | 上游 | 提交 |
|---|---|---|
| `packages/modules/Bluetooth` | `PixelOS-AOSP/android_packages_modules_Bluetooth` | `04618720b1bcefe990ab2d14e713a39393ae1072` |
| `hardware/interfaces` | `PixelOS-AOSP/android_hardware_interfaces` | `7447ce416bbc527a1e7100196eddd568d084b413` |
| `frameworks/base` | PixelOS Android 17 | `044b965aa2d30d056b930e5ff013ba5303cde9fa` |

1. 准备 PixelOS `seventeen` 源码及正常工作的 alioth 构建环境，并核对以上基线。
2. 在对应仓库先运行 `git apply --check`，再应用 `patches/bluetooth-adaptive-port.patch`、`hardware-interfaces-adaptive-r2.patch` 和 `framework-adaptive-audio-format.patch`。
3. 把 `src/bridge` 放到 Android 源码树的 `vendor/k40-adaptive/bridge`。
4. 配置产品后构建 `libbluetooth_jni libk40_btaudio_offload_if libbluetooth_audio_session_aidl k40_adaptive_aidl_test`，framework 改动需配套构建系统。

原实验使用 `aosp_arm64_plus_armv7-trunk_staging-eng` 诊断产品构建 ARM64 JNI 与 ARMv7 vendor 库；该 generic 产品输出不是可直接刷入 alioth 的 ROM。原工作区曾为构建图和内存做本地修正，不能把这些编译结果理解成完整产品构建已可复现。

## 模块打包输入

准备一个 staging 目录，复制 `module/` 脚本，另外放入：

- `payload/framework.jar`、`payload/framework-bluetooth.jar`：由当前 ROM 的文件生成并验证的补丁产物，不能用 generic framework 替换。
- `payload/libbluetooth_jni.so`：ARM64 蓝牙 APEX JNI。
- `payload/btaudio_offload_if.so`：构建的 ARM32 `libk40_btaudio_offload_if.so`，以此名称挂载。
- `payload/libbluetooth_audio_session_aidl.so`：配套 ARM32 会话库。
- `payload/audio_policy_configuration.xml`：当前 ROM 的 policy，增加 Adaptive primary DSP 路由并保留其他配置。
- `files.manifest`：六行 `payload路径|目标绝对路径|原始SHA256|产物SHA256|SELinux上下文`。
- `compatibility.manifest`：逐行 `已审计依赖或Bluetooth.apk绝对路径|SHA256`。

DEX 与 policy 工具接受本地输入，不会读取手机或自动安装：

```sh
python3 tools/patch_framework.py --build 1792813374 \
  --input-dir /path/to/original-jars --output-dir /path/to/patched-jars \
  --android-tree /path/to/android --scratch-dir /path/to/empty-scratch
python3 tools/generate_policy.py /path/to/audio_policy_configuration.xml /path/to/policy-output
```

DEX 工具需要在 Android 树中先构建 `smali-baksmali android-smali zipalign`。所带内容指纹只覆盖两个已审计基线；不是通用自动适配器。原厂库 ABI 审计仍需自己完成，不能只修改 manifest 中的哈希来绕过兼容性检查。

```sh
python3 tools/package.py /path/to/staging /path/to/output.zip
```

打包器仅验证清单、文件哈希及 ZIP 完整性，不证明兼容性；安装前还要核对 JNI 注册、依赖导出、真实 SELinux 标签及回退路径。安装器会再次检查手机上的文件。
