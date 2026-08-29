[CmdletBinding()]
param(
    [string]$GameBin,
    [string]$BackupRoot,
    [switch]$RemoveFiles,
    [switch]$StockOutfitConfirmed
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
$modDirectory = Join-Path $modsRoot 'ZeroCompanyMandoWardrobe'
$dllPath = Join-Path $modDirectory 'dlls\main.dll'

if ($RemoveFiles -and -not $StockOutfitConfirmed) {
    throw 'File removal refused. First equip and save an ordinary stock outfit, then pass -StockOutfitConfirmed.'
}
if ($RemoveFiles -and (Test-Path -LiteralPath $modDirectory -PathType Container)) {
    Assert-PathInsideDirectory -Candidate $modDirectory -Parent $modsRoot | Out-Null
    if (Test-Path -LiteralPath $dllPath -PathType Leaf) {
        Assert-KnownFileHash -Path $dllPath -ExpectedSha256 $script:WardrobeDllSha256 -Label 'installed wardrobe DLL' | Out-Null
    }
}

$backupDirectory = New-WardrobeBackupDirectory -BackupRoot $BackupRoot -Purpose 'uninstall'
if (Test-Path -LiteralPath $modsPath -PathType Leaf) {
    Copy-Item -LiteralPath $modsPath -Destination (Join-Path $backupDirectory 'mods.txt')
    Set-WardrobeManifestEntry -ModsPath $modsPath -Enabled $false
    Write-Host 'PASS wardrobe disabled; unrelated mods.txt content was preserved'
} else {
    Write-Host 'Wardrobe manifest entry was already absent because mods.txt does not exist.'
}

if ($RemoveFiles) {
    if (Test-Path -LiteralPath $modDirectory -PathType Container) {
        $resolvedModDirectory = Assert-PathInsideDirectory -Candidate $modDirectory -Parent $modsRoot
        $removedDestination = Join-Path $backupDirectory 'removed-ZeroCompanyMandoWardrobe'
        Move-Item -LiteralPath $resolvedModDirectory -Destination $removedDestination
        Write-Host "PASS wardrobe folder moved to recoverable backup $removedDestination"
    } else {
        Write-Host 'Wardrobe folder was already absent.'
    }
} else {
    Write-Host 'DLL retained for easy rollback. Use -RemoveFiles -StockOutfitConfirmed only after changing to a stock outfit.'
}
Write-Host "RECOVERABLE BACKUP $backupDirectory"
