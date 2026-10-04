param(
    [string]$OutputDirectory = "$PSScriptRoot\..\build-driver-source"
)

$ErrorActionPreference = 'Stop'
$sourceCommit = '2dc3fd3a0cc84a2933f2194e7ec0871584979071'
$sourceRepository = 'https://github.com/microsoft/Windows-driver-samples.git'
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
$patchRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\audio-driver\sysvad-patch'))

if (Test-Path $output) {
    throw "目标目录已存在，为避免覆盖未知文件已停止：$output"
}

git clone --filter=blob:none --no-checkout $sourceRepository $output
if ($LASTEXITCODE -ne 0) { throw '下载微软驱动示例失败' }

Push-Location $output
try {
    git sparse-checkout init --cone
    git sparse-checkout set audio/sysvad
    git checkout $sourceCommit
    if ($LASTEXITCODE -ne 0) { throw '检出固定 SysVAD 版本失败' }

    Copy-Item (Join-Path $patchRoot 'FengYinAudioRing.h') 'audio\sysvad\FengYinAudioRing.h'
    Copy-Item (Join-Path $patchRoot 'FengYinAudioRing.cpp') 'audio\sysvad\FengYinAudioRing.cpp'
    git apply --check (Join-Path $patchRoot 'apply-fengyin.patch')
    if ($LASTEXITCODE -ne 0) { throw '风吟 SysVAD 补丁与固定上游版本不匹配' }
    git apply (Join-Path $patchRoot 'apply-fengyin.patch')
    if ($LASTEXITCODE -ne 0) { throw '应用风吟 SysVAD 补丁失败' }
}
finally {
    Pop-Location
}

Write-Host "风吟驱动源码已准备：$output\audio\sysvad"
