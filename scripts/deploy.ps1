#Requires -Version 5.1
# Builds nothing: deploys whatever is already in bin\<Config> to every copy of the
# game this machine holds. pixi run install builds first and then calls this.
# Detection and the per-install loop live in cameraunlock-core's DevDeploy.psm1.
#
# The ini is deliberately not deployed: a dev copy next to the exe is usually
# hand-tuned, and the mod writes its own defaults when none is there.

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
