$ErrorActionPreference='Stop'
function Checked([scriptblock]$Run) { & $Run; if($LASTEXITCODE -ne 0){throw "Command failed: $LASTEXITCODE"} }
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$work=Join-Path $root 'build-sf2-installed'
$src=Join-Path $work 'source'
New-Item -ItemType Directory -Force -Path $src | Out-Null
foreach($name in @('native','tests','cmake','third_party')) { New-Item -ItemType Directory -Force -Path (Join-Path $src $name) | Out-Null }
Copy-Item "$root/native/Sf2" "$src/native/Sf2" -Recurse -Force
New-Item -ItemType Directory -Force -Path "$src/native/Source" | Out-Null
foreach($name in @('VideoFileChooser.h','NativeVideoFileDialog.h','VideoHostRendering.h')) {Copy-Item "$root/native/Source/$name" "$src/native/Source/$name" -Force}
foreach($name in @('test_sf2_installed_editor.cpp','NativeVideoDialogInput.h')) {Copy-Item "$root/tests/$name" "$src/tests/$name" -Force}
Copy-Item "$root/cmake/FengYinSf2.cmake" "$src/cmake/FengYinSf2.cmake" -Force
Copy-Item "$root/third_party/sf2" "$src/third_party/sf2" -Recurse -Force
@'
cmake_minimum_required(VERSION 3.22)
project(FengYinSf2Installed VERSION 1.1.48 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(/utf-8)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.2 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
set(BUILD_TESTING OFF)
juce_add_console_app(FengYin PRODUCT_NAME FengYinSf2InstalledTest)
target_sources(FengYin PRIVATE tests/test_sf2_installed_editor.cpp)
target_compile_definitions(FengYin PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1)
target_link_libraries(FengYin PRIVATE juce::juce_audio_utils juce::juce_cryptography ole32 oleaut32 shell32 uuid uiautomationcore)
include(cmake/FengYinSf2.cmake)
'@ | Set-Content "$src/CMakeLists.txt" -Encoding utf8
# Compile the exact released editor in the real DLL, not an equivalent test stub.
Checked {git fetch --depth=1 origin afa674127fc90edc2356724d6f5048c58b9741bb}
$released=git show 'afa674127fc90edc2356724d6f5048c58b9741bb:native/Sf2/Sf2Editor.cpp'
if($LASTEXITCODE){throw 'Cannot read released editor baseline'}
$released | Set-Content "$src/native/Sf2/Sf2Editor.cpp" -Encoding utf8
Checked {cmake -S $src -B "$work/build" -G 'Visual Studio 17 2022' -A x64}
Checked {cmake --build "$work/build" --config Release --target FengYin --parallel 4}
$built=Join-Path $work 'build/FengYin_artefacts/Release'
$isolated=Join-Path $work 'isolated'
New-Item -ItemType Directory -Force -Path $isolated | Out-Null
Copy-Item "$built/*" $isolated -Recurse -Force
$fixture=Join-Path $work '音源 中文测试.sf2'
Checked {python "$root/tests/create_sf2_fixture.py" $fixture}
$exe=Join-Path $isolated 'FengYin.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Built regression executable missing'}
$plugin=Join-Path $isolated 'components/SF2/FengYin SF2.vst3'
$oldHash=(Get-FileHash "$plugin/Contents/x86_64-win/FengYin SF2.vst3" -Algorithm SHA256).Hash
$neutral=Join-Path $work 'neutral-working-directory'
New-Item -ItemType Directory -Force -Path $neutral | Out-Null
Push-Location $neutral
try {
    Checked {& $exe $plugin $fixture --expect-picker-failure}
} finally {Pop-Location}
Get-Content "$root/native/Sf2/Sf2Editor.cpp" -Raw | Set-Content "$src/native/Sf2/Sf2Editor.cpp" -Encoding utf8
Checked {cmake --build "$work/build" --config Release --target FengYin --parallel 4}
Copy-Item "$built/*" $isolated -Recurse -Force
# The host EXE may be up to date, so its POST_BUILD copy need not rerun.
$freshBundle=Join-Path $work 'build/FengYinSF2_artefacts/Release/VST3/FengYin SF2.vst3'
Copy-Item "$freshBundle/*" $plugin -Recurse -Force
$newHash=(Get-FileHash "$plugin/Contents/x86_64-win/FengYin SF2.vst3" -Algorithm SHA256).Hash
if($newHash -eq $oldHash){throw 'Repaired DLL was not rebuilt; refusing stale payload'}
Push-Location $neutral
try { Checked {& $exe $plugin $fixture} } finally {Pop-Location}
# Test the shipped application's scan entry, outside the build tree/CWD.
$actual=Join-Path $work 'production-entry'
New-Item -ItemType Directory -Force -Path $actual | Out-Null
Checked {gh run download 45835303551 --repo JiaoRich/fengyin --name FengYin-performance-runtime --dir "$work/released-runtime"}
Copy-Item "$work/released-runtime/FengYin.exe" "$actual/FengYin.exe"
Copy-Item "$isolated/components" "$actual/components" -Recurse
$actualPlugin=Join-Path $actual 'components/SF2/FengYin SF2.vst3'
function Scan([string]$Name) {
    $report=Join-Path $actual ($Name+'.xml')
    $p=Start-Process "$actual/FengYin.exe" -ArgumentList ('--scan-plugin "'+$actualPlugin+'" "'+$report+'"') -WorkingDirectory $neutral -PassThru
    if(!$p.WaitForExit(45000)){$p.Kill();throw 'Production scan timed out'}
    if(!(Test-Path $report)){throw 'No production scan report'}
    return [xml](Get-Content $report -Raw)
}
$missing=Scan 'before-dependency'
if(@($missing.PLUGIN_SCAN.PLUGIN | Where-Object {$_.name -eq 'FengYin SF2'}).Count){throw 'Missing dependency regression did not reproduce'}
Copy-Item "$isolated/libfluidsynth-3.dll" "$actual/libfluidsynth-3.dll"
$present=Scan 'after-dependency'
if($present.PLUGIN_SCAN.validation -ne 'complete' -or !@($present.PLUGIN_SCAN.PLUGIN | Where-Object {$_.name -eq 'FengYin SF2' -and $_.isInstrument -eq '1'}).Count){throw 'Installed production scan failed'}
$payload=Join-Path $work 'verified-component'
New-Item -ItemType Directory -Force -Path $payload | Out-Null
Copy-Item "$isolated/components/SF2" "$payload/SF2" -Recurse
Copy-Item "$isolated/libfluidsynth-3.dll" "$payload/libfluidsynth-3.dll"
Get-ChildItem $payload -File -Recurse | Get-FileHash -Algorithm SHA256 | ConvertTo-Json | Set-Content "$work/component-hashes.json"
Write-Host 'PASS installed dependency negative/positive, actual released EXE scan, dynamic VST3 old failure/new real UI and PCM/state tests'
