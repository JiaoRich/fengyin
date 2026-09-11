param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir
)

$ErrorActionPreference = "Stop"
$RequiredFiles = @("FengYin.exe", "README.md", "THIRD_PARTY_NOTICES.md", "MicrosoftEdgeWebview2Setup.exe")
foreach ($Name in $RequiredFiles) {
    $Path = Join-Path $PackageDir $Name
    if (-not (Test-Path $Path)) { throw "发布检查失败，缺少：$Name" }
}

$Forbidden = Get-ChildItem $PackageDir -Recurse -File | Where-Object {
    $_.Name -match "private-key|license-private|\.pfx$|\.pem$"
}
if ($Forbidden) {
    throw "发布检查失败：客户目录中发现疑似私钥文件：$($Forbidden.FullName -join ', ')"
}

$Size = (Get-ChildItem $PackageDir -Recurse -File | Measure-Object Length -Sum).Sum
if ($Size -lt 1MB) { throw "发布检查失败：客户程序目录体积异常。" }

Write-Host "发布检查通过：主程序、说明文件齐全，客户目录未发现私钥。" -ForegroundColor Green
