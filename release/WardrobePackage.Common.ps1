Set-StrictMode -Version Latest

$script:WardrobeModName = 'ZeroCompanyMandoWardrobe'
$script:WardrobePackageVersion = '0.4.0-build24874058'
$script:WardrobeDllSha256 = 'CCF08AB5E82CE02ED5016857AA1B322130B95E292ADF6E1F8149A2C0F5FBAF2A'
$script:RetailExeSha256 = 'C69131D496756EA421E408261FBA33B60613948E2C480ACAC91CB93632A4B67C'
$script:CompatibilityUe4ssSha256 = '8CB45C18230547A1EAD97BFEB34A2B5EF710B778890DDFC01877A7E9C61A07F4'
$script:SteamAppId = '2075800'
$script:SteamInstallDirName = 'Star Wars Zero Company'

function Get-WardrobeDefaultBackupRoot {
    $localAppData = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    if ([string]::IsNullOrWhiteSpace($localAppData)) {
        throw 'LOCALAPPDATA could not be resolved for recoverable backups.'
    }
    return Join-Path $localAppData 'ZeroCompanyMandoWardrobe\Backups'
}

function Assert-ZeroCompanyStopped {
    $running = @(Get-Process -Name 'SWZeroCompany','SWZeroCompany-Win64-Shipping' -ErrorAction SilentlyContinue)
    if ($running.Count -ne 0) {
        $ids = ($running | ForEach-Object { $_.Id }) -join ', '
        throw "Operation refused: Zero Company is running (PID $ids). Close every game process first."
    }
}

function Resolve-WardrobeGameBin {
    param([AllowNull()][AllowEmptyString()][string]$GameBin)

    if (-not [string]::IsNullOrWhiteSpace($GameBin)) {
        if (-not (Test-Path -LiteralPath $GameBin -PathType Container)) {
            throw "Game binary directory is missing: $GameBin"
        }
        $resolved = (Resolve-Path -LiteralPath $GameBin).ProviderPath.TrimEnd([IO.Path]::DirectorySeparatorChar)
        if (-not (Test-Path -LiteralPath (Join-Path $resolved 'SWZeroCompany.exe') -PathType Leaf)) {
            throw "The supplied directory is not Zero Company's Win64 folder: $resolved"
        }
        return $resolved
    }

    $steamRoots = @()
    foreach ($registryValue in @(
        @{ Path = 'HKCU:\Software\Valve\Steam'; Name = 'SteamPath' },
        @{ Path = 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam'; Name = 'InstallPath' },
        @{ Path = 'HKLM:\SOFTWARE\Valve\Steam'; Name = 'InstallPath' }
    )) {
        try {
            $value = [string](Get-ItemPropertyValue -LiteralPath $registryValue.Path -Name $registryValue.Name -ErrorAction Stop)
            if (-not [string]::IsNullOrWhiteSpace($value)) { $steamRoots += $value }
        } catch {
            # Registry discovery is best-effort; libraryfolders.vdf is authoritative below.
        }
    }
    $programFilesX86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
    if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
        $steamRoots += (Join-Path $programFilesX86 'Steam')
    }
    $steamRoots = @($steamRoots |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Group-Object { $_.ToLowerInvariant() } |
        ForEach-Object { $_.Group[0] })

    foreach ($steamRoot in @($steamRoots)) {
        $libraryFile = Join-Path $steamRoot 'steamapps\libraryfolders.vdf'
        if (-not (Test-Path -LiteralPath $libraryFile -PathType Leaf)) { continue }
        try {
            $libraryText = [IO.File]::ReadAllText($libraryFile)
            foreach ($match in [regex]::Matches($libraryText, '(?im)^\s*"path"\s+"(?<path>[^"]+)"')) {
                $libraryRoot = $match.Groups['path'].Value.Replace('\\','\')
                if (-not [string]::IsNullOrWhiteSpace($libraryRoot)) { $steamRoots += $libraryRoot }
            }
        } catch {
            # Continue with every other discovered Steam root.
        }
    }
    $steamRoots = @($steamRoots |
        Group-Object { $_.ToLowerInvariant() } |
        ForEach-Object { $_.Group[0] })

    $matches = @()
    foreach ($steamRoot in $steamRoots) {
        $steamApps = Join-Path $steamRoot 'steamapps'
        $installDir = $script:SteamInstallDirName
        $manifest = Join-Path $steamApps "appmanifest_$($script:SteamAppId).acf"
        if (Test-Path -LiteralPath $manifest -PathType Leaf) {
            try {
                $manifestText = [IO.File]::ReadAllText($manifest)
                $installMatch = [regex]::Match($manifestText, '(?im)^\s*"installdir"\s+"(?<dir>[^"]+)"')
                if ($installMatch.Success) { $installDir = $installMatch.Groups['dir'].Value }
            } catch {
                # The shipped Steam install directory name remains a safe candidate.
            }
        }
        $candidate = Join-Path $steamApps "common\$installDir\SWZeroCompany\Binaries\Win64"
        if (Test-Path -LiteralPath (Join-Path $candidate 'SWZeroCompany.exe') -PathType Leaf) {
            $matches += (Resolve-Path -LiteralPath $candidate).ProviderPath.TrimEnd([IO.Path]::DirectorySeparatorChar)
        }
    }
    $matches = @($matches |
        Group-Object { $_.ToLowerInvariant() } |
        ForEach-Object { $_.Group[0] })
    if ($matches.Count -eq 1) {
        Write-Host "AUTO-DETECTED Zero Company: $($matches[0])"
        return $matches[0]
    }
    if ($matches.Count -gt 1) {
        throw "Multiple Zero Company installations were found. Re-run with -GameBin followed by the intended SWZeroCompany\Binaries\Win64 path: $($matches -join '; ')"
    }
    throw 'Zero Company could not be auto-detected in Steam libraries. In Steam, right-click the game, choose Manage > Browse local files, open SWZeroCompany\Binaries\Win64, then re-run with -GameBin followed by that full path.'
}

function Assert-KnownFileHash {
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)][string]$ExpectedSha256,
        [Parameter(Mandatory=$true)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label is missing: $Path"
    }
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($actual -ne $ExpectedSha256.ToUpperInvariant()) {
        throw "$Label hash mismatch. Expected $ExpectedSha256; got $actual at $Path"
    }
    return $actual
}

function Assert-PathInsideDirectory {
    param(
        [Parameter(Mandatory=$true)][string]$Candidate,
        [Parameter(Mandatory=$true)][string]$Parent
    )

    $parentFull = [IO.Path]::GetFullPath($Parent).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $candidateFull = [IO.Path]::GetFullPath($Candidate)
    if (-not $candidateFull.StartsWith($parentFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escaped its expected parent. Candidate=$candidateFull Parent=$parentFull"
    }
    return $candidateFull
}

function New-WardrobeBackupDirectory {
    param(
        [Parameter(Mandatory=$true)][string]$BackupRoot,
        [Parameter(Mandatory=$true)][string]$Purpose
    )

    $stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffffffZ')
    $directory = Join-Path $BackupRoot "$stamp-$Purpose"
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    return (Resolve-Path -LiteralPath $directory).ProviderPath
}

function Read-PreservedTextDocument {
    param([Parameter(Mandatory=$true)][string]$Path)

    $bytes = [IO.File]::ReadAllBytes($Path)
    $encoding = $null
    [byte[]]$preamble = @()
    $offset = 0

    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $encoding = [Text.UTF8Encoding]::new($true)
        [byte[]]$preamble = @(0xEF,0xBB,0xBF)
        $offset = 3
    } elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
        $encoding = [Text.UnicodeEncoding]::new($false,$true)
        [byte[]]$preamble = @(0xFF,0xFE)
        $offset = 2
    } elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF) {
        $encoding = [Text.UnicodeEncoding]::new($true,$true)
        [byte[]]$preamble = @(0xFE,0xFF)
        $offset = 2
    } else {
        $encoding = [Text.UTF8Encoding]::new($false)
    }

    $text = if ($bytes.Length -gt $offset) {
        $encoding.GetString($bytes,$offset,$bytes.Length-$offset)
    } else {
        ''
    }

    return [pscustomobject]@{
        Text = $text
        Encoding = $encoding
        Preamble = $preamble
    }
}

function Write-PreservedTextDocument {
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)]$Document,
        [Parameter(Mandatory=$true)][AllowEmptyString()][string]$Text
    )

    [byte[]]$body = $Document.Encoding.GetBytes($Text)
    [byte[]]$prefix = $Document.Preamble
    [byte[]]$combined = New-Object byte[] ($prefix.Length + $body.Length)
    if ($prefix.Length -ne 0) {
        [Array]::Copy($prefix,0,$combined,0,$prefix.Length)
    }
    if ($body.Length -ne 0) {
        [Array]::Copy($body,0,$combined,$prefix.Length,$body.Length)
    }
    [IO.File]::WriteAllBytes($Path,$combined)
}

function Get-Ue4ssModEntries {
    param([Parameter(Mandatory=$true)][string]$ModsPath)

    $document = Read-PreservedTextDocument -Path $ModsPath
    $pattern = '(?m)^[ \t]*(?!;|#)(?<name>[^:;#\r\n]+?)[ \t]*:[ \t]*(?<value>[01])[ \t]*(?:;[^\r\n]*)?\r?$'
    return @([regex]::Matches($document.Text,$pattern) | ForEach-Object {
        [pscustomobject]@{
            Name = $_.Groups['name'].Value.Trim()
            Value = [int]$_.Groups['value'].Value
            Text = $_.Value
        }
    })
}

function Set-WardrobeManifestEntry {
    param(
        [Parameter(Mandatory=$true)][string]$ModsPath,
        [Parameter(Mandatory=$true)][bool]$Enabled
    )

    if (-not (Test-Path -LiteralPath $ModsPath -PathType Leaf)) {
        $parent = Split-Path -Parent $ModsPath
        if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
            throw "UE4SS Mods directory is missing: $parent"
        }
        [IO.File]::WriteAllText($ModsPath,"; UE4SS mod activation manifest`r`n",[Text.UTF8Encoding]::new($false))
    }

    $document = Read-PreservedTextDocument -Path $ModsPath
    $escapedName = [regex]::Escape($script:WardrobeModName)
    $pattern = "(?im)^(?<prefix>[ \t]*$escapedName[ \t]*:[ \t]*)(?<value>[01])(?<suffix>[ \t]*(?:;[^\r\n]*)?)(?<cr>\r?)$"
    $matches = [regex]::Matches($document.Text,$pattern)
    if ($matches.Count -gt 1) {
        throw "mods.txt has duplicate $script:WardrobeModName entries; refusing to guess which line owns activation."
    }

    $value = if ($Enabled) { '1' } else { '0' }
    if ($matches.Count -eq 1) {
        $lineRegex = [regex]::new($pattern)
        $updated = $lineRegex.Replace(
            $document.Text,
            { param($match) $match.Groups['prefix'].Value + $value + $match.Groups['suffix'].Value + $match.Groups['cr'].Value },
            1)
    } else {
        $newline = if ($document.Text.Contains("`r`n")) { "`r`n" } else { "`n" }
        $updated = $document.Text
        if ($updated.Length -ne 0 -and -not $updated.EndsWith("`n") -and -not $updated.EndsWith("`r")) {
            $updated += $newline
        }
        $updated += "$script:WardrobeModName : $value$newline"
    }

    $temporary = "$ModsPath.wardrobe-update.$PID"
    try {
        Write-PreservedTextDocument -Path $temporary -Document $document -Text $updated
        Move-Item -LiteralPath $temporary -Destination $ModsPath -Force
    } finally {
        if (Test-Path -LiteralPath $temporary -PathType Leaf) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }

    $entries = @(Get-Ue4ssModEntries -ModsPath $ModsPath | Where-Object {
        $_.Name.Equals($script:WardrobeModName,[StringComparison]::OrdinalIgnoreCase)
    })
    if ($entries.Count -ne 1 -or $entries[0].Value -ne [int]$value) {
        throw 'The post-write mods.txt verification failed.'
    }
}
