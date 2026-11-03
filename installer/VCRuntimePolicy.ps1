# Shared, side-effect-free decision policy, used by the actual installer and tests.
function Get-VCRuntimeAction {
    param([version]$Required, [version]$Installed, [bool]$Healthy)
    if ($null -eq $Installed -or $Installed -lt $Required) { return 'install' }
    if ($Healthy) { return 'skip' }
    if ($Installed -eq $Required) { return 'repair' }
    return 'newer-needs-repair'
}

function Get-VCRuntimeExitAction {
    param([int]$Code, [bool]$Verified, [bool]$Registered)
    if ($Verified -and $Code -in @(0, 3010, 1638, -2147023258)) {
        if ($Code -eq 3010) { return 'restart' }
        return 'complete'
    }
    if ($Code -eq 3010 -and $Registered) { return 'restart-before-retry' }
    return 'failed'
}
