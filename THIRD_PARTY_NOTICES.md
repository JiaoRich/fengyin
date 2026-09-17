# 第三方组件说明

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
