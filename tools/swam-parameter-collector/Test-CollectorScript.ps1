param([Parameter(Mandatory=$true)][string]$Collector)
$ErrorActionPreference = 'Stop'
$tokens = $null; $parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'Collect.ps1'), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -gt 0) { throw ($parseErrors | Out-String) }
$fn = $ast.Find({param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Find-SwamPlugins'}, $true)
if (!$fn) { throw 'Scan function missing' }
. ([scriptblock]::Create($fn.Extent.Text))
$testRoot = Join-Path $env:TEMP ('swam-scan-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $testRoot 'SWAM\SWAM Bundle.vst3\Contents\x86_64-win') -Force | Out-Null
New-Item -ItemType File -Path (Join-Path $testRoot 'SWAM\SWAM Bundle.vst3\Contents\x86_64-win\SWAM Bundle.vst3') | Out-Null
New-Item -ItemType File -Path (Join-Path $testRoot 'SWAM\SWAM Flat.vst3') | Out-Null
New-Item -ItemType File -Path (Join-Path $testRoot 'Unrelated.vst3') | Out-Null
$paths = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$scanWarnings = New-Object 'System.Collections.Generic.List[string]'
Find-SwamPlugins $testRoot
Find-SwamPlugins $testRoot
if ($paths.Count -ne 2) { throw ('Expected two deduplicated candidates, got ' + $paths.Count) }
if (@($paths | Where-Object { $_ -match 'Contents' }).Count -gt 0) { throw 'Inner bundle binary duplicated' }
$p = New-Object System.Diagnostics.Process
$p.StartInfo.FileName = (Resolve-Path $Collector).Path
$p.StartInfo.UseShellExecute = $false
$p.StartInfo.RedirectStandardOutput = $true
$p.StartInfo.RedirectStandardError = $true
[void]$p.Start()
$o=$p.StandardOutput.ReadToEndAsync();$e=$p.StandardError.ReadToEndAsync()
if (!$p.WaitForExit(30000)) { $p.Kill(); throw 'Executable startup timeout' }
$p.WaitForExit()
$exitCode=$p.ExitCode
$stdout=$o.GetAwaiter().GetResult();$stderr=$e.GetAwaiter().GetResult()
$p.Dispose()
if ($exitCode -ne 2 -or $stderr -notmatch 'Usage:') { throw ('Bad native startup/exit code: ' + $exitCode + ' ' + $stderr) }
Write-Host 'PASS: Windows PowerShell syntax; recursive scan; VST3 bundle deduplication; unrelated filter; native executable startup; non-null exit code.'
Write-Host 'This does not validate SWAM plugin loading or parameter values on the target computer.'
