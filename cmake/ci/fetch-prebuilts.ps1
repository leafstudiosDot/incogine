# Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
<#
.SYNOPSIS
    Downloads the Windows prebuilt SDL3 development packages into reqs/.

.DESCRIPTION
    Windows links SDL3 and the addons against the prebuilt import libraries in
    reqs/SDL3, reqs/SDL3_ttf, reqs/SDL3_image and reqs/SDL3_mixer (see the
    "Prebuilt SDL3 dependencies" block in the top-level CMakeLists.txt). Those
    directories are gitignored, so CI has to fetch them the same way a local
    Windows setup does: unpack the official *-devel-*-VC.zip release archives
    straight from the upstream GitHub releases.

    Versions and URLs are derived from .gitmodules, so bumping a branch there
    (e.g. `branch = release-3.4.14`) is enough to bump the prebuilt too and the
    CI build always tests the same versions a local Windows setup uses.

    The archives ship a single top-level folder (SDL3-3.4.14/...); it is
    stripped so that reqs/SDL3/include, reqs/SDL3/lib/x64 and the DLLs sitting
    next to the import libraries land exactly where the build looks for them.
#>
[CmdletBinding()]
param(
    # Where the <lib>/include + <lib>/lib layout is unpacked. Overridable so the
    # script can be smoke-tested without touching the checkout's reqs/.
    [string] $ReqsRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $ReqsRoot) { $ReqsRoot = Join-Path $repoRoot 'reqs' }
$reqsRoot = $ReqsRoot
$modulesFile = Join-Path $repoRoot '.gitmodules'

if (-not (Test-Path -LiteralPath $modulesFile)) {
    throw "fetch-prebuilts: $modulesFile not found"
}

function Read-Submodules {
    # Parse [submodule "name"] blocks straight out of .gitmodules so the script
    # does not depend on git being on PATH (the workflow runs it on a checkout
    # that has one, but PowerShell-only parsing keeps this usable standalone).
    $entries = [ordered]@{}
    $current = $null
    foreach ($line in Get-Content -LiteralPath $modulesFile) {
        if ($line -match '^\s*\[submodule\s+"([^"]+)"\]\s*$') {
            $current = $Matches[1]
            $entries[$current] = [ordered]@{}
        }
        elseif ($current -and $line -match '^\s*([\w-]+)\s*=\s*(.+?)\s*$') {
            $key = $Matches[1]
            $entries[$current][$key] = $Matches[2]
        }
    }
    return $entries
}

function Invoke-Download {
    # GitHub release assets sit behind a CDN redirect that occasionally resets or
    # answers 404 on the first attempt, so retry a few times before giving up.
    param(
        [string] $Uri,
        [string] $OutFile,
        [int] $Attempts = 5
    )
    for ($i = 1; $i -le $Attempts; $i++) {
        try {
            Invoke-WebRequest -Uri $Uri -OutFile $OutFile -UseBasicParsing -ErrorAction Stop
            return
        }
        catch {
            if ($i -eq $Attempts) { throw }
            Write-Warning "fetch-prebuilts: download attempt $i/$Attempts failed ($($_.Exception.Message)) - retrying"
            Start-Sleep -Seconds (5 * $i)
        }
    }
}

$workDir = Join-Path ([System.IO.Path]::GetTempPath()) ('incogine-sdl-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $workDir | Out-Null

try {
    $submodules = Read-Submodules
    foreach ($name in $submodules.Keys) {
        $entry = $submodules[$name]
        $path = $entry['path']
        $url = $entry['url']
        $branch = $entry['branch']

        # Only the reqs/<lib>_source entries describe a *source* checkout; the
        # prebuilt archive lives on the matching <lib> repository's releases.
        if (-not $path -or $path -notmatch '^reqs[/\\](?<lib>.+)_source$') { continue }
        $lib = $Matches['lib']

        if (-not $url -or $url -notmatch 'github\.com[:/](?<owner>[^/]+)/') { continue }
        $owner = $Matches['owner']

        # release-3.4.14 -> 3.4.14
        $version = "$branch" -replace '^release-', ''
        if (-not $version) {
            throw "fetch-prebuilts: submodule '$name' has no release-* branch (got '$branch')"
        }

        $dest = Join-Path $reqsRoot $lib
        if (Test-Path (Join-Path $dest 'include')) {
            Write-Host "fetch-prebuilts: reqs/$lib already present - skipping"
            continue
        }

        $asset = "$lib-devel-$version-VC.zip"
        $assetUrl = "https://github.com/$owner/$lib/releases/download/$branch/$asset"
        $archive = Join-Path $workDir $asset

        Write-Host "fetch-prebuilts: $assetUrl"
        Invoke-Download -Uri $assetUrl -OutFile $archive

        $extract = Join-Path $workDir ([System.IO.Path]::GetFileNameWithoutExtension($asset))
        Expand-Archive -LiteralPath $archive -DestinationPath $extract -Force

        # Strip the single top-level directory the archive is wrapped in.
        $top = @(Get-ChildItem -LiteralPath $extract -Directory)
        if ($top.Count -ne 1) {
            throw "fetch-prebuilts: expected exactly one top-level dir in $asset, found $($top.Count)"
        }
        if (Test-Path -LiteralPath $dest) {
            Remove-Item -LiteralPath $dest -Recurse -Force
        }
        New-Item -ItemType Directory -Path $dest -Force | Out-Null
        Copy-Item -Path (Join-Path $top[0].FullName '*') -Destination $dest -Recurse -Force

        # Sanity-check the layout the top-level CMakeLists.txt hardcodes.
        foreach ($required in @('include', 'lib\x64')) {
            if (-not (Test-Path (Join-Path $dest $required))) {
                throw "fetch-prebuilts: $asset did not provide reqs/$lib/$required"
            }
        }

        # Older SDL3_mixer VC packages ship SDL_mixer.h at the include root
        # instead of include/SDL3_mixer/. The engine includes the namespaced
        # path, so recreate the one-line shim (see CLAUDE.md, "Windows
        # prebuilts note") if the namespaced header is missing.
        if ($lib -eq 'SDL3_mixer') {
            $namespaced = Join-Path $dest 'include\SDL3_mixer\SDL_mixer.h'
            $legacy = Join-Path $dest 'include\SDL_mixer.h'
            if ((Test-Path -LiteralPath $legacy) -and -not (Test-Path -LiteralPath $namespaced)) {
                New-Item -ItemType Directory -Path (Split-Path -Parent $namespaced) -Force | Out-Null
                @(
                    '// Shim: this prebuilt SDL3_mixer package ships SDL_mixer.h at the include'
                    '// root (legacy layout). Map the canonical namespaced include to it.'
                    '#pragma once'
                    '#include <SDL_mixer.h>'
                ) | Set-Content -LiteralPath $namespaced -Encoding utf8
                Write-Host "fetch-prebuilts: recreated reqs/SDL3_mixer/include/SDL3_mixer/SDL_mixer.h shim"
            }
        }

        Write-Host "fetch-prebuilts: unpacked reqs/$lib ($version)"
    }
}
finally {
    Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue
}
