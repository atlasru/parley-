$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$deps = Join-Path $repoRoot '.deps'
New-Item -ItemType Directory -Force $deps | Out-Null
$pins = @(
    @('toxcore', 'https://github.com/TokTok/c-toxcore.git', '285b90265d9f79039c3573d00afdfbfff34efd6d'),
    @('libsodium', 'https://github.com/jedisct1/libsodium.git', '77e1ce5d6dee871c49ef211222ba18ef0c486bda')
)
foreach ($pin in $pins) {
    $dest = Join-Path $deps $pin[0]
    if (-not (Test-Path -LiteralPath "$dest/.git")) {
        git init $dest
        if ($LASTEXITCODE -ne 0) { throw 'git init failed' }
        git -C $dest remote add origin $pin[1]
    }
    $head = git -C $dest rev-parse HEAD 2>$null
    if ($head -ne $pin[2]) {
        git -C $dest fetch --depth 1 origin $pin[2]
        if ($LASTEXITCODE -ne 0) { throw 'Dependency fetch failed' }
        git -C $dest checkout --detach $pin[2]
        if ($LASTEXITCODE -ne 0) { throw 'Dependency checkout failed' }
    }
}
git -C "$deps/toxcore" submodule update --init --depth 1
if ($LASTEXITCODE -ne 0) { throw 'cmp fetch failed' }
