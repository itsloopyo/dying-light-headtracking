#Requires -Version 5.1
<#
.SYNOPSIS
    Cuts a release of Dying Light Head Tracking: changelog, version stamp,
    package, commit, tag, push.

.DESCRIPTION
    Runs end to end with no operator interaction. `pixi run release minor` IS
    the authorization. `pixi run` allocates no TTY, so any stdin read here dies
    with "IOException: The handle is invalid" and takes the release with it.

    Safety comes from deterministic preconditions instead: on main, clean tree,
    tag absent, semver valid, notices in sync, and a changelog that has
    something to say. Each fails fast with a non-zero exit before any version
    file is touched. Nothing here force-pushes, amends or overwrites a tag.

.PARAMETER Version
    major | minor | patch | nightly | X.Y.Z

.PARAMETER Force
    Ship even when every commit since the last tag was filtered as noise
    (writes a maintenance changelog entry instead of aborting).
#>

[CmdletBinding()]
param(
    # Not Mandatory: PowerShell fills a missing mandatory parameter by reading
    # stdin, which throws under pixi instead of printing a usage line.
    [Parameter(Position = 0)][string]$Version,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))

if (-not $Version) {
    Write-Host 'Usage: pixi run release <major|minor|patch|nightly|X.Y.Z> [-Force]' -ForegroundColor Red
    exit 1
}

if ($Version -eq 'nightly') {
    & (Join-Path $PSScriptRoot 'release-nightly.ps1')
    exit $LASTEXITCODE
}

Import-Module (Join-Path $root 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force

# Windows PowerShell 5.1's Set-Content -Encoding UTF8 writes a BOM, which pixi
# rejects in pixi.toml and serde_json rejects in launcher-manifest.json. Raw
# .NET writes also keep each file's existing line endings, which matters for
# the CRLF-only install.cmd.
function Set-TextFileNoBom {
    param([string]$Path, [string]$Text)
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding $false))
}

function Update-VersionInFile {
    param([string]$Path, [string]$Pattern, [string]$Replacement)
    $full = Join-Path $root $Path
    $text = [System.IO.File]::ReadAllText($full)
    $updated = [regex]::Replace($text, $Pattern, $Replacement)
    if ($updated -eq $text) { throw "Version stamp did not match anything in $Path" }
    Set-TextFileNoBom -Path $full -Text $updated
}

# Mirrors New-ChangelogFromCommits' insertion so a -Force maintenance entry
# lands in the same place with the same shape.
function Add-MaintenanceChangelogEntry {
    param([string]$Path, [string]$NewVersion)
    $full = Join-Path $root $Path
    $date = Get-Date -Format 'yyyy-MM-dd'
    $entry = "## [$NewVersion] - $date`n`n### Changed`n`n- Maintenance release (no user-facing changes).`n`n"
    $changelog = [System.IO.File]::ReadAllText($full)
    $changelog = $changelog -replace '(?s)(# Changelog.*?\n\n)', "`$1$entry"
    Set-TextFileNoBom -Path $full -Text ($changelog.TrimEnd() + "`n")
}

Push-Location $root
try {
    # manifest.json is canonical: package-release.ps1 names the ZIP from it and
    # release.yml checks the tag against it.
    $current = Get-ProjectVersion -Source 'manifest' -Path 'manifest.json'
    $new = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $current
    if (-not (Test-SemanticVersion -Version $new)) { throw "Release version must be X.Y.Z, got '$new'." }

    # New-ReleaseTag pushes to main, so releasing from any other branch would
    # push commits that branch does not contain.
    $branch = (& git rev-parse --abbrev-ref HEAD).Trim()
    if ($branch -ne 'main') { throw "Releases are cut from 'main' only; currently on '$branch'." }
    if (-not (Test-CleanGitStatus)) { throw 'Working tree is dirty - commit or stash first.' }
    if (Test-GitTagExists -Tag "v$new") { throw "Tag v$new already exists." }

    # THIRD-PARTY-NOTICES.md names the cameraunlock-core commit compiled into
    # the ZIP, and a submodule bump does not touch it. Copy-SharedBundle refuses
    # to package that mismatch, so re-sync here rather than failing inside
    # `pixi run package` or, worse, in CI after the tag is pushed.
    & (Join-Path $root 'cameraunlock-core\scripts\sync-core-notices.ps1') -Repo $root
    if ($LASTEXITCODE -ne 0) { throw "sync-core-notices.ps1 exited $LASTEXITCODE - fix THIRD-PARTY-NOTICES.md before releasing." }
    & git diff --quiet -- THIRD-PARTY-NOTICES.md
    if ($LASTEXITCODE -ne 0) {
        & git commit -q -m 'chore: record the cameraunlock-core commit this build compiles' -- THIRD-PARTY-NOTICES.md
        if ($LASTEXITCODE -ne 0) { throw 'Could not commit the re-synced THIRD-PARTY-NOTICES.md.' }
        Write-Host 'THIRD-PARTY-NOTICES.md re-synced to the pinned cameraunlock-core commit.' -ForegroundColor Yellow
    }

    # Changelog before any version file: this is the gate that aborts when every
    # commit since the last tag is noise, and failing here leaves a clean tree
    # instead of a half-applied bump with no tag.
    try {
        New-ChangelogFromCommits -ChangelogPath 'CHANGELOG.md' -Version $new -ArtifactPaths @(
            'src/', 'cameraunlock-core/', 'DyingLightHeadTracking.ini',
            'launcher-manifest.json', 'scripts/install.cmd', 'scripts/uninstall.cmd'
        ) | Out-Null
    } catch {
        if (-not $Force) {
            Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
            Write-Host 'No user-facing changes to release. Re-run with -Force for a maintenance release.' -ForegroundColor Yellow
            exit 1
        }
        Write-Host 'No user-facing commits since last tag - writing maintenance entry (-Force).' -ForegroundColor Yellow
        Add-MaintenanceChangelogEntry -Path 'CHANGELOG.md' -NewVersion $new
    }

    # manifest.json is canonical; the rest are hand-kept copies. CMakeLists.txt
    # bakes MOD_VERSION into the ASI, install.cmd prints it to the player, and
    # launcher-manifest.json is what Lopari reads.
    $versionFiles = @('manifest.json', 'CMakeLists.txt', 'pixi.toml', 'launcher-manifest.json', 'scripts/install.cmd', 'CHANGELOG.md')
    Update-VersionInFile -Path 'manifest.json' -Pattern '(?m)^(\s*"version":\s*)"[0-9.]+"' -Replacement "`${1}`"$new`""
    Update-VersionInFile -Path 'CMakeLists.txt' -Pattern 'project\(DyingLightHeadTracking VERSION [0-9.]+' -Replacement "project(DyingLightHeadTracking VERSION $new"
    Update-VersionInFile -Path 'pixi.toml' -Pattern '(?m)^version = "[0-9.]+"' -Replacement "version = `"$new`""
    Update-VersionInFile -Path 'launcher-manifest.json' -Pattern '(?s)("mod_info":\s*\{.*?"version":\s*)"[0-9.]+"' -Replacement "`${1}`"$new`""
    Update-VersionInFile -Path 'scripts/install.cmd' -Pattern '(?m)^set "MOD_VERSION=[0-9.]+"' -Replacement "set `"MOD_VERSION=$new`""

    # The same chain CI runs (setup -> build-release -> test -> package), so the
    # ZIP this version ships is proven buildable before the tag exists.
    & pixi run package
    if ($LASTEXITCODE -ne 0) { throw "pixi run package failed (exit $LASTEXITCODE)." }

    # Not Invoke-VersionCommit: it hardcodes "chore: bump version to X", and the
    # "Release v" subject is what the changelog noise filter and CI key on.
    foreach ($f in $versionFiles) {
        & git add -- $f
        if ($LASTEXITCODE -ne 0) { throw "git add failed for $f" }
    }
    if (-not (& git diff --cached --name-only)) { throw 'Version stamping produced no staged changes.' }
    & git commit -m "Release v$new"
    if ($LASTEXITCODE -ne 0) { throw 'Failed to commit the release.' }

    New-ReleaseTag -Version $new -Message "Release v$new"
    Write-Host "Released v$new" -ForegroundColor Green
} finally {
    Pop-Location
}
