风吟 VB-CABLE 替代测试版

本包不安装驱动、不卸载原来的风吟共享扬声器，不修改测试签名或安全启动。
演奏仍使用已验证的 ASIO4ALL；只有浏览器声音改走 VB-CABLE。

1. 单独从 https://vb-audio.com/Cable/ 下载基础版 VB-CABLE（不是 A+B、C+D、Voicemeeter）。
   解压全部文件，右键 VBCABLE_Setup_x64.exe，以管理员身份运行，安装后重启。
2. 先保留你已经验证能出声的 ASIO4ALL 版本和设置，不同时换版本。
3. 关闭风吟，将 Windows 默认输出暂设为实际 Realtek 扬声器/耳机。
4. 完整解压本测试包，双击 Start-VBCable-Test.cmd，不要用桌面旧快捷方式。
5. 声音设置选择“风吟低延迟（ASIO4ALL）”。正常启动后会自动把系统输出切到 CABLE Input。
   ASIO4ALL 只启用实际 Realtek 输出；不要启用 CABLE、风吟共享扬声器或麦克风。
6. 播放网页视频并吹奏。若浏览器之前指定了某个输出，请把浏览器输出恢复“默认”。
   不要打开 CABLE Output 的“侦听此设备”，否则可能重复出声或产生回授。
7. 正常退出风吟后恢复之前的 Windows 默认输出。若异常退出未恢复，可手动选回 Realtek。

若提示 CABLE Output 无法打开：在 Windows 隐私设置中允许桌面应用访问麦克风。
本程序只采集 VB-CABLE 虚拟录音端点，不采集实际麦克风。
首次连接可出现短暂静音；建议先降低音量再测试。

回到原方案：关闭测试版，正常启动 FengYin.exe（不通过此 CMD）。无需卸载 VB-CABLE。
诊断日志中 SystemAudio=VB-CABLE 表示本次使用替代路径。
测试重点：风吟和网页同时有声、无爆音、退出后浏览器仍可正常播放。
界面延迟仍为估计值，不是实测端到端延迟。本版本尚需 Windows 实机验证。
