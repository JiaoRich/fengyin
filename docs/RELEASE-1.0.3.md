# 1.0.3 ASIO 桥接测试版

本版只验证“风吟低延迟 ASIO＋Windows 网页伴奏同时发声”的技术路线，不将第三方虚拟音频驱动打包到风吟安装包。

## 安全边界

- 新安装和自动优化仍然使用 Windows 共享输出。
- 只有检测到 `Synchronous Audio Router` 时才显示 ASIO 桥接选项。
- 风吟不会把 ASIO4ALL、声卡原生 ASIO 或其他 ASIO 驱动误当作桥接器。
- 桥接初始化失败时，下次启动自动回到 Windows 共享输出。
- 退出或卸载 SAR 前，先在风吟声音设置中切回 Windows 共享模式。

## Windows 实机准备

1. 安装 ASIO4ALL。
2. 从官方项目安装 Synchronous Audio Router 0.13.1（签名版）：
   <https://github.com/eiz/SynchronousAudioRouter/releases/tag/0.13.1>
3. 在 SAR Configuration 中将底层 Hardware Interface 设为 ASIO4ALL，并新建一个 Playback 端点，建议命名为“风吟网页伴奏”。
4. 在 ASIO4ALL 面板中选择实际的 Realtek 耳机/扬声器，先测试 48 kHz / 128，爆音则改为 256。
5. 将 Windows 或浏览器的输出选为“风吟网页伴奏”。
6. 打开风吟声音设置，手动选择“桥接低延迟测试（ASIO）”和 `Synchronous Audio Router`。

## 验收记录

- 记录桥接模式显示的采样率、缓冲区和预计输出延迟。
- 先只吹奏 5 分钟，确认无爆音、丢音、卡音。
- 再播放网页动态谱 15 分钟，确认伴奏和风吟同一耳机发声。
- 检查伴奏是否渐进偏移、爆音，风吟吹奏手感是否低于原 512 共享模式。
- 切回 Windows 共享模式后，浏览器和风吟应继续正常发声。
