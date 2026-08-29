[CmdletBinding()]
param(
    [string]$GameBin
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'WardrobePackage.Common.ps1')

Assert-ZeroCompanyStopped
$resolvedGameBin = Resolve-WardrobeGameBin -GameBin $GameBin
$exePath = Join-Path $resolvedGameBin 'SWZeroCompany.exe'
$ue4ssPath = Join-Path $resolvedGameBin 'ue4ss\UE4SS.dll'
$modsPath = Join-Path $resolvedGameBin 'ue4ss\Mods\mods.txt'
$dllPath = Join-Path $resolvedGameBin 'ue4ss\Mods\ZeroCompanyMandoWardrobe\dlls\main.dll'

$exeHash = Assert-KnownFileHash -Path $exePath -ExpectedSha256 $script:RetailExeSha256 -Label 'retail executable'
$ue4ssHash = Assert-KnownFileHash -Path $ue4ssPath -ExpectedSha256 $script:CompatibilityUe4ssSha256 -Label 'compatibility UE4SS'
$dllHash = Assert-KnownFileHash -Path $dllPath -ExpectedSha256 $script:WardrobeDllSha256 -Label 'installed wardrobe DLL'
if (-not (Test-Path -LiteralPath $modsPath -PathType Leaf)) {
    throw "UE4SS mods.txt is missing: $modsPath"
}

$entries = @(Get-Ue4ssModEntries -ModsPath $modsPath)
$duplicates = @($entries | Group-Object { $_.Name.ToLowerInvariant() } | Where-Object Count -gt 1)
if ($duplicates.Count -ne 0) {
    throw "mods.txt contains duplicate activation keys: $(($duplicates | ForEach-Object Name) -join ', ')"
}
$wardrobeEntries = @($entries | Where-Object {
    $_.Name.Equals($script:WardrobeModName,[StringComparison]::OrdinalIgnoreCase)
})
if ($wardrobeEntries.Count -ne 1 -or $wardrobeEntries[0].Value -ne 1) {
    throw "Strict wardrobe preflight requires exactly one '$script:WardrobeModName : 1' entry."
}
$otherEnabled = @($entries | Where-Object {
    $_.Value -eq 1 -and -not $_.Name.Equals($script:WardrobeModName,[StringComparison]::OrdinalIgnoreCase)
})
if ($otherEnabled.Count -ne 0) {
    throw "Strict wardrobe isolation refused because other mods are enabled: $(($otherEnabled | ForEach-Object Name) -join ', ')"
}

Write-Host "PASS retail executable $exeHash"
Write-Host "PASS compatibility UE4SS $ue4ssHash"
Write-Host "PASS proven wardrobe DLL $dllHash"
Write-Host 'PASS exact wardrobe-only activation: 1 of 1 enabled entry'
Write-Host 'PREFLIGHT COMPLETE - the wardrobe is the only enabled UE4SS mod.'
