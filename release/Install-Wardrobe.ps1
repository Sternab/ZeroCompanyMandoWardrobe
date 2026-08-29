[CmdletBinding()]
param(
    [string]$GameBin,
    [ValidateSet('Disabled','Enabled')]
    [string]$Mode = 'Enabled',
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

$exePath = Join-Path $resolvedGameBin 'SWZeroCompany.exe'
$ue4ssPath = Join-Path $resolvedGameBin 'ue4ss\UE4SS.dll'
$modsRoot = Join-Path $resolvedGameBin 'ue4ss\Mods'
$modsPath = Join-Path $modsRoot 'mods.txt'
$dllSource = Join-Path $PSScriptRoot 'payload\ue4ss\Mods\ZeroCompanyMandoWardrobe\dlls\main.dll'
$dllDirectory = Join-Path $modsRoot 'ZeroCompanyMandoWardrobe\dlls'
$dllDestination = Join-Path $dllDirectory 'main.dll'

Assert-KnownFileHash -Path $exePath -ExpectedSha256 $script:RetailExeSha256 -Label 'retail executable' | Out-Null
Assert-KnownFileHash -Path $ue4ssPath -ExpectedSha256 $script:CompatibilityUe4ssSha256 -Label 'compatibility UE4SS' | Out-Null
Assert-KnownFileHash -Path $dllSource -ExpectedSha256 $script:WardrobeDllSha256 -Label 'proven wardrobe payload' | Out-Null
if (-not (Test-Path -LiteralPath $modsRoot -PathType Container)) {
    throw "UE4SS Mods directory is missing: $modsRoot"
}

$backupDirectory = New-WardrobeBackupDirectory -BackupRoot $BackupRoot -Purpose 'install'
$modsExisted = Test-Path -LiteralPath $modsPath -PathType Leaf
$dllExisted = Test-Path -LiteralPath $dllDestination -PathType Leaf
$priorModsHash = if ($modsExisted) { (Get-FileHash -LiteralPath $modsPath -Algorithm SHA256).Hash.ToUpperInvariant() } else { $null }
$priorDllHash = if ($dllExisted) { (Get-FileHash -LiteralPath $dllDestination -Algorithm SHA256).Hash.ToUpperInvariant() } else { $null }

if ($modsExisted) {
    Copy-Item -LiteralPath $modsPath -Destination (Join-Path $backupDirectory 'mods.txt')
}
if ($dllExisted) {
    Copy-Item -LiteralPath $dllDestination -Destination (Join-Path $backupDirectory 'main.dll')
}

$state = [ordered]@{
    Schema = 1
    Purpose = 'ZeroCompanyMandoWardrobe install backup'
    PackageVersion = $script:WardrobePackageVersion
    ModName = $script:WardrobeModName
    GameBin = $resolvedGameBin
    ModsTxtExisted = [bool]$modsExisted
    PriorModsTxtSha256 = $priorModsHash
    DllExisted = [bool]$dllExisted
    PriorDllSha256 = $priorDllHash
    CreatedUtc = [DateTime]::UtcNow.ToString('o')
}
[IO.File]::WriteAllText(
    (Join-Path $backupDirectory 'backup-state.json'),
    ($state | ConvertTo-Json -Depth 3),
    [Text.UTF8Encoding]::new($false))

$stagedDll = "$dllDestination.installing.$PID"
$changedDll = $false
$changedManifest = $false
try {
    New-Item -ItemType Directory -Path $dllDirectory -Force | Out-Null
    Copy-Item -LiteralPath $dllSource -Destination $stagedDll -Force
    Assert-KnownFileHash -Path $stagedDll -ExpectedSha256 $script:WardrobeDllSha256 -Label 'staged wardrobe DLL' | Out-Null
    Move-Item -LiteralPath $stagedDll -Destination $dllDestination -Force
    $changedDll = $true

    $changedManifest = $true
    Set-WardrobeManifestEntry -ModsPath $modsPath -Enabled ($Mode -eq 'Enabled')

    Assert-KnownFileHash -Path $dllDestination -ExpectedSha256 $script:WardrobeDllSha256 -Label 'installed wardrobe DLL' | Out-Null
} catch {
    $failure = $_
    try {
        if ($modsExisted -and (Test-Path -LiteralPath (Join-Path $backupDirectory 'mods.txt') -PathType Leaf)) {
            Copy-Item -LiteralPath (Join-Path $backupDirectory 'mods.txt') -Destination $modsPath -Force
        } elseif ($changedManifest -and (Test-Path -LiteralPath $modsPath -PathType Leaf)) {
            Move-Item -LiteralPath $modsPath -Destination (Join-Path $backupDirectory 'failed-install-mods.txt') -Force
        }

        if ($dllExisted -and (Test-Path -LiteralPath (Join-Path $backupDirectory 'main.dll') -PathType Leaf)) {
            Copy-Item -LiteralPath (Join-Path $backupDirectory 'main.dll') -Destination $dllDestination -Force
        } elseif ($changedDll -and (Test-Path -LiteralPath $dllDestination -PathType Leaf)) {
            Move-Item -LiteralPath $dllDestination -Destination (Join-Path $backupDirectory 'failed-install-main.dll') -Force
        }
    } catch {
        throw "Install failed and automatic rollback also failed. Original error: $($failure.Exception.Message). Rollback error: $($_.Exception.Message). Backup: $backupDirectory"
    }
    throw "Install failed; prior files were restored. $($failure.Exception.Message). Backup: $backupDirectory"
} finally {
    if (Test-Path -LiteralPath $stagedDll -PathType Leaf) {
        Remove-Item -LiteralPath $stagedDll -Force
    }
}

$enabledText = if ($Mode -eq 'Enabled') { 'enabled' } else { 'disabled' }
Write-Host "PASS installed proven wardrobe DLL $script:WardrobeDllSha256"
Write-Host "PASS wardrobe activation entry is $enabledText; all unrelated mods.txt content was preserved"
Write-Host "RECOVERABLE BACKUP $backupDirectory"
if ($Mode -eq 'Disabled') {
    Write-Host 'Run Set-WardrobeEnabled.ps1 -State Enabled when you intentionally want the wardrobe active.'
}
