$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class FengYinProbeWindow {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
}
'@
$Helper=Join-Path $PSScriptRoot 'FengYinScreenCaptureProbe.exe'
$Session=$null
$Capture=$null
function Invoke-CaptureProcess([string[]]$Arguments,[int]$Timeout) {
    # Own the original .NET process handle. Start-Process objects can lose the
    # exit code on Windows PowerShell 5.1 after a short-lived helper exits.
    $Process=New-Object System.Diagnostics.Process
    $Process.StartInfo.FileName=$Helper
    $Process.StartInfo.Arguments=(($Arguments | ForEach-Object {'"'+$_+'"'}) -join ' ')
    $Process.StartInfo.UseShellExecute=$false
    $Process.StartInfo.CreateNoWindow=$true
    try {
        if(-not $Process.Start()){throw '无法启动本次测试组件。'}
        if(-not $Process.WaitForExit($Timeout)) {
            $Process.Kill()
            $Process.WaitForExit()
            throw '本次测试组件超时，已只关闭该组件；风吟未关闭，临时文件保留。'
        }
        return [int]$Process.ExitCode
    } finally {$Process.Dispose()}
}
try {
    if(-not (Test-Path -LiteralPath $Helper)){throw '缺少录屏测试组件，请先完整解压测试包。'}
    $Apps=@(Get-Process -Name FengYin -ErrorAction SilentlyContinue | Where-Object {$_.MainWindowHandle -ne [IntPtr]::Zero})
    if($Apps.Count -ne 1){throw '请先打开一个风吟，选好音色与视频。不要同时打开多个风吟。'}
    $App=$Apps[0]
    $Answer=[Windows.Forms.MessageBox]::Show("这是独立录屏测试，不更新或替换风吟。`n`n点击确定后返回风吟：先正常吹奏约15秒，再自动录屏45秒。期间可切换全屏和精简模式。`n`n会录下该显示器全部内容，包括其他窗口和通知；录制声音为系统扬声器输出，不使用麦克风。`n`n请持续正常吹奏，测试结束会提示。",'风吟录屏性能验证',[Windows.Forms.MessageBoxButtons]::OKCancel)
    if($Answer -ne [Windows.Forms.DialogResult]::OK){exit 0}
    $Name='Screen-capture-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6)
    $Session=Join-Path $PSScriptRoot $Name
    [FengYinProbeWindow]::SetForegroundWindow($App.MainWindowHandle) | Out-Null
    Start-Sleep -Seconds 15
    if($App.HasExited){throw '风吟已关闭，未开始录屏。'}
    $Arguments=@('--capture',$App.MainWindowHandle.ToInt64().ToString('x'),$Session,[string]$App.Id,'45')
    # The helper starts its 45s deadline only after the first real video frame.
    # This independent watchdog protects the app if initialization/finalize hangs.
    $CaptureExit=Invoke-CaptureProcess $Arguments 80000
    if($CaptureExit -ne 0){
        $Detail=if(Test-Path -LiteralPath (Join-Path $Session 'state.txt')){Get-Content -LiteralPath (Join-Path $Session 'state.txt') -Raw -Encoding UTF8}else{'组件未生成状态记录'}
        throw "录屏未通过：$Detail`n正式版风吟与原有设置未改变。"
    }
    $Choice=[Windows.Forms.MessageBox]::Show("实时录屏测试结束，可以停止吹奏。`n`n接下来只合成刚才的视频与音频，不重新压缩视频。合成期间请暂不吹奏。`n`n点击确定开始合成；取消则保留原始音视频，暂不合成。",'实时测试结束',[Windows.Forms.MessageBoxButtons]::OKCancel)
    if($Choice -eq [Windows.Forms.DialogResult]::OK){
        $MuxExit=Invoke-CaptureProcess @('--mux-session',$Session) 120000
        if($MuxExit -ne 0){throw ('合成未通过，原始文件保留：'+(Get-Content -LiteralPath (Join-Path $Session 'state.txt') -Raw -Encoding UTF8))}
    }
    [Windows.Forms.MessageBox]::Show("测试文件已保存在：`n$Session`n`n请回报：录屏前后吹奏是否有差别、视频是否卡顿、回放声音是否正常。`n此测试包还不是正式录屏功能。",'测试完成') | Out-Null
    Start-Process explorer.exe -ArgumentList ('"{0}"' -f $Session)
} catch {
    [Windows.Forms.MessageBox]::Show($_.Exception.Message,'测试未通过',[Windows.Forms.MessageBoxButtons]::OK,[Windows.Forms.MessageBoxIcon]::Warning) | Out-Null
    exit 1
}
