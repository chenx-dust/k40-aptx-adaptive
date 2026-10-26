# K40 aptX Adaptive for PixelOS

为 Redmi K40 / POCO F3（`alioth`）的 PixelOS Android 17 增加 aptX Adaptive DSP offload 支持的实验项目。

**0.5 已在一台 alioth 上启用，设备使用者于 2026-10-27 确认 aptX Adaptive 正常出声。** 这是单设备反馈，不代表其他 ROM、耳机或所有采样率均已验证。

## 已知环境与范围

| 项目 | 状态 |
|---|---|
| 系统 | PixelOS Android 17 / SDK 37，构建 `1792813374`，2026-10 安全补丁 |
| Root | Magisk 31.1 |
| 启动及 Adaptive 可听播放 | 0.5 已由设备使用者确认 |
| 协商目标 | Adaptive R2，24-bit，48 / 96 kHz；96 kHz 实机结果未单独确认 |
| 当前实现 | 非 TWS+、单设备、HQ 模式 |
| 暂停/重连/普通编码回归 | 仍需系统测试 |

本仓库发布源码、补丁、加载脚本和测试，不包含从手机提取的 JAR、APK、固件、boot 镜像、日志或私有编码器。编译和打包需要自行提供匹配的系统文件。

## 实现

- Bluetooth 源码补丁：Adaptive 协商、AIDL 配置、Qualcomm `0xFC0A` 启停与 HQ 模式命令。
- ARM32 桥接库：把 AIDL 音频会话接到设备既有 DSP offload 接口，保留原有 DSP 固件。
- Framework：源码映射补丁；实验模块采用当前设备 JAR 的三处 DEX 方法补丁。
- Magisk：启动时校验并 bind mount 六个文件，失败时尝试回退；不写系统分区。

厂商传输格式 `0x25000000` 与 Android framework 音频格式 `0x27000000` 属于不同接口，不能混用。

## 兼容性限制

构建号本身不是硬限制，设备型号、SDK 和已审计组件仍须匹配。**当前手机端安装器仍校验整个 JAR/APK 的哈希**，重新签名或重打包可能被拒绝。

`tools/archive_identity.py` 是已测试的主机侧内容指纹原型，可忽略 ZIP 包装和签名记录变化，保留 DEX、资源和有意义的 Manifest 属性检查；它尚未集成到手机端安装器，也不是签名真实性验证。不要把它理解成“任意更新都兼容”。

## 使用与开发

- [构建和打包](docs/BUILD.md)
- [实现与验证范围](docs/VALIDATION.md)
- [第三方来源](NOTICE.md)

在 Linux / WSL 安装 Python 3 和支持 C++17 的 `g++` 后运行：

```sh
python3 tools/run_tests.py
```

模块通过 Magisk 应用安装，不能把源码 ZIP 当作 recovery 包刷入。使用经过核对的模块包后重启生效。停用模块并重启可恢复原始文件；开机失败时可在能访问 `/data` 的环境创建 `/data/adb/modules/k40_pixelos_adaptive/disable` 后重启。自动回退依赖 Magisk 能运行脚本，不保证所有启动故障均可自动恢复。

aptX 是其权利人的商标。本项目非官方项目，不附带专利授权。
