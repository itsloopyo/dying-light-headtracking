#!/usr/bin/env pwsh
#Requires -Version 5.1
# Packaging for Dying Light Head Tracking.
#
# One ZIP: DyingLightHeadTracking-v{version}-installer.zip, holding install.cmd,
# the plugin and the docs.
#
# There is deliberately NO Nexus ZIP. The payload is an ASI plugin that has to
# sit next to DyingLightGame.exe in the game root, and a mod manager deploys into
# one fixed subtree per game, so nothing a manager installs can reach the root.
# An archive shaped for one would install without complaining and never load.
# Publish-NightlyBuild therefore needs -NoNexusZip, whose default treats a
# missing Nexus ZIP as fatal.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = 'SilentlyContinue'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

Import-Module (Join-Path $projectDir "cameraunlock-core\powershell\ReleaseWorkflow.psm1") -Force

$manifest = Get-Content (Join-Path $projectDir "manifest.json") | ConvertFrom-Json
$version = $manifest.version

Write-Host "=== Dying Light Head Tracking - Package Release ===" -ForegroundColor Magenta
Write-Host "Version: $version" -ForegroundColor Cyan
Write-Host ""

$releaseDir = Join-Path $projectDir "release"
if (-not (Test-Path $releaseDir)) {
    New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
}

$asiPath = Join-Path $projectDir "bin/Release/DyingLightHeadTracking.asi"
if (-not (Test-Path $asiPath)) { throw "DyingLightHeadTracking.asi not found at: $asiPath" }

$iniPath = Join-Path $projectDir "DyingLightHeadTracking.ini"
if (-not (Test-Path $iniPath)) { throw "DyingLightHeadTracking.ini not found at: $iniPath" }

$vendorAsiDir = Join-Path $projectDir "vendor/ultimate-asi-loader"
if (-not (Test-Path (Join-Path $vendorAsiDir "dinput8.dll"))) {
    throw "Bundled ASI loader missing: $vendorAsiDir\dinput8.dll"
}

$launcherManifestPath = Join-Path $projectDir "launcher-manifest.json"
if (-not (Test-Path $launcherManifestPath)) { throw "launcher-manifest.json not found" }
# Lopari writes the seeded ini on a fresh install, so a blob that has drifted
# from DyingLightHeadTracking.ini ships different defaults to launcher users.
Assert-ManifestSeedsMatchShipped -ManifestPath $launcherManifestPath -ProjectRoot $projectDir

$scriptsDir = Join-Path $projectDir "scripts"
foreach ($script in @("install.cmd", "uninstall.cmd")) {
    if (-not (Test-Path (Join-Path $scriptsDir $script))) {
        throw "Required script not found: $scriptsDir\$script"
    }
}

$stagingDir = Join-Path $releaseDir "staging-github"
if (Test-Path $stagingDir) { Remove-Item -Recurse -Force $stagingDir }
New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null

foreach ($script in @("install.cmd", "uninstall.cmd")) {
    Copy-Item (Join-Path $scriptsDir $script) -Destination $stagingDir -Force
    Write-Host "  $script" -ForegroundColor Green
}

# The launcher manifest, with the real release version stamped in. Written
# through the .NET API because Set-Content -Encoding UTF8 on Windows PowerShell
# 5.1 emits a BOM, which serde_json rejects.
$manifestJson = Get-Content $launcherManifestPath -Raw | ConvertFrom-Json
$manifestJson.mod_info.version = $version
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText(
    (Join-Path $stagingDir "launcher-manifest.json"),
    ($manifestJson | ConvertTo-Json -Depth 10),
    $utf8NoBom)
Write-Host "  launcher-manifest.json (v$version)" -ForegroundColor Green

$pluginsDir = Join-Path $stagingDir "plugins"
New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
Copy-Item $asiPath -Destination $pluginsDir -Force
Write-Host "  plugins/DyingLightHeadTracking.asi" -ForegroundColor Green
Copy-Item $iniPath -Destination $pluginsDir -Force
Write-Host "  plugins/DyingLightHeadTracking.ini" -ForegroundColor Green

# Ultimate ASI Loader travels with the installer so install.cmd never reaches
# the network. Its MIT licence has to travel with the binary, so a missing
# LICENSE is a compliance failure rather than a cosmetic gap.
$stagingVendorDir = Join-Path $stagingDir "vendor/ultimate-asi-loader"
New-Item -ItemType Directory -Path $stagingVendorDir -Force | Out-Null
foreach ($vendorFile in @("dinput8.dll", "LICENSE", "README.md")) {
    $src = Join-Path $vendorAsiDir $vendorFile
    if (-not (Test-Path $src)) { throw "Vendored ASI loader file not found: $src" }
    Copy-Item $src -Destination $stagingVendorDir -Force
    Write-Host "  vendor/ultimate-asi-loader/$vendorFile" -ForegroundColor Green
}

foreach ($doc in @("README.md", "LICENSE", "CHANGELOG.md", "THIRD-PARTY-NOTICES.md")) {
    $docPath = Join-Path $projectDir $doc
    if (-not (Test-Path $docPath)) { throw "Required document not found: $docPath" }
    Copy-Item $docPath -Destination $stagingDir -Force
    Write-Host "  $doc" -ForegroundColor Green
}

Copy-SharedBundle -StagingDir $stagingDir

$zipName = "DyingLightHeadTracking-v$version-installer.zip"
$zipPath = Join-Path $releaseDir $zipName
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

Write-Host ""
Write-Host "Creating installer ZIP..." -ForegroundColor Cyan
Push-Location $stagingDir
try {
    Compress-Archive -Path ".\*" -DestinationPath $zipPath -Force
} finally {
    Pop-Location
}
Remove-Item -Recurse -Force $stagingDir

Write-Host ("  $zipPath ({0:N1} KB)" -f ((Get-Item $zipPath).Length / 1KB)) -ForegroundColor Green
Write-Host ""
Write-Host "Done." -ForegroundColor Magenta
