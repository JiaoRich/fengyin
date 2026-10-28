# 第三方组件说明

## ScreenRecorderLib 7.0.1 / miniaudio 0.11.23

- 独立 `FengYinRecorder.exe` 使用 ScreenRecorderLib 原生 C++ 核心录屏、miniaudio 的 WASAPI 回环与 WAV 编码录音；不依赖 .NET UI，不采麦克风。
- ScreenRecorderLib 附有一项 WASAPI 静音包复制修复：在自有缓冲中处理整包静音，不写入 Windows 端点缓冲。安装包保留原始源码、精确修改补丁、配套头文件与修改前后校验记录。
- ScreenRecorderLib：MIT，https://github.com/sskodje/ScreenRecorderLib ，固定提交 `646383234187efd3854e6b1069612b80c3ab9981`。
- miniaudio：Public Domain 或 MIT No Attribution 双许可，https://github.com/mackron/miniaudio/tree/0.11.23 。
- `components/Recording/` 随附版本/校验记录、完整上游源码和许可证（包括源码附带的第三方声明）。各组件以其自身许可为准。

## Fresh Air 1.0.8

The Windows installer includes the original Fresh Air 1.0.8 installer supplied
by the product owner. Fresh Air is developed by Slate Digital and is used by
the bundled “次中萨-气包音” tone scheme.

风吟使用下列第三方组件。此文件是发布检查清单，不替代各组件的正式许可证文本。

## JUCE

- 用途：桌面界面、MIDI、音频、VST3 宿主、视频和 DSP。
- 当前工程版本：9.0.2。
- 正式商业发布前必须取得与发行方式相符的 JUCE 授权，并随安装包保留所需声明。

## FFmpeg

- 用途：在系统无法直接解码视频音轨时，离线提取伴奏缓存。
- Windows 安装包使用 BtbN FFmpeg Builds 提供的 FFmpeg 8.1 x64 LGPL shared 构建，作为独立命令行程序运行。
- 下载来源：https://github.com/BtbN/FFmpeg-Builds/releases/tag/autobuild-2026-09-15-13-18
- FFmpeg 主要采用 GNU Lesser General Public License 2.1 或更高版本；安装目录 `tools/ffmpeg` 保留构建包附带的许可证与说明文件。
- FFmpeg 项目与源码：https://ffmpeg.org/ ；构建脚本与对应补丁：https://github.com/BtbN/FFmpeg-Builds

## 用户自行安装的插件

SWAM 和其他 VST3 音源、效果器不随风吟提供。用户必须自行获得相应插件授权。

## LibVLC 3.0.24 (VideoLAN)

- Windows 伴奏由 LibVLC 直接播放原始文件；不再经网页解码、WAV 提取或后台视频转码。
- 使用 VideoLAN 官方 `VideoLAN.LibVLC.Windows` 3.0.24 LGPL-2.1-or-later 包，非另行发布的 `.GPL` 插件包；以独立 DLL 动态加载，未修改 LibVLC。
- 官方项目：https://code.videolan.org/videolan/libvlc-nuget ；API：https://www.videolan.org/vlc/libvlc.html
- `components/VLC/COPYING.txt` 保留 LGPL 文本，`components/VLC/sdk/` 保留上游许可及公开头文件，`components/VLC/source/` 随附对应上游 VLC 源码；下载来源与校验值记录在 `fengyin-source.json`。
- 可替换兼容 ABI 的 LibVLC DLL/插件。风吟许可不限制为修改、调试这些 LGPL 组件而进行的替换及必要逆向工程。第三方组件以其自身许可为准，不提供额外保证。

## BLE MIDI packet codec

Microsoft Windows MIDI Services BLE MIDI 1.0 codec, copyright Microsoft Corporation, MIT License. Only the standalone source codec is used; no preview transport, driver or service binaries are distributed. Full license: components/BluetoothMIDI/LICENSE.txt. Source: https://github.com/microsoft/MIDI/blob/main/src/in-box/Transport/BluetoothMidiTransport/midi_ble_midi1_codec.h
