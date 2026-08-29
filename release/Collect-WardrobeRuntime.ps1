[CmdletBinding()]
param(
    [string]$GameBin
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'WardrobePackage.Common.ps1')

function Get-SummaryTotal {
    param(
        [Parameter(Mandatory=$true)][AllowEmptyCollection()][string[]]$Lines,
        [Parameter(Mandatory=$true)][string]$Field
    )

    [uint64]$total = 0
    $pattern = '(?:^|\s)' + [regex]::Escape($Field) + '=(?<value>[0-9]+)(?:\s|$)'
    foreach ($line in $Lines) {
        $match = [regex]::Match($line,$pattern)
        if ($match.Success) {
            $total += [uint64]::Parse($match.Groups['value'].Value,[Globalization.CultureInfo]::InvariantCulture)
        }
    }
    return $total
}

Assert-ZeroCompanyStopped
$resolvedGameBin = Resolve-WardrobeGameBin -GameBin $GameBin
$dllPath = Join-Path $resolvedGameBin 'ue4ss\Mods\ZeroCompanyMandoWardrobe\dlls\main.dll'
$logPath = Join-Path $resolvedGameBin 'ue4ss\UE4SS.log'

[void](Assert-KnownFileHash -Path $dllPath -ExpectedSha256 $script:WardrobeDllSha256 -Label 'installed wardrobe DLL')
if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    throw "UE4SS.log is missing: $logPath"
}

$allModLines = @(Get-Content -LiteralPath $logPath | Where-Object { $_ -match '\[ZeroCompanyMandoWardrobe\]' })
if ($allModLines.Count -eq 0) {
    throw 'No ZeroCompanyMandoWardrobe events were found in the current UE4SS.log.'
}

$sessionStart = -1
for ($index = $allModLines.Count - 1; $index -ge 0; --$index) {
    if ($allModLines[$index] -match 'loaded scope=' -and
        $allModLines[$index] -match 'helmet_fit=Man001-Man002-head-pivot-render-palette-horizontal-1\.06' -and
        $allModLines[$index] -match 'matrix_mutation=true') {
        $sessionStart = $index
        break
    }
}
$modLines = if ($sessionStart -ge 0) {
    @($allModLines[$sessionStart..($allModLines.Count - 1)])
} else {
    @()
}
$sessionMarker = if ($sessionStart -ge 0) { 1 } else { 0 }

$mainReady = @($modLines | Where-Object {
    $_ -match 'READY hooks_active=true' -and
    $_ -match 'helmet_fit=Man001-Man002-head-pivot-render-palette-horizontal-1\.06' -and
    $_ -match 'matrix_mutation=true' -and
    $_ -match 'scene_transform_writes=false' -and
    $_ -match 'face_visibility_mutation=false'
}).Count
$renderReady = @($modLines | Where-Object {
    $_ -match 'helmet_render_fit_READY' -and
    $_ -match 'mode=head-pivot-render-palette' -and
    $_ -match 'mutation_enabled=true' -and
    $_ -match 'retail_live_args=6' -and
    $_ -match 'optimized_unused_arg7=ignored' -and
    $_ -match 'scene_transform_writes=false'
}).Count
$registryReady = @($modLines | Where-Object {
    $_ -match 'helmet_render_fit_registry_scan' -and
    $_ -match 'published_assets=2(?:\s|$)' -and
    $_ -match 'unresolved_assets=0(?:\s|$)' -and
    $_ -match 'conflicts=0(?:\s|$)' -and
    $_ -match 'component_bound_refused=false' -and
    $_ -match 'target_bound_refused=false'
}).Count
$registryFailures = @($modLines | Where-Object {
    $_ -match 'helmet_render_fit_registry_scan' -and (
        $_ -match 'conflicts=[1-9][0-9]*(?:\s|$)' -or
        $_ -match 'component_bound_refused=true' -or
        $_ -match 'target_bound_refused=true'
    )
}).Count
$catalogue = @($modLines | Where-Object { $_ -match '(catalogue_tile_added|compatibility_applied)' }).Count
$voice = @($modLines | Where-Object { $_ -match 'helmet_voice' -and $_ -match '(applied|resolved)' }).Count
$summaries = @($modLines | Where-Object { $_ -match 'helmet_render_fit_summary' })

[uint64]$currentTarget = Get-SummaryTotal -Lines $summaries -Field 'current_target'
[uint64]$previousTarget = Get-SummaryTotal -Lines $summaries -Field 'previous_target'
[uint64]$currentValidated = Get-SummaryTotal -Lines $summaries -Field 'current_validated'
[uint64]$previousValidated = Get-SummaryTotal -Lines $summaries -Field 'previous_validated'
[uint64]$currentApplied = Get-SummaryTotal -Lines $summaries -Field 'current_applied'
[uint64]$previousApplied = Get-SummaryTotal -Lines $summaries -Field 'previous_applied'
[uint64]$matrices = Get-SummaryTotal -Lines $summaries -Field 'matrices'
[uint64]$registryRefused = Get-SummaryTotal -Lines $summaries -Field 'registry_refused'
[uint64]$outputRefused = Get-SummaryTotal -Lines $summaries -Field 'output_refused'
[uint64]$leaderRefused = Get-SummaryTotal -Lines $summaries -Field 'leader_refused'
[uint64]$transformRefused = Get-SummaryTotal -Lines $summaries -Field 'transform_refused'
[uint64]$deformerRefused = Get-SummaryTotal -Lines $summaries -Field 'deformer_refused'

$summaryMutationMismatch = @($summaries | Where-Object { $_ -notmatch 'mutation_enabled=true' }).Count
$hardRefusals = @($modLines | Where-Object { $_ -match '(?:^|\s)(?:helmet_render_fit_)?REFUSED(?:\s|$)' }).Count
$legacyTransformMarkers = @($modLines | Where-Object {
    $_ -match 'helmet_fit_READY' -or
    $_ -match 'helmet_fit_apply_complete' -or
    $_ -match 'helmet_pivot_compensation_summary' -or
    $_ -match 'SetRelative(?:Transform|Scale|Location|Rotation)'
}).Count
$faceMutation = @($modLines | Where-Object {
    $_ -match 'helmet_face_guard_READY' -or
    $_ -match 'helmet_face_hidden_compatibility_applied' -or
    $_ -match 'face_(?:component_)?visibility_(?:applied|mirror|hidden|reset)'
}).Count

$diagnosticRoot = Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)) 'ZeroCompanyMandoWardrobe\Diagnostics'
New-Item -ItemType Directory -Path $diagnosticRoot -Force | Out-Null
$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
$evidencePath = Join-Path $diagnosticRoot "wardrobe-runtime-$stamp.txt"
$evidenceLines = @(
    "Installed DLL SHA-256: $($script:WardrobeDllSha256)"
    "Session start index: $sessionStart"
    ''
) + $modLines
[IO.File]::WriteAllLines($evidencePath,$evidenceLines,[Text.UTF8Encoding]::new($false))
$evidenceHash = (Get-FileHash -LiteralPath $evidencePath -Algorithm SHA256).Hash

$abiRefusalTotal = $registryRefused + $outputRefused + $leaderRefused + $transformRefused + $deformerRefused
$currentComplete = $currentTarget -gt 0 -and $currentTarget -eq $currentValidated -and $currentValidated -eq $currentApplied
$previousComplete = $previousTarget -eq $previousValidated -and $previousValidated -eq $previousApplied
if ($sessionMarker -ne 1 -or $mainReady -ne 1 -or $renderReady -ne 1) {
    $result = 'PARTIAL_INTEGRATED_WARDROBE_READY_GATE_MISSING'
} elseif ($registryReady -eq 0 -or $registryFailures -ne 0) {
    $result = 'PARTIAL_INTEGRATED_WARDROBE_REGISTRY_INCOMPLETE'
} elseif ($summaries.Count -eq 0 -or -not $currentComplete -or -not $previousComplete -or $matrices -eq 0) {
    $result = 'PARTIAL_INTEGRATED_WARDROBE_NO_RENDER_FIT_OBSERVED'
} elseif ($summaryMutationMismatch -ne 0 -or $legacyTransformMarkers -ne 0 -or $faceMutation -ne 0) {
    $result = 'PARTIAL_INTEGRATED_WARDROBE_SCOPE_VIOLATION'
} elseif ($hardRefusals -ne 0 -or $abiRefusalTotal -ne 0) {
    $result = 'PARTIAL_INTEGRATED_WARDROBE_ABI_REFUSAL_OBSERVED'
} else {
    $result = 'PASS_INTEGRATED_MANDALORIAN_WARDROBE_RENDER_PALETTE_RUNTIME'
}

Write-Host "RESULT: $result"
Write-Host "Session/main/render READY events: $sessionMarker/$mainReady/$renderReady"
Write-Host "Complete registry scans: $registryReady"
Write-Host "Registry bound/conflict failures: $registryFailures"
Write-Host "Catalogue compatibility events: $catalogue"
Write-Host "Helmet voice events: $voice"
Write-Host "Current target/validated/applied calls: $currentTarget/$currentValidated/$currentApplied"
Write-Host "Previous target/validated/applied calls: $previousTarget/$previousValidated/$previousApplied"
Write-Host "Matrices adjusted: $matrices"
Write-Host "ABI refusal totals (registry/output/leader/transform/deformer): $registryRefused/$outputRefused/$leaderRefused/$transformRefused/$deformerRefused"
Write-Host "Hard refusal events: $hardRefusals"
Write-Host "Legacy component-transform markers: $legacyTransformMarkers"
Write-Host "Unexpected face-visibility mutation events: $faceMutation"
Write-Host "Evidence: $evidencePath"
Write-Host "Evidence SHA-256: $evidenceHash"

if (-not $result.StartsWith('PASS_', [StringComparison]::Ordinal)) {
    exit 2
}
