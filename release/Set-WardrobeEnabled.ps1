[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('Enabled','Disabled')]
    [string]$State,
    [string]$GameBin = 'C:\Program Files (x86)\Steam\steamapps\common\Star Wars Zero Company\SWZeroCompany\Binaries\Win64',
    [string]$BackupRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'WardrobePackage.Common.ps1')

Assert-ZeroCompanyStopped
$resolvedGameBin = Resolve-WardrobeGameBin -GameBin $GameBin
if ([string]::IsNullOrWhiteSpace($BackupRoot)) {
    $BackupRoot = Get-WardrobeDefaultBackupRoot
}

$modsRoot = Join-Path $resolvedGameBin 'ue4ss\Mods'
$modsPath = Join-Path $modsRoot 'mods.txt'
if (-not (Test-Path -LiteralPath $modsRoot -PathType Container)) {
    throw "UE4SS Mods directory is missing: $modsRoot"
}

# Disabling must remain available after a game update, but enabling old native code
# against a different executable or loader is refused.
if ($State -eq 'Enabled') {
    Assert-KnownFileHash -Path (Join-Path $resolvedGameBin 'SWZeroCompany.exe') `
        -ExpectedSha256 $script:RetailExeSha256 -Label 'retail executable' | Out-Null
    Assert-KnownFileHash -Path (Join-Path $resolvedGameBin 'ue4ss\UE4SS.dll') `
        -ExpectedSha256 $script:CompatibilityUe4ssSha256 -Label 'compatibility UE4SS' | Out-Null
    Assert-KnownFileHash `
        -Path (Join-Path $modsRoot 'ZeroCompanyMandoWardrobe\dlls\main.dll') `
        -ExpectedSha256 $script:WardrobeDllSha256 -Label 'installed wardrobe DLL' | Out-Null
}

$backupDirectory = New-WardrobeBackupDirectory -BackupRoot $BackupRoot -Purpose 'manifest-change'
if (Test-Path -LiteralPath $modsPath -PathType Leaf) {
    Copy-Item -LiteralPath $modsPath -Destination (Join-Path $backupDirectory 'mods.txt')
}

Set-WardrobeManifestEntry -ModsPath $modsPath -Enabled ($State -eq 'Enabled')
Write-Host "PASS $script:WardrobeModName is $($State.ToLowerInvariant())"
Write-Host 'PASS comments, ordering, and unrelated mods.txt entries were preserved'
Write-Host "RECOVERABLE MANIFEST BACKUP $backupDirectory"
