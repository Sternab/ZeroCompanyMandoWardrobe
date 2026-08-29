[CmdletBinding()]
param(
    [string]$GameBin
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'WardrobePackage.Common.ps1')

Assert-ZeroCompanyStopped
$resolvedGameBin = Resolve-WardrobeGameBin -GameBin $GameBin
$dllPath = Join-Path $resolvedGameBin 'ue4ss\Mods\ZeroCompanyMandoWardrobe\dlls\main.dll'
$logPath = Join-Path $resolvedGameBin 'ue4ss\UE4SS.log'

[void](Assert-KnownFileHash -Path $dllPath -ExpectedSha256 $script:WardrobeDllSha256 -Label 'installed wardrobe DLL')
if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    throw "UE4SS.log is missing: $logPath"
}

$modLines = @(Get-Content -LiteralPath $logPath | Where-Object { $_ -match '\[ZeroCompanyMandoWardrobe\]' })
if ($modLines.Count -eq 0) {
    throw 'No ZeroCompanyMandoWardrobe events were found in the current UE4SS.log.'
}

$mainReady = @($modLines | Where-Object {
    $_ -match 'READY hooks_active=true' -and $_ -match 'face_visibility_mutation=false'
}).Count
$fitReady = @($modLines | Where-Object { $_ -match 'helmet_fit_READY' }).Count
$catalogue = @($modLines | Where-Object { $_ -match '(catalogue_tile_added|compatibility_applied)' }).Count
$voice = @($modLines | Where-Object { $_ -match 'helmet_voice' -and $_ -match '(applied|resolved)' }).Count
$fitApply = @($modLines | Where-Object { $_ -match 'helmet_fit_apply_complete' }).Count
$settle = @($modLines | Where-Object { $_ -match 'helmet_fit_settle_complete' }).Count
$pivot = @($modLines | Where-Object { $_ -match 'helmet_pivot_compensation' -and $_ -match 'updates=[1-9][0-9]*' }).Count
$isolatedRefusals = @($modLines | Where-Object {
    $_ -match 'helmet_fit_bounded_scan' -and $_ -match 'component_refusals=[1-9][0-9]*'
}).Count
$rollback = @($modLines | Where-Object {
    $_ -match 'helmet_fit_bounded_scan' -and $_ -match 'rollback_restored=[1-9][0-9]*'
}).Count
$refused = @($modLines | Where-Object { $_ -match '(?:^|\s)(?:helmet_fit_)?REFUSED(?:\s|$)' }).Count
$faceMutation = @($modLines | Where-Object {
    $_ -match 'face_(?:component_)?visibility_(?:applied|mirror|hidden)'
}).Count

$diagnosticRoot = Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)) 'ZeroCompanyMandoWardrobe\Diagnostics'
New-Item -ItemType Directory -Path $diagnosticRoot -Force | Out-Null
$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
$evidencePath = Join-Path $diagnosticRoot "wardrobe-runtime-$stamp.txt"
[IO.File]::WriteAllLines($evidencePath,$modLines,[Text.UTF8Encoding]::new($false))
$evidenceHash = (Get-FileHash -LiteralPath $evidencePath -Algorithm SHA256).Hash

if ($mainReady -eq 0 -or $fitReady -eq 0) {
    $result = 'PARTIAL_INTEGRATED_WARDROBE_READY_GATE_MISSING'
} elseif ($refused -ne 0 -or $rollback -ne 0 -or $faceMutation -ne 0) {
    $result = 'PARTIAL_INTEGRATED_WARDROBE_REFUSED_OR_ROLLED_BACK'
} elseif ($fitApply -eq 0 -or $pivot -eq 0) {
    $result = 'PARTIAL_INTEGRATED_WARDROBE_READY_NO_MAN001_MAN002_FIT_OBSERVED'
} else {
    $result = 'PASS_INTEGRATED_MANDALORIAN_WARDROBE_RUNTIME'
}

Write-Host "RESULT: $result"
Write-Host "Wardrobe/fit READY events: $mainReady/$fitReady"
Write-Host "Catalogue compatibility events: $catalogue"
Write-Host "Helmet voice events: $voice"
Write-Host "Helmet-fit apply/settle events: $fitApply/$settle"
Write-Host "Animated pivot-compensation summaries: $pivot"
Write-Host "Isolated component-refusal events: $isolatedRefusals"
Write-Host "Rollback/refused events: $rollback/$refused"
Write-Host "Unexpected face-visibility mutation events: $faceMutation"
Write-Host "Evidence: $evidencePath"
Write-Host "Evidence SHA-256: $evidenceHash"

if (-not $result.StartsWith('PASS_', [StringComparison]::Ordinal)) {
    exit 2
}
