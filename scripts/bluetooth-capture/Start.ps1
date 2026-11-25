param([string]$BootstrapPath='', [switch]$NoElevation, [string]$OutputRoot='', [ValidateRange(5,1200)][int]$MaxSeconds=1200)
$ErrorActionPreference='Stop'
if([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or -not [Environment]::Is64BitProcess){throw 'Run Start.cmd on Windows 10/11 x64.'}
. (Join-Path $PSScriptRoot 'Common.ps1')
if($BootstrapPath){$launch=Get-Content -LiteralPath $BootstrapPath -Raw -Encoding UTF8 | ConvertFrom-Json}
else {
    if(-not $OutputRoot){$OutputRoot=[Environment]::GetFolderPath('Desktop')}
    if(-not $OutputRoot){$OutputRoot=$PSScriptRoot}
    $launch=[pscustomobject]@{OutputRoot=$OutputRoot;AppData=$env:APPDATA;LocalData=$env:LOCALAPPDATA;MaxSeconds=$MaxSeconds}
    $admin=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if(-not $admin -and -not $NoElevation){
        # Preserve original user's Desktop/AppData when UAC uses a different administrator account.
        $bootstrap=Join-Path ([IO.Path]::GetTempPath()) ('FengYinBleLaunch-'+[guid]::NewGuid().ToString('N')+'.json')
        Save-Json $launch $bootstrap
        $command="& '"+$PSCommandPath.Replace("'","''")+"' -BootstrapPath '"+$bootstrap.Replace("'","''")+"'"
        $pi=[Diagnostics.ProcessStartInfo]::new((Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'))
        $pi.Arguments='-NoLogo -NoProfile -STA -ExecutionPolicy Bypass -EncodedCommand '+[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
        $pi.UseShellExecute=$true;$pi.Verb='runas'
        try {$elevated=[Diagnostics.Process]::Start($pi);$elevated.Dispose();exit 0}catch {Write-Host '未获得管理员权限，继续普通采集；受限项目会标记。'}
    }
}
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$out=Join-Path $launch.OutputRoot ('FengYin-BLE-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6))
$null=New-Item -ItemType Directory -Path $out
$roots=@(foreach($base in @($launch.AppData,$launch.LocalData)){foreach($name in @('FengYin','FengYinDJ','FengYinToneTest')){Join-Path $base $name}})
$specPath=Join-Path $out 'session-input.json'
Save-Json ([ordered]@{Output=$out;LogRoots=$roots;Started=(Get-Date).ToString('o');MaxSeconds=$launch.MaxSeconds;ParentId=$PID;ParentStart=(Get-Process -Id $PID).StartTime.ToUniversalTime().ToFileTimeUtc()}) $specPath
$engine=Start-OwnedProcess (Join-Path $PSScriptRoot 'Engine.ps1') $specPath $out 'engine' 1380
$form=New-Object Windows.Forms.Form
$form.Text='YDS-150 蓝牙连接诊断';$form.StartPosition='CenterScreen';$form.Size=[Drawing.Size]::new(780,660);$form.MinimumSize=[Drawing.Size]::new(620,560)
$form.Font=[Drawing.Font]::new('Microsoft YaHei UI',10);$form.AutoScaleMode='Dpi'
$layout=[Windows.Forms.TableLayoutPanel]::new();$layout.Dock='Fill';$layout.AutoScroll=$true;$layout.Padding=[Windows.Forms.Padding]::new(18);$layout.ColumnCount=1;$layout.RowCount=7
foreach($height in @(48,130,78,55,75,50,65)){$null=$layout.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::Absolute,$height))}
$form.Controls.Add($layout)
function Label([string]$Text){$l=[Windows.Forms.Label]::new();$l.Text=$Text;$l.Dock='Fill';$l.AutoSize=$false;return $l}
$title=Label '正在记录蓝牙连接过程';$title.Font=[Drawing.Font]::new('Microsoft YaHei UI',16,[Drawing.FontStyle]::Bold);$layout.Controls.Add($title,0,0)
$help=Label "1. 若现在已连接失败，先点“标记连接失败”，暂时不要删除配对。`r`n2. 在风吟中重试一次；若能连接，正常退出风吟、重新打开，再连接一次。`r`n3. 如果仍失败，先标记；确有需要时再自行删除 Windows 配对，点“标记已删除配对”后重连。`r`n4. 点击“结束并打包”。不用反复试吹，最多记录20分钟。"
$layout.Controls.Add($help,0,1)
$panel=[Windows.Forms.FlowLayoutPanel]::new();$panel.Dock='Fill';$panel.WrapContents=$true
$buttons=@{}
foreach($item in @(@('failed','标记连接失败'),@('connected','标记连接成功'),@('disconnected','标记自行断连'),@('forgotten','标记已删除配对'))){
    $b=[Windows.Forms.Button]::new();$b.Text=$item[1];$b.Tag=$item[0];$b.AutoSize=$true;$b.Height=34;$b.Margin=[Windows.Forms.Padding]::new(3)
    $b.Add_Click({param($sender,$eventArgs)
        $cmd=Join-Path $out ('command-'+[guid]::NewGuid().ToString('N')+'.json')
        Save-Json @{Kind=[string]$sender.Tag;ClickedAt=(Get-Date).ToString('o');Note=$note.Text} $cmd
        $last.Text='已记录：'+$sender.Text+'  '+(Get-Date -Format 'HH:mm:ss')
    })
    $panel.Controls.Add($b);$buttons[$item[0]]=$b
};$layout.Controls.Add($panel,0,2)
$last=Label '只有你点击按钮才会标记；这些按钮不会连接或删除设备。';$layout.Controls.Add($last,0,3)
$note=[Windows.Forms.TextBox]::new();$note.Dock='Fill';$note.Multiline=$true;$note.ScrollBars='Vertical'
$note.AccessibleName='现场备注（可留空）';$note.Text='现场备注（可留空）：例如弹窗文字、是否连着手机或USB、是否删除配对后恢复。';$layout.Controls.Add($note,0,4)
$footer=[Windows.Forms.FlowLayoutPanel]::new();$footer.Dock='Fill'
$finish=[Windows.Forms.Button]::new();$finish.Text='结束并打包';$finish.Width=150;$finish.Height=36
$open=[Windows.Forms.Button]::new();$open.Text='打开报告位置';$open.Width=150;$open.Height=36;$open.Enabled=$false
$footer.Controls.AddRange([Windows.Forms.Control[]]@($finish,$open));$layout.Controls.Add($footer,0,5)
$status=Label '正在启动采集。报告只保存在这台电脑，不自动上传。';$layout.Controls.Add($status,0,6)
$script:closing=$false;$script:finished=$false
function Finish-Capture {
    if($script:closing -or $script:finished){return};$script:closing=$true
    Save-Json @{Kind='note';ClickedAt=(Get-Date).ToString('o');Note=$note.Text} (Join-Path $out ('command-'+[guid]::NewGuid().ToString('N')+'.json'))
    [IO.File]::WriteAllText((Join-Path $out 'finish.flag'),'finish')
    $finish.Enabled=$false;foreach($b in $buttons.Values){$b.Enabled=$false};$note.ReadOnly=$true
    $title.Text='正在打包，请稍候';$status.Text='正在保存最后的设备状态和系统事件。'
}
$finish.Add_Click({Finish-Capture})
$open.Add_Click({$p=[Diagnostics.ProcessStartInfo]::new('explorer.exe');$p.Arguments='"'+$launch.OutputRoot+'"';$p.UseShellExecute=$true;$null=[Diagnostics.Process]::Start($p)})
$form.Add_FormClosing({param($sender,$eventArgs)if(-not $script:finished){$eventArgs.Cancel=$true;Finish-Capture}})
$timer=[Windows.Forms.Timer]::new();$timer.Interval=500
$timer.Add_Tick({
    try {
        $statePath=Join-Path $out 'status.json'
        if(Test-Path -LiteralPath $statePath){
            try {$s=Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json}catch {return}
            $status.Text=$s.Message+'  （'+[int]$s.Elapsed+'秒）'
            if($s.Phase -in @('done','error','duplicate')){
                $script:finished=$true;$finish.Enabled=$false;$open.Enabled=($s.Phase -ne 'duplicate');foreach($b in $buttons.Values){$b.Enabled=$false}
                $title.Text=if($s.Phase -eq 'done'){'完成，请发送桌面的诊断ZIP'}else{$s.Message}
                $timer.Stop()
            }
        }
        if(-not $script:finished -and $engine.Process.HasExited){$script:finished=$true;$open.Enabled=$true;$finish.Enabled=$false;$title.Text='采集进程已结束';$status.Text='请打开报告位置，发送已有的诊断ZIP或同名文件夹。';$timer.Stop()}
    } catch {$status.Text='界面读取状态失败；已采集文件仍在桌面。'+$_.Exception.Message}
})
$timer.Start()
try {$null=$form.ShowDialog()}finally {$timer.Stop();$timer.Dispose();$form.Dispose();$engine.Process.Dispose()}
