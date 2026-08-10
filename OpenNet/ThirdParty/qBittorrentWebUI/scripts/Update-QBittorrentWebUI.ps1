param(
    [Parameter(Mandatory = $true)]
    [string]$Repository
)

$source = Join-Path $Repository 'src\webui\www'
$target = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\upstream'))
if (-not (Test-Path (Join-Path $source 'private\index.html')) -or
    -not (Test-Path (Join-Path $source 'public\index.html'))) {
    throw "Repository does not contain src/webui/www: $Repository"
}
if (-not $target.EndsWith('ThirdParty\qBittorrentWebUI\upstream', [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to mirror to unexpected target: $target"
}

& robocopy $source $target /MIR /COPY:DAT /DCOPY:DAT /R:2 /W:1 /NFL /NDL /NJH /NJS
if ($LASTEXITCODE -ge 8) { throw "robocopy failed with exit code $LASTEXITCODE" }

$revision = (& git -C $Repository rev-parse HEAD).Trim()
[IO.File]::WriteAllText(
    (Join-Path $PSScriptRoot '..\UPSTREAM_COMMIT'),
    "$revision`n",
    [Text.UTF8Encoding]::new($false))

$manifest = Join-Path $PSScriptRoot '..\manifests\files.sha256'
$rootLength = $target.Length + 1
$files = [System.IO.Directory]::GetFiles($target, '*', [System.IO.SearchOption]::AllDirectories)
$hashes = @{}
foreach ($file in $files) {
    $relative = $file.Substring($rootLength).Replace('\', '/')
    $hashes[$relative] = (Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant()
}

# Retain the established path order so an upstream refresh only changes hashes
# for files whose contents really changed. New files are appended deterministically.
$lines = New-Object System.Collections.Generic.List[string]
$seen = @{}
if (Test-Path $manifest) {
    foreach ($line in [IO.File]::ReadAllLines($manifest)) {
        if (($line -match '^[0-9a-fA-F]{64}  (.+)$') -and $hashes.ContainsKey($Matches[1])) {
            $relative = $Matches[1]
            $lines.Add(('{0}  {1}' -f $hashes[$relative], $relative))
            $seen[$relative] = $true
        }
    }
}
foreach ($relative in ($hashes.Keys | Where-Object { -not $seen.ContainsKey($_) } | Sort-Object)) {
    $lines.Add(('{0}  {1}' -f $hashes[$relative], $relative))
}
[IO.File]::WriteAllLines($manifest, $lines.ToArray(), [Text.UTF8Encoding]::new($false))
Write-Host "qBittorrent WebUI synced at $revision"
