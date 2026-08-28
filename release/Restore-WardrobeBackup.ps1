[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BackupDirectory,
    [string]$GameBin,
    [string]$BackupRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'WardrobePackage.Common.ps1')

Assert-ZeroCompanyStopped
$resolvedBackup = (Resolve-Path -LiteralPath $BackupDirectory).ProviderPath
$statePath = Join-Path $resolvedBackup 'backup-state.json'
if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) {
    throw "This is not an installer backup: $statePath is missing."
}
$state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
if ($state.Schema -ne 1 -or $state.ModName -ne $script:WardrobeModName -or
    $state.PackageVersion -ne $script:WardrobePackageVersion) {
    throw 'Backup metadata does not belong to this exact wardrobe package.'
}
if ([string]::IsNullOrWhiteSpace($GameBin)) {
    $GameBin = [string]$state.GameBin
}
$resolvedGameBin = Resolve-WardrobeGameBin -GameBin $GameBin
$stateGameBin = [IO.Path]::GetFullPath([string]$state.GameBin).TrimEnd([IO.Path]::DirectorySeparatorChar)
if (-not $resolvedGameBin.Equals($stateGameBin,[StringComparison]::OrdinalIgnoreCase)) {
    throw "Backup belongs to a different game directory: $stateGameBin"
}
if ([string]::IsNullOrWhiteSpace($BackupRoot)) {
    $BackupRoot = Get-WardrobeDefaultBackupRoot
}

$modsRoot = Join-Path $resolvedGameBin 'ue4ss\Mods'
$modsPath = Join-Path $modsRoot 'mods.txt'
$dllDirectory = Join-Path $modsRoot 'ZeroCompanyMandoWardrobe\dlls'
$dllPath = Join-Path $dllDirectory 'main.dll'
$preRestore = New-WardrobeBackupDirectory -BackupRoot $BackupRoot -Purpose 'pre-restore'

if (Test-Path -LiteralPath $modsPath -PathType Leaf) {
    Copy-Item -LiteralPath $modsPath -Destination (Join-Path $preRestore 'mods.txt')
}
if (Test-Path -LiteralPath $dllPath -PathType Leaf) {
    New-Item -ItemType Directory -Path (Join-Path $preRestore 'dll') -Force | Out-Null
    Copy-Item -LiteralPath $dllPath -Destination (Join-Path $preRestore 'dll\main.dll')
}

if ([bool]$state.ModsTxtExisted) {
    $savedMods = Join-Path $resolvedBackup 'mods.txt'
    if (-not (Test-Path -LiteralPath $savedMods -PathType Leaf)) {
        throw "Backup metadata expects mods.txt, but the saved file is missing: $savedMods"
    }
    if ($state.PriorModsTxtSha256) {
        Assert-KnownFileHash -Path $savedMods -ExpectedSha256 ([string]$state.PriorModsTxtSha256) -Label 'saved prior mods.txt' | Out-Null
    }
    Copy-Item -LiteralPath $savedMods -Destination $modsPath -Force
} elseif (Test-Path -LiteralPath $modsPath -PathType Leaf) {
    Move-Item -LiteralPath $modsPath -Destination (Join-Path $preRestore 'created-by-install-mods.txt') -Force
}

if ([bool]$state.DllExisted) {
    $savedDll = Join-Path $resolvedBackup 'main.dll'
    if (-not (Test-Path -LiteralPath $savedDll -PathType Leaf)) {
        throw "Backup metadata expects a prior DLL, but the saved file is missing: $savedDll"
    }
    if ($state.PriorDllSha256) {
        Assert-KnownFileHash -Path $savedDll -ExpectedSha256 ([string]$state.PriorDllSha256) -Label 'saved prior DLL' | Out-Null
    }
    New-Item -ItemType Directory -Path $dllDirectory -Force | Out-Null
    Copy-Item -LiteralPath $savedDll -Destination $dllPath -Force
} elseif (Test-Path -LiteralPath $dllPath -PathType Leaf) {
    Move-Item -LiteralPath $dllPath -Destination (Join-Path $preRestore 'created-by-install-main.dll') -Force
}

Write-Host "PASS restored installer backup $resolvedBackup"
Write-Host "PRE-RESTORE SAFETY COPY $preRestore"

