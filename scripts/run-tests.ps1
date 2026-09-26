#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Run the unit tests and the config differential test in the Release build tree.
.DESCRIPTION
    `pixi run build-release` has already built every test binary into build/. This
    checks that the differential test still compiles the files provenance.txt names,
    then runs ctest.

    Non-interactive: exits 0 when every test passes, non-zero with ctest's own
    output on the first failure.
.NOTES
    Run via: pixi run test
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'

# The differential test is only as good as its claim about what it compiled. SHA256 through
# .NET, because Get-FileHash is not found when a runner's pwsh runs this under Windows
# PowerShell.
$provenance = Join-Path $projectRoot 'tests/config_differential/provenance.txt'
$sha256 = [System.Security.Cryptography.SHA256]::Create()
foreach ($line in Get-Content $provenance) {
    if ($line -match '^\s*(#|$)') { continue }
    $hash, $path = ($line -split '\s+', 3)[0, 1]
    $bytes = [System.IO.File]::ReadAllBytes((Join-Path $projectRoot $path))
    $actual = -join ($sha256.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') })
    if ($actual -ne $hash) { throw "$path has changed: sha256 $actual, provenance.txt records $hash" }
}

& ctest --test-dir $buildDir --build-config Release --output-on-failure
if ($LASTEXITCODE -ne 0) { Write-Host 'ERROR: tests failed' -ForegroundColor Red; exit 1 }

Write-Host 'All tests passed.' -ForegroundColor Green
