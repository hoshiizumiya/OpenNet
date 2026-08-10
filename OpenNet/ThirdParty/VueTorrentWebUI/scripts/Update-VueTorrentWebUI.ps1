[CmdletBinding()]
param(
    [string]$ReleaseUri = 'https://github.com/VueTorrent/VueTorrent/releases/latest/download/vuetorrent.zip',
    [string]$ArchivePath,
    [string]$SourceRepository
)

$ErrorActionPreference = 'Stop'
$packageRoot = Split-Path -Parent $PSScriptRoot
$destination = Join-Path $packageRoot 'upstream\public'
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('OpenNet-VueTorrent-' + [guid]::NewGuid().ToString('N'))
$downloadedArchive = $false

try {
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
    if ([string]::IsNullOrWhiteSpace($ArchivePath)) {
        $ArchivePath = Join-Path $temporaryRoot 'vuetorrent.zip'
        Invoke-WebRequest -Uri $ReleaseUri -OutFile $ArchivePath -UseBasicParsing
        $downloadedArchive = $true
    }

    $expanded = Join-Path $temporaryRoot 'expanded'
    Expand-Archive -LiteralPath $ArchivePath -DestinationPath $expanded -Force
    $index = Get-ChildItem -LiteralPath $expanded -Filter index.html -File -Recurse |
        Where-Object { $_.Directory.Name -eq 'public' } |
        Select-Object -First 1
    if ($null -eq $index) {
        throw 'The archive does not contain a VueTorrent public/index.html.'
    }

    $source = $index.Directory.FullName
    if (Test-Path -LiteralPath $destination) {
        Remove-Item -LiteralPath $destination -Recurse -Force
    }
    New-Item -ItemType Directory -Path $destination | Out-Null
    Copy-Item -Path (Join-Path $source '*') -Destination $destination -Recurse -Force

    $versionFile = Join-Path $index.Directory.Parent.FullName 'version.txt'
    $version = if (Test-Path -LiteralPath $versionFile) {
        (Get-Content -LiteralPath $versionFile -Raw).Trim()
    }
    else {
        'release-archive'
    }
    Set-Content -LiteralPath (Join-Path $packageRoot 'UPSTREAM_VERSION') -Value $version -Encoding UTF8

    if (-not [string]::IsNullOrWhiteSpace($SourceRepository)) {
        $license = Join-Path $SourceRepository 'LICENSE'
        if (Test-Path -LiteralPath $license) {
            Copy-Item -LiteralPath $license -Destination (Join-Path $packageRoot 'LICENSE') -Force
        }
    }

    $manifest = Get-ChildItem -LiteralPath $destination -File -Recurse |
        Sort-Object FullName |
        ForEach-Object {
            $relative = $_.FullName.Substring($destination.Length).TrimStart('\').Replace('\', '/')
            '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $relative
        }
    $manifest | Set-Content -LiteralPath (Join-Path $packageRoot 'files.sha256') -Encoding UTF8
    Write-Host "VueTorrent $version synchronized to $destination"
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
