#Requires -Version 5.1
# Builds nothing: deploys whatever is already in bin\<Config> to every copy of the
# game this machine holds. pixi run install builds first and then calls this.
# Detection and the per-install loop live in cameraunlock-core's DevDeploy.psm1.
#
# No config is deployed: the mod creates CameraUnlock.ini at its first start, and
# a copy from here would overwrite the developer's own CameraUnlock.ini or the
# DyingLightHeadTracking.ini an older build reads.

param(
    [Parameter(Position = 0)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    # A caller-supplied install wins outright and stays a single target.
    [Parameter(Position = 1)]
    [string]$GivenPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = 'SilentlyContinue'

$repo = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $repo "cameraunlock-core\powershell\DevDeploy.psm1") -Force

Invoke-DevDeployASILoader `
    -GameId 'dying-light' `
    -GameDisplayName 'Dying Light' `
    -BuildOutputPath (Join-Path $repo "bin\$Configuration") `
    -ModDllName 'DyingLightHeadTracking.asi' `
    -VendorLoaderDll (Join-Path $repo 'vendor\ultimate-asi-loader\dinput8.dll') `
    -AsiLoaderName 'winmm.dll' `
    -GivenPath $GivenPath | Out-Null
