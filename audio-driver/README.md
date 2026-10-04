# 风吟固定虚拟音频设备

该目录实现安装时创建、运行期固定不变的“风吟共享扬声器”。普通 Windows 应用把共享混音写入该端点，`FengYinAudioEngine.exe` 读取它的回环流；电吹管音源走独立共享内存快速通道，因此网页周期不会增加乐器延迟。

## 驱动基线

- 上游：Microsoft `Windows-driver-samples/audio/sysvad`
- 固定提交：`2dc3fd3a0cc84a2933f2194e7ec0871584979071`
- 上游许可：Microsoft Public License；分发时保留原版权和完整许可证。
- 只保留一个 48 kHz、双声道、16-bit PCM 的 Speaker WaveRT 端点及其 loopback pin；不安装示例麦克风、HDMI、Bluetooth、APO 或测试音。

## 与原 SysVAD 的关键差异

1. Render DMA 已消费的数据写入 `FengYinAudioRing`，而不是写测试 WAV 文件。
2. Loopback capture 从同一环读取真实系统混音，不生成正弦测试音。
3. 环满时丢弃最旧系统音频，环空时补零；任何情况下不阻塞 DPC/音频线程。
4. 固定格式避免在内核中重采样；跨物理声卡时钟漂移由用户态引擎的有界 ASRC 修正。
5. INF 使用风吟自己的硬件 ID、设备名称、Provider、Class GUID 与升级版本；正式安装包只携带微软签名后的 `.cat/.inf/.sys`。

`apply-fengyin.patch` 已锁定上述微软提交，准备脚本会在干净源码上先执行 `git apply --check`。GitHub 上的驱动工作流只用于编译验证和产出内部测试件，不会把未签名驱动并入用户安装包。

## 安全门槛

- 驱动构建必须经过 Static Driver Verifier、CodeQL/编译器警告、Driver Verifier 和 HLK/实机压力验证。
- 开发包只能在隔离 Windows 测试机使用测试签名；不允许安装到用户日常电脑。
- 正式包必须通过 Microsoft Partner Center 签名。安装器发现签名无效时立即终止，不能自动开启 Windows 测试模式。
- 卸载或升级前先恢复原默认端点；驱动删除失败时保留恢复数据并提示重启，不能强删设备。

## 正式签名与发布交接

1. 驱动工作流会同时生成内部编译件和 `fengyin-audio-driver-partner-center-submission-unsigned`。后者包含 Partner Center 要求的独立子目录、INF、SYS、CAT 和 PDB。
2. 在保管 EV/Authenticode 证书的 Windows 电脑上对 CAB 做 SHA-256 时间戳签名，再上传 Microsoft Hardware Dev Center。证书私钥不进入源码仓库或 GitHub Actions。
3. 下载微软返回的 ZIP 后运行：

   ```powershell
   ./scripts/import-signed-driver.ps1 -SignedPackage "C:\下载\微软返回包.zip"
   ```

4. 导入脚本只接受固定硬件 ID `Root\FengYinAudioEngine`，并验证 CAT 和 SYS 均由 Microsoft Windows Hardware Compatibility Publisher 签名、目录哈希匹配且不含 WDK 测试证书。
5. 再运行 `scripts/build-windows.ps1`。只有上述验证全部通过，安装包才会携带驱动；否则继续生成安全的共享模式版本。

微软官方流程要求 CAB 内每个驱动包位于独立子目录，并建议附 PDB；Hardware Dev Center 会重新生成 CAT。参见：

- https://learn.microsoft.com/windows-hardware/drivers/dashboard/code-signing-attestation
- https://learn.microsoft.com/windows-hardware/drivers/dashboard/code-signing-reqs
