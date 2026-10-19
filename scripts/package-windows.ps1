# SPDX-License-Identifier: GPL-3.0-or-later
# Build a distributable Windows x64 folder + zip from build-rel/Release.
# Usage (from repo root, after a Release build):
#   powershell -File scripts/package-windows.ps1
param(
    [string]$BuildDir = "build-rel\Release",
    [string]$OutRoot = "dist"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

$verLine = (Select-String -Path CMakeLists.txt -Pattern 'project\(Fission360 VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$tag = "Fission360-$verLine-windows-x64"
$src = Join-Path $root $BuildDir
$dst = Join-Path $root "$OutRoot\$tag"
$zip = Join-Path $root "$OutRoot\$tag.zip"

if (-not (Test-Path (Join-Path $src "Fission360.exe"))) {
    throw "Missing $src\Fission360.exe - build Release first."
}

if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
New-Item -ItemType Directory -Force $dst | Out-Null

robocopy $src $dst /E /XD CMakeFiles /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed: $LASTEXITCODE" }

# Safety net: pull a complete OCCT runtime if the build tree is missing TKBRep.dll etc.
function Find-OrcaRoot {
    foreach ($c in @(
        $env:FISSION_ORCA_ROOT,
        (Join-Path $root "..\MyOrcaSlicer"),
        (Join-Path $env:USERPROFILE "Documents\GitHub\MyOrcaSlicer")
    )) {
        if ($c -and (Test-Path (Join-Path $c "deps\build\dep_OCCT-prefix"))) { return $c }
    }
    return $null
}
if (-not (Test-Path (Join-Path $dst "TKBRep.dll"))) {
    $orca = Find-OrcaRoot
    $occtBin = $null
    foreach ($c in @(
        (Join-Path $orca "deps\build\OrcaSlicer_dep\usr\local\bin\occt"),
        (Join-Path $orca "OrcaSlicer_dep\usr\local\bin\occt"),
        (Join-Path $orca "dist\MyOrcaSlicer-portable"),
        (Join-Path $orca "build-portable\src\Release")
    )) {
        if ($c -and (Test-Path (Join-Path $c "TKBRep.dll"))) { $occtBin = $c; break }
    }
    if (-not $occtBin) { throw "TKBRep.dll missing from $dst and no complete OCCT runtime found under FISSION_ORCA_ROOT." }
    Write-Host "Filling OCCT DLLs from $occtBin"
    Copy-Item (Join-Path $occtBin "TK*.dll") $dst -Force
    $ft = Join-Path (Split-Path $occtBin) "freetype.dll"
    if (-not (Test-Path $ft)) { $ft = Join-Path $occtBin "freetype.dll" }
    if (-not (Test-Path $ft)) { $ft = Join-Path $orca "dist\MyOrcaSlicer-portable\freetype.dll" }
    if (Test-Path $ft) { Copy-Item $ft $dst -Force }
}
if (-not (Test-Path (Join-Path $dst "TKBRep.dll"))) {
    throw "Package is still missing TKBRep.dll - refusing to ship a broken zip."
}

Copy-Item LICENSE, THIRD_PARTY_NOTICES.md, README.md $dst -Force
@"
Fission 360 $verLine (Windows x64)

Run Fission360.exe in this folder. Do not move the exe without its DLL/QML/WebEngine files.

License: LICENSE (GPL-3.0-or-later)
Third-party: THIRD_PARTY_NOTICES.md
Docs: README.md

Source for this build: repository tag / commit matching version $verLine
"@ | Set-Content (Join-Path $dst "README-RELEASE.txt") -Encoding UTF8

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $dst -DestinationPath $zip -CompressionLevel Optimal

$exe = Get-Item (Join-Path $dst "Fission360.exe")
Write-Host "Packaged: $dst"
Write-Host "Zip:      $zip  ($([math]::Round((Get-Item $zip).Length/1MB,1)) MB)"
Write-Host "Exe:      $($exe.LastWriteTime)  $($exe.Length) bytes"
