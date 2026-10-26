# K40 aptX Adaptive for PixelOS

[English](README.md)

为运行 PixelOS Android 17 的 Redmi K40 / POCO F3（`alioth`）提供 aptX Adaptive DSP offload 支持。

## 平台

| 组件 | 配置 |
|---|---|
| 设备 | Redmi K40 / POCO F3（`alioth`）|
| 系统 | PixelOS Android 17 / SDK 37，构建 `1792813374` |
| Root | Magisk 31.1 |
| 编码 | aptX Adaptive R2，24-bit，48 / 96 kHz 协商 |
| 传输 | 单设备、非 TWS+、HQ 模式 |
| 当前版本 | 0.5，Adaptive 音频播放正常 |

## 实现

- **蓝牙栈：** Adaptive 协商、AIDL 配置、Qualcomm `0xFC0A` 启停命令与 HQ 模式选择。
- **ARM32 桥接库：** 将 AIDL 音频会话接入设备的 DSP offload 接口，使用现有固件。
- **Framework：** 通过源码补丁或对当前 ROM JAR 的三处 DEX 方法补丁增加编码与音频格式映射。
- **Magisk 模块：** 启动时校验并 bind mount 六个文件，激活失败时回退，不改写系统分区。

厂商传输格式为 `0x25000000`，Android framework 音频格式为 `0x27000000`。

## 兼容性

安装时检查设备、SDK、组件哈希和依赖，不要求固定构建号。当前手机端安装器仍检查整个 JAR/APK 的哈希，重新签名或重打包后需要更新包的兼容配置。

主机端 `tools/archive_identity.py` 可独立于 ZIP 包装和签名记录比较归档内容，保留 DEX、资源及有意义的 Manifest 属性检查。手机端安装器集成尚待完成。

## 构建与测试

- [构建和打包](docs/BUILD.md)
- [协议修改与测试](docs/VALIDATION.md)
- [第三方来源与许可证](NOTICE.md)

在 Linux / WSL 安装 Python 3 和支持 C++17 的 `g++` 后运行：

```sh
python3 tools/run_tests.py
```

仓库包含源码、补丁、模块脚本和测试。构建模块包时需提供匹配的 ROM 文件。

## 安装与卸载

通过 Magisk 应用安装构建好的模块 ZIP，然后重启。卸载时在 Magisk 中停用或卸载模块，再重启。

需要恢复时，在可访问 `/data` 的环境中创建 `/data/adb/modules/k40_pixelos_adaptive/disable`，然后重启。激活日志位于模块目录的 `boot.log`。
