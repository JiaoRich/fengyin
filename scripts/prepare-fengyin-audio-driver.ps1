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
    # The upstream revision is pinned. Apply checked textual edits instead of
    # relying on git-apply across Windows checkout/line-ending policies. Every
    # edit must match exactly once, otherwise preparation stops before build.
    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    function Edit-PinnedFile([string]$target, [array]$edits) {
        $targetPath = Join-Path $output $target
        $text = [System.IO.File]::ReadAllText($targetPath).Replace("`r`n", "`n")
        foreach ($edit in $edits) {
            if ($edit.Count -ne 2) { throw "驱动修改参数不完整：$target" }
            $old = [string]$edit[0]
            $new = [string]$edit[1]
            $first = $text.IndexOf($old, [System.StringComparison]::Ordinal)
            $last = $text.LastIndexOf($old, [System.StringComparison]::Ordinal)
            if ($first -lt 0 -or $first -ne $last) {
                throw "固定 SysVAD 源码不符合预期，无法安全修改：$target"
            }
            $text = $text.Replace($old, $new)
        }
        [System.IO.File]::WriteAllText($targetPath, $text, $utf8NoBom)
    }

    Edit-PinnedFile 'audio\sysvad\EndpointsCommon\minwavertstream.cpp' @(
        @('#include "AudioModuleHelper.h"', "#include `"AudioModuleHelper.h`"`n#include `"../FengYinAudioRing.h`""),
        @('        if (!g_DoNotCreateDataFiles)
        {
            // Read from buffer and write to a file.
            ReadBytes(ByteDisplacement);
        }', '        // Forward every consumed render byte to the fixed loopback cable.
        ReadBytes(ByteDisplacement);'),
        @('        ULONG runWrite = min(ByteDisplacement, m_ulDmaBufferSize - bufferOffset);
            m_ToneGenerator.GenerateSine(m_pDmaBuffer + bufferOffset, runWrite);', '        ULONG runWrite = min(ByteDisplacement, m_ulDmaBufferSize - bufferOffset);
        if (m_pMiniport->IsLoopbackPin(m_ulPin))
            FengYinAudioRingRead(m_pDmaBuffer + bufferOffset, runWrite);
        else
            m_ToneGenerator.GenerateSine(m_pDmaBuffer + bufferOffset, runWrite);'),
        @('        m_SaveData.WriteData(m_pDmaBuffer + bufferOffset, runWrite);', '        FengYinAudioRingWrite(m_pDmaBuffer + bufferOffset, runWrite);
        if (!g_DoNotCreateDataFiles)
            m_SaveData.WriteData(m_pDmaBuffer + bufferOffset, runWrite);'),
        @('        if (!NT_SUCCESS(ntStatus))
        {
            return ntStatus;
        }
    }
    else if (!g_DoNotCreateDataFiles)', '        if (!NT_SUCCESS(ntStatus))
        {
            return ntStatus;
        }
        if (m_pMiniport->IsLoopbackPin(Pin_))
        {
            FengYinAudioRingResetReader();
        }
    }
    else if (!g_DoNotCreateDataFiles)')
    )
    Edit-PinnedFile 'audio\sysvad\EndpointsCommon\speakerwavtable.h' @(
        @('{ // 0 : First entry in this table is the default format for the audio engine
        {
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,
                44100,
                176400,', '{ // 0 : First entry in this table is the default format for the audio engine
        {
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,
                48000,
                192000,')
    )
    Edit-PinnedFile 'audio\sysvad\TabletAudioSample\TabletAudioSample.vcxproj' @(
        @('    <ClCompile Include="..\common.cpp" />', "    <ClCompile Include=`"..\common.cpp`" />`n    <ClCompile Include=`"..\FengYinAudioRing.cpp`" />")
    )
    Edit-PinnedFile 'audio\sysvad\TabletAudioSample\minipairs.h' @(
        @('    &SpeakerMiniports,
    &SpeakerHpMiniports,
    &HdmiMiniports,
    &SpdifMiniports,', '    &SpeakerMiniports,'),
        @('    &MicInMiniports,
    &MicArray1Miniports,
    &MicArray2Miniports,
    &MicArray3Miniports,', '    nullptr,'),
        @('#define g_cCaptureEndpoints (SIZEOF_ARRAY(g_CaptureEndpoints))', '#define g_cCaptureEndpoints 0')
    )
    Edit-PinnedFile 'audio\sysvad\common.cpp' @(
        @('#include "simple.h"', "#include `"simple.h`"`n#include `"FengYinAudioRing.h`""),
        @('    ASSERT(DeviceObject);', "    ASSERT(DeviceObject);`n    FengYinAudioRingInitialize();")
    )
}
finally {
    Pop-Location
}

Write-Host "风吟驱动源码已准备：$output\audio\sysvad"
