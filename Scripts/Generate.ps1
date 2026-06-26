<#
.SYNOPSIS
    CMake プリセットでプロジェクト（VS ソリューション等）を生成する。
.DESCRIPTION
    `cmake --preset <debug|release>` を実行し、build/<config> 配下に
    プロジェクトファイルを生成する。VS は マルチ構成ジェネレータなので
    1 回の生成で Debug/Release 両方をビルドできる。
.PARAMETER Config
    使用するプリセット。debug（既定）または release。
.EXAMPLE
    .\Scripts\Generate.ps1
    .\Scripts\Generate.ps1 -Config release
#>
[CmdletBinding()]
param(
    [ValidateSet('debug', 'release')]
    [string]$Config = 'debug'
)

$ErrorActionPreference = 'Stop'

# リポジトリルート（このスクリプトの 1 つ上）へ移動して実行する。
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    Write-Host "==> cmake --preset $Config" -ForegroundColor Cyan
    cmake --preset $Config
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configure failed (exit $LASTEXITCODE)"
    }
    Write-Host "==> Generated: build/$Config" -ForegroundColor Green
}
finally {
    Pop-Location
}
