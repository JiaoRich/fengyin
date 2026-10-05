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

    # The pinned sample tracks Microsoft's newest WDK.  The first trial was
    # consequently linked with WDK 28000 and failed to load on Windows 11
    # 21H2 (build 22000) with Code 39 / entry point not found.  Compile the
    # same source with the stable 26100 kit and explicitly keep the KMDF ABI
    # at the broadly supported 1.15 baseline.
    $packagesPath = Join-Path $output 'packages.config'
    $packages = [System.IO.File]::ReadAllText($packagesPath)
    $packages = $packages.Replace('10.0.28000.2526', '10.0.26100.1')
    [System.IO.File]::WriteAllText($packagesPath, $packages, [System.Text.UTF8Encoding]::new($true))
    $directoryPropsPath = Join-Path $output 'Directory.Build.props'
    $directoryProps = [System.IO.File]::ReadAllText($directoryPropsPath)
    $directoryProps = $directoryProps.Replace('10.0.28000.2526', '10.0.26100.1')
    # Set the WDK's official target-version property before its props are
    # imported.  Adding NTDDI_VERSION only to ClCompile is insufficient: the
    # WDK appends its own latest-version define afterwards and wins. CO is
    # Windows 11 21H2, the target machine's build 22000 API baseline.
    $directoryProps = $directoryProps.Replace(
        '</Project>',
        "  <PropertyGroup>`n    <_NT_TARGET_VERSION>0xA00000B</_NT_TARGET_VERSION>`n  </PropertyGroup>`n</Project>")
    [System.IO.File]::WriteAllText($directoryPropsPath, $directoryProps, [System.Text.UTF8Encoding]::new($true))

    Copy-Item (Join-Path $patchRoot 'FengYinAudioRing.h') 'audio\sysvad\FengYinAudioRing.h'
    Copy-Item (Join-Path $patchRoot 'FengYinAudioRing.cpp') 'audio\sysvad\FengYinAudioRing.cpp'
    # Replace the broad Microsoft sample package with one x64 render endpoint.
    # INF files containing Chinese text must be UTF-16LE for Windows SetupAPI.
    $minimalInf = [System.IO.File]::ReadAllText((Join-Path $patchRoot 'FengYinAudio.inx'),
                                                [System.Text.Encoding]::UTF8)
    [System.IO.File]::WriteAllText(
        (Join-Path $output 'audio\sysvad\TabletAudioSample\FengYinAudio.inx'),
        $minimalInf,
        [System.Text.Encoding]::Unicode)
    Remove-Item (Join-Path $output 'audio\sysvad\TabletAudioSample\ComponentizedAudioSample.inx') -Force
    Remove-Item (Join-Path $output 'audio\sysvad\TabletAudioSample\ComponentizedAudioSampleExtension.inx') -Force
    Remove-Item (Join-Path $output 'audio\sysvad\TabletAudioSample\ComponentizedApoSample.inx') -Force
    # The upstream revision is pinned. Apply checked textual edits instead of
    # relying on git-apply across Windows checkout/line-ending policies. Every
    # edit must match exactly once, otherwise preparation stops before build.
    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    function Edit-PinnedFile([string]$target, [array]$edits) {
        $targetPath = Join-Path $output $target
        $text = [System.IO.File]::ReadAllText($targetPath).Replace("`r`n", "`n")
        $text = [System.Text.RegularExpressions.Regex]::Replace($text, '[ \t]+(?=\n)', '')
        if ($edits.Count -eq 2 -and $edits[0] -is [string]) { $edits = ,$edits }
        $editNumber = 0
        foreach ($edit in $edits) {
            $editNumber++
            if ($edit.Count -ne 2) { throw "驱动修改参数不完整：$target" }
            $old = ([string]$edit[0]).Replace("`r`n", "`n")
            $new = ([string]$edit[1]).Replace("`r`n", "`n")
            $first = $text.IndexOf($old, [System.StringComparison]::Ordinal)
            $last = $text.LastIndexOf($old, [System.StringComparison]::Ordinal)
            if ($first -lt 0 -or $first -ne $last) {
                throw "固定 SysVAD 源码不符合预期，无法安全修改：$target，第 $editNumber 项，匹配 $(([regex]::Matches($text, [regex]::Escape($old))).Count) 次"
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
    # Jack description v3 was added in 22H2. Keep the existing v1/v2
    # handlers on 21H2 and compile v3 only when the target SDK exposes it.
    $jackV3Sections = @(
        @('mintopo.h', '(?ms)^    NTSTATUS PropertyHandlerJackDescription3\s*\(.*?^    \);'),
        @('mintopo.cpp', '(?ms)^#pragma code_seg\("PAGE"\)\r?\nNTSTATUS\r?\nCMiniportTopology::PropertyHandlerJackDescription3\b.*?^\}'),
        @('speakertopo.cpp', '(?ms)^        else if \(PropertyRequest->PropertyItem->Id == KSPROPERTY_JACK_DESCRIPTION3\).*?^        \}'),
        @('speakertoptable.h', '(?ms)^    \{\r?\n        &KSPROPSETID_Jack,\r?\n        KSPROPERTY_JACK_DESCRIPTION3,.*?^    \},')
    )
    foreach ($section in $jackV3Sections) {
        $relative = 'audio\sysvad\EndpointsCommon\' + $section[0]
        $sourceText = [IO.File]::ReadAllText((Join-Path $output $relative)).Replace("`r`n", "`n")
        $sourceText = [regex]::Replace($sourceText, '[ \t]+(?=\n)', '')
        $matches = [regex]::Matches($sourceText, $section[1])
        if ($matches.Count -ne 1) { throw "Jack v3 compatibility section mismatch: $relative" }
        $original = $matches[0].Value
        Edit-PinnedFile $relative @($original, "#if (NTDDI_VERSION >= NTDDI_WIN10_NI)`n$original`n#endif")
    }
    Edit-PinnedFile 'audio\sysvad\EndpointsCommon\EndpointsCommon.vcxproj' @(
        @('  <ItemDefinitionGroup Condition="''$(Configuration)|$(Platform)''==''Release|x64''">
    <ResourceCompile>
      <AdditionalIncludeDirectories>%(AdditionalIncludeDirectories);$(DDK_INC_PATH);..</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>%(PreprocessorDefinitions);_USE_WAVERT_;SYSVAD_BTH_BYPASS;SYSVAD_USB_SIDEBAND</PreprocessorDefinitions>
    </ResourceCompile>
    <ClCompile>
      <LanguageStandard>stdcpp17</LanguageStandard>
      <AdditionalIncludeDirectories>%(AdditionalIncludeDirectories);$(DDK_INC_PATH);..;.</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>%(PreprocessorDefinitions);_USE_WAVERT_;SYSVAD_BTH_BYPASS;SYSVAD_USB_SIDEBAND;_NEW_DELETE_OPERATORS_</PreprocessorDefinitions>',
          '  <ItemDefinitionGroup Condition="''$(Configuration)|$(Platform)''==''Release|x64''">
    <ResourceCompile>
      <AdditionalIncludeDirectories>%(AdditionalIncludeDirectories);$(DDK_INC_PATH);..</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>%(PreprocessorDefinitions);_USE_WAVERT_;SYSVAD_BTH_BYPASS;SYSVAD_USB_SIDEBAND</PreprocessorDefinitions>
    </ResourceCompile>
    <ClCompile>
      <LanguageStandard>stdcpp17</LanguageStandard>
      <AdditionalIncludeDirectories>%(AdditionalIncludeDirectories);$(DDK_INC_PATH);..;.</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>%(PreprocessorDefinitions);_USE_WAVERT_;SYSVAD_BTH_BYPASS;SYSVAD_USB_SIDEBAND;_NEW_DELETE_OPERATORS_</PreprocessorDefinitions>')
    )
    Edit-PinnedFile 'audio\sysvad\TabletAudioSample\TabletAudioSample.vcxproj' @(
        @('    <KMDF_VERSION_MAJOR>1</KMDF_VERSION_MAJOR>', "    <KMDF_VERSION_MAJOR>1</KMDF_VERSION_MAJOR>`n    <KMDF_VERSION_MINOR>15</KMDF_VERSION_MINOR>"),
        @('    <ClCompile Include="..\common.cpp" />', "    <ClCompile Include=`"..\common.cpp`" />`n    <ClCompile Include=`"..\FengYinAudioRing.cpp`" />"),
        @('  <ItemDefinitionGroup Condition="''$(Configuration)|$(Platform)''==''Release|x64''">
    <Link>
      <AdditionalDependencies>%(AdditionalDependencies);$(DDK_LIB_PATH)\portcls.lib;$(DDK_LIB_PATH)\stdunk.lib;$(DDK_LIB_PATH)\libcntpr.lib</AdditionalDependencies>', '  <ItemDefinitionGroup Condition="''$(Configuration)|$(Platform)''==''Release|x64''">
    <ClCompile>
      <PreprocessorDefinitions>%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <DisableSpecificWarnings>4296;%(DisableSpecificWarnings)</DisableSpecificWarnings>
    </ClCompile>
    <Link>
      <AdditionalDependencies>%(AdditionalDependencies);$(DDK_LIB_PATH)\portcls.lib;$(DDK_LIB_PATH)\stdunk.lib;$(DDK_LIB_PATH)\libcntpr.lib</AdditionalDependencies>')
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
    Edit-PinnedFile 'audio\sysvad\adapter.cpp' @(
        @('    for(ULONG i = 0; i < g_cCaptureEndpoints; ++i, ++ppAeMiniports)',
          '    for(ULONG i = 0; i != g_cCaptureEndpoints; ++i, ++ppAeMiniports)')
    )
}
finally {
    Pop-Location
}

Write-Host "风吟驱动源码已准备：$output\audio\sysvad"
