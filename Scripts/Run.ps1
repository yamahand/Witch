<#
.SYNOPSIS
    WitchGame をビルドして実行する。
.DESCRIPTION
    必要なら configure（build/<config> が無ければ自動生成）してから
    `cmake --build` でビルドし、生成された WitchGame.exe を起動する。
    VS マルチ構成のため、出力は build/<config>/<Config>/WitchGame.exe。
.PARAMETER Config
    使用するプリセット。debug（既定）または release。
.PARAMETER NoBuild
    ビルドを省略し、既存の実行ファイルをそのまま起動する。
.EXAMPLE
    .\Scripts\Run.ps1
    .\Scripts\Run.ps1 -Config release
#>
[CmdletBinding()]
param(
    [ValidateSet('debug', 'release')]
    [string]$Config = 'debug',

    [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'

$repoRoot  = Split-Path -Parent $PSScriptRoot
$buildDir  = Join-Path $repoRoot "build/$Config"
# プリセット名（debug/release）と MSBuild の構成名（Debug/Release）は別物。
$msbuildConfig = (Get-Culture).TextInfo.ToTitleCase($Config)

Push-Location $repoRoot
try {
    # build/<config> が未生成なら configure を先に走らせる。
    if (-not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) {
        Write-Host "==> build/$Config が無いので生成します" -ForegroundColor Yellow
        cmake --preset $Config
        if ($LASTEXITCODE -ne 0) { throw "cmake configure failed (exit $LASTEXITCODE)" }
    }

    if (-not $NoBuild) {
        Write-Host "==> cmake --build build/$Config --config $msbuildConfig" -ForegroundColor Cyan
        cmake --build $buildDir --config $msbuildConfig
        if ($LASTEXITCODE -ne 0) { throw "build failed (exit $LASTEXITCODE)" }
    }

    $exe = Join-Path $buildDir "$msbuildConfig/WitchGame.exe"
    if (-not (Test-Path $exe)) {
        throw "実行ファイルが見つかりません: $exe"
    }

    Write-Host "==> Run: $exe" -ForegroundColor Green
    # 実行ファイルのあるディレクトリで起動（隣の Shaders/ Assets/ を拾うため）。
    Push-Location (Split-Path -Parent $exe)
    try {
        & $exe
        $exitCode = $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
    exit $exitCode
}
finally {
    Pop-Location
}
