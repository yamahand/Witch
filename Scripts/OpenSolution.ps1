<#
.SYNOPSIS
    生成された Visual Studio ソリューション（Witch.slnx）を開く。
.DESCRIPTION
    build/<config>/Witch.slnx を既定のアプリ（Visual Studio）で開く。
    未生成の場合は先に configure する。VS はマルチ構成なので、
    開いた 1 つのソリューション内で Debug/Release を切り替えられる。
.PARAMETER Config
    どのプリセットの生成物を開くか。debug（既定）または release。
.EXAMPLE
    .\Scripts\OpenSolution.ps1
    .\Scripts\OpenSolution.ps1 -Config release
#>
[CmdletBinding()]
param(
    [ValidateSet('debug', 'release')]
    [string]$Config = 'debug'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build/$Config"
$slnx     = Join-Path $buildDir 'Witch.slnx'

Push-Location $repoRoot
try {
    # ソリューションが無ければ生成する。
    if (-not (Test-Path $slnx)) {
        Write-Host "==> $slnx が無いので生成します" -ForegroundColor Yellow
        cmake --preset $Config
        if ($LASTEXITCODE -ne 0) { throw "cmake configure failed (exit $LASTEXITCODE)" }
    }

    if (-not (Test-Path $slnx)) {
        throw "ソリューションが見つかりません: $slnx"
    }

    Write-Host "==> Open: $slnx" -ForegroundColor Green
    # 既定アプリ（.slnx に関連付けられた Visual Studio）で開く。
    Invoke-Item $slnx
}
finally {
    Pop-Location
}
