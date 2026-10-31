# Puts the release build together as dist\<name>\ and dist\<name>.zip: the executable, the SDK
# runtime DLLs, README and license texts. No game files: players supply their own disc image or
# extracted disc. The Remix profile is chosen in the game (PC SETTINGS).
#
# Usage (after building nfsc with the win-amd64-release preset):
#   powershell -File tools\package-release.ps1 [-Version 0.75]

param([string]$Version = "0.75")

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root "nfsc\out\build\win-amd64-release"
$sdk = Join-Path $root "sdk\rexglue-sdk"
$name = "NFS-Carbon-CE-Recompiled-v$Version"
$dist = Join-Path $root "dist"
$out = Join-Path $dist $name

# A failed configure once left the release build at -O0 (see the ROADMAP); refuse that.
$flags = Select-String -Path (Join-Path $build "CMakeCache.txt") -Pattern "^CMAKE_CXX_FLAGS_RELEASE:STRING=(.*)$"
if (-not $flags -or $flags.Matches[0].Groups[1].Value -notmatch "-O3") {
  throw "The release build is not optimized (CMAKE_CXX_FLAGS_RELEASE has no -O3)."
}

if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null
New-Item -ItemType Directory -Force (Join-Path $out "licenses") | Out-Null

foreach ($file in "nfsc.exe", "rexruntime.dll", "rexgpu-xenos.dll", "amd_fidelityfx_vk.dll") {
  Copy-Item (Join-Path $build $file) $out
}
Copy-Item (Join-Path $root "README.md") $out
Copy-Item (Join-Path $root "LICENSE") (Join-Path $out "LICENSE.txt")
Copy-Item (Join-Path $root "docs\release-notes-v$Version.md") (Join-Path $out "RELEASE-NOTES.md")

# License texts: the SDK and the third-party libraries it builds in.
Copy-Item (Join-Path $sdk "LICENSE") (Join-Path $out "licenses\rexglue-sdk.txt")
Get-ChildItem (Join-Path $sdk "thirdparty") -Directory | ForEach-Object {
  $license = Get-ChildItem $_.FullName -File |
    Where-Object { $_.Name -match '^(LICENSE|LICENCE|COPYING)' } | Select-Object -First 1
  if ($license) {
    Copy-Item $license.FullName (Join-Path $out "licenses\$($_.Name).txt")
  }
}
# Licenses that aren't at the top of their folder.
$extra = @{
  "libmspack.txt"  = "thirdparty\libmspack\libmspack\COPYING.LIB"
  "fidelityfx.md"  = "out\build\win-amd64\_deps\fidelityfx-src\docs\license.md"
}
foreach ($entry in $extra.GetEnumerator()) {
  $source = Join-Path $sdk $entry.Value
  if (-not (Test-Path $source)) { throw "License text not found: $source" }
  Copy-Item $source (Join-Path $out "licenses\$($entry.Key)")
}

$zip = "$out.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip
Get-ChildItem $out -Recurse -File | Measure-Object -Property Length -Sum |
  ForEach-Object { "{0} files, {1:N1} MB -> {2}" -f $_.Count, ($_.Sum / 1MB), $zip }
