param([Parameter(Mandatory=$true)][string]$PackageDirectory,[string]$EvidenceFile)
$ErrorActionPreference='Stop'
$LogDir=Join-Path $env:ProgramData 'FengYinDJ/InstallLogs'
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$Log=Join-Path $LogDir 'FabFilter-Pro-R-2.txt'
function Find-ProR {
    foreach($Root in @($env:CommonProgramW6432,"$env:ProgramFiles/Common Files")) {
        if (-not $Root) {continue}
        foreach($Name in @('FabFilter/FabFilter Pro-R 2.vst3','FabFilter Pro-R 2.vst3')) {
            $Candidate=Join-Path (Join-Path $Root 'VST3') $Name
            if (Test-Path -LiteralPath $Candidate -PathType Container) { $Candidate=Join-Path $Candidate 'Contents/x86_64-win/FabFilter Pro-R 2.vst3' }
            if (-not (Test-Path -LiteralPath $Candidate -PathType Leaf)) {continue}
            $Stream=[IO.File]::OpenRead($Candidate)
            try {
                $Reader=New-Object IO.BinaryReader($Stream)
                if ($Reader.ReadUInt16() -ne 0x5a4d) {continue}
                $Stream.Position=0x3c;$Offset=$Reader.ReadInt32();$Stream.Position=$Offset
                if ($Reader.ReadUInt32() -ne 0x4550 -or $Reader.ReadUInt16() -ne 0x8664) {continue}
            } finally {$Stream.Dispose()}
            $V=(Get-Item -LiteralPath $Candidate).VersionInfo
            if ($V.FileMajorPart -ge 2) {return $Candidate}
        }
    }
    return $null
}
try {
    $Existing=Find-ProR
    $Action='existing-compatible-plugin'
    if (-not $Existing) {
        $Package=Join-Path $PackageDirectory 'ffpror206x64.exe'
        if ((Get-FileHash -LiteralPath $Package -Algorithm SHA256).Hash -ne 'D7FC0FB605CBA0B5D359E25CC497C99F292395285F7797964017C28CCC4A83A0') {throw 'Installer SHA256 mismatch'}
        $Signature=Get-AuthenticodeSignature -LiteralPath $Package
        if ($Signature.Status -ne 'Valid' -or $Signature.SignerCertificate.Subject -notmatch 'FabFilter') {throw 'Installer signature not verified'}
        $Process=Start-Process -FilePath $Package -ArgumentList '/Unattended' -PassThru
        if (-not $Process.WaitForExit(180000)) {throw 'FabFilter installer exceeded 180 seconds; finish the official installer before retrying'}
        if ($Process.ExitCode -ne 0) {throw "FabFilter installer exit=$($Process.ExitCode)"}
        $Existing=Find-ProR
        if (-not $Existing) {throw 'No compatible x64 Pro-R 2 VST3 found after official installer completed'}
        $Action='installed-official-package'
    }
    $Result=@{action=$Action;plugin=$Existing;version=(Get-Item -LiteralPath $Existing).VersionInfo.FileVersion;utc=[DateTime]::UtcNow.ToString('o');licenseIncluded=$false}
    $Result | ConvertTo-Json | Add-Content -LiteralPath $Log -Encoding UTF8
    if ($EvidenceFile) {$Result | ConvertTo-Json | Set-Content -LiteralPath $EvidenceFile -Encoding UTF8}
    exit 0
} catch {
    ($_ | Out-String) | Add-Content -LiteralPath $Log -Encoding UTF8
    Write-Error $_
    exit 1
}
