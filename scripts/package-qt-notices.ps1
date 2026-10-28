# SPDX-License-Identifier: MIT
param(
    [Parameter(Mandatory=$true)][string]$Destination,
    [string]$Version = '6.8.3',
    [string]$Cache = '.tools/qt-notice-sources'
)
$ErrorActionPreference = 'Stop'
# The binary Qt SDK does not consistently ship LICENSES. Collect the notices
# from the exact source version instead of silently omitting them.
foreach ($module in @('qtbase', 'qtdeclarative', 'qtsvg', 'qtshadertools')) {
    $source = Join-Path $Cache "$module-$Version"
    if (-not (Test-Path (Join-Path $source '.git'))) {
        git -c advice.detachedHead=false clone --quiet --depth 1 --branch "v$Version" --filter=blob:none --sparse "https://github.com/qt/$module.git" $source
        if ($LASTEXITCODE -ne 0) { throw "Cannot fetch $module notices" }
    }
    git -C $source sparse-checkout set --no-cone '/LICENSES/' '**/LICENSE*' '**/COPYING*' '**/qt_attribution.json' '**/REUSE.toml'
    if ($LASTEXITCODE -ne 0) { throw "Cannot select $module notices" }
    if (-not (Test-Path (Join-Path $source 'LICENSES/GPL-3.0-only.txt'))) { throw "Missing $module GPL notice" }
    $target = Join-Path $Destination $module
    New-Item -ItemType Directory -Force $target | Out-Null
    Get-ChildItem $source -File -Recurse -Force | Where-Object { $_.FullName -notmatch '[\\/]\.git[\\/]' } | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath((Resolve-Path $source).Path, $_.FullName)
        $output = Join-Path $target $relative
        New-Item -ItemType Directory -Force (Split-Path $output -Parent) | Out-Null
        Copy-Item -LiteralPath $_.FullName -Destination $output
    }
    $revision = git -C $source rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw "Cannot identify $module source revision" }
    "Qt $module $Version; unmodified shared SDK libraries. Source: https://github.com/qt/$module/tree/$revision" | Set-Content -LiteralPath (Join-Path $target 'SOURCE.txt') -Encoding utf8
}
