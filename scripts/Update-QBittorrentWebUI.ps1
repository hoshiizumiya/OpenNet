[CmdletBinding()]
param(
    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $SourceRepository = 'C:\Users\Kanata\source\repos\qBittorrent',

    [Parameter()]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string] $Commit = '78bf5f0c715b447c4ca1127e3aae7cd3c2f0e90b'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sourceRoot = [IO.Path]::GetFullPath($SourceRepository)
$thirdPartyRoot = [IO.Path]::GetFullPath(
    (Join-Path $repositoryRoot 'OpenNet\ThirdParty\qBittorrentWebUI'))
$targetRoot = [IO.Path]::GetFullPath((Join-Path $thirdPartyRoot 'upstream'))
$licensesRoot = [IO.Path]::GetFullPath((Join-Path $thirdPartyRoot 'licenses'))
$manifestRoot = [IO.Path]::GetFullPath((Join-Path $thirdPartyRoot 'manifests'))

if (-not $targetRoot.StartsWith($repositoryRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Resolved target is outside the OpenNet repository: $targetRoot"
}
if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot '.git'))) {
    throw "qBittorrent repository was not found: $sourceRoot"
}

$resolvedCommit = (& git -C $sourceRoot rev-parse "$Commit^{commit}").Trim()
if ($LASTEXITCODE -ne 0 -or $resolvedCommit -notmatch '^[0-9a-f]{40}$') {
    throw "Unable to resolve qBittorrent commit: $Commit"
}

$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) (
    'opennet-qbt-webui-' + [Guid]::NewGuid().ToString('N'))
$archivePath = Join-Path $temporaryRoot 'webui.zip'
$extractRoot = Join-Path $temporaryRoot 'extract'
$stagedUpstream = Join-Path $extractRoot 'src\webui\www'

try {
    New-Item -ItemType Directory -Path $temporaryRoot, $extractRoot -Force | Out-Null

    & git -C $sourceRoot archive `
        --format=zip `
        "--output=$archivePath" `
        $resolvedCommit `
        src/webui/www
    if ($LASTEXITCODE -ne 0) {
        throw 'git archive failed while exporting qBittorrent WebUI.'
    }

    Expand-Archive -LiteralPath $archivePath -DestinationPath $extractRoot -Force
    if (-not (Test-Path -LiteralPath (Join-Path $stagedUpstream 'private\index.html'))) {
        throw 'The exported qBittorrent WebUI is incomplete.'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $stagedUpstream 'public\index.html'))) {
        throw 'The exported qBittorrent login UI is incomplete.'
    }

    New-Item -ItemType Directory -Path $thirdPartyRoot, $manifestRoot -Force | Out-Null

    if (Test-Path -LiteralPath $targetRoot) {
        $resolvedTarget = [IO.Path]::GetFullPath(
            (Resolve-Path -LiteralPath $targetRoot).Path)
        if (-not $resolvedTarget.StartsWith(
            $thirdPartyRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to replace unexpected target: $resolvedTarget"
        }
        Remove-Item -LiteralPath $resolvedTarget -Recurse -Force
    }
    Copy-Item -LiteralPath $stagedUpstream -Destination $targetRoot -Recurse

    if (Test-Path -LiteralPath $licensesRoot) {
        $resolvedLicenses = [IO.Path]::GetFullPath(
            (Resolve-Path -LiteralPath $licensesRoot).Path)
        if (-not $resolvedLicenses.StartsWith(
            $thirdPartyRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to replace unexpected target: $resolvedLicenses"
        }
        Remove-Item -LiteralPath $resolvedLicenses -Recurse -Force
    }
    New-Item -ItemType Directory -Path $licensesRoot -Force | Out-Null

    foreach ($licenseName in @(
        'AUTHORS',
        'COPYING',
        'COPYING.GPLv2',
        'COPYING.GPLv3'
    )) {
        $licenseArchive = Join-Path $temporaryRoot "$licenseName.zip"
        $licenseExtract = Join-Path $temporaryRoot "license-$licenseName"
        New-Item -ItemType Directory -Path $licenseExtract -Force | Out-Null
        & git -C $sourceRoot archive `
            --format=zip `
            "--output=$licenseArchive" `
            $resolvedCommit `
            $licenseName
        if ($LASTEXITCODE -ne 0) {
            throw "Unable to export qBittorrent license file: $licenseName"
        }
        Expand-Archive -LiteralPath $licenseArchive `
            -DestinationPath $licenseExtract `
            -Force
        Copy-Item -LiteralPath (Join-Path $licenseExtract $licenseName) `
            -Destination (Join-Path $licensesRoot $licenseName)
    }

    Set-Content -LiteralPath (Join-Path $thirdPartyRoot 'UPSTREAM_COMMIT') `
        -Value $resolvedCommit `
        -Encoding utf8NoBOM `
        -NoNewline

    $hashLines = Get-ChildItem -LiteralPath $targetRoot -Recurse -File |
        Sort-Object FullName |
        ForEach-Object {
            $relativePath = [IO.Path]::GetRelativePath(
                $targetRoot, $_.FullName).Replace('\', '/')
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            "$hash  $relativePath"
        }
    Set-Content -LiteralPath (Join-Path $manifestRoot 'files.sha256') `
        -Value $hashLines `
        -Encoding utf8NoBOM

    $fileCount = (Get-ChildItem -LiteralPath $targetRoot -Recurse -File).Count
    Write-Host "Imported qBittorrent WebUI $resolvedCommit ($fileCount files)."
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        $resolvedTemporary = [IO.Path]::GetFullPath($temporaryRoot)
        $systemTemporary = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if ($resolvedTemporary.StartsWith(
            $systemTemporary, [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $resolvedTemporary -Recurse -Force
        }
    }
}
