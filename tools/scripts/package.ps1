[CmdletBinding()]
param(
    [ValidateSet("x64-release")]
    [string]$Preset = "x64-release",
    [string]$Out = ".build\package",
    [switch]$SkipBuild,
    [switch]$Smoke,
    # Local dry runs may skip the clean-tree rule; a package meant for others
    # must be built from a committed tree.
    [switch]$AllowDirty
)

# Assembles the preview package: headers, x64 static libraries built against
# the dynamic CRT, the two sample apps, the docs, a source archive and a
# checksum list, all inside one portable zip. The working tree has to be clean
# so the source archive matches the binaries that were just built.

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$buildDir = Join-Path $repoRoot ".build\$Preset"
$outDir = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $repoRoot $Out }

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Preset $Preset
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for preset '$Preset'."
    }
}

$status = & git -C $repoRoot status --porcelain
if ($LASTEXITCODE -ne 0) {
    throw "git status failed under '$repoRoot'."
}
if (-not $AllowDirty -and -not [string]::IsNullOrWhiteSpace($status)) {
    throw "The working tree is not clean; commit before packaging so the source archive matches the binaries."
}
if ($AllowDirty -and -not [string]::IsNullOrWhiteSpace($status)) {
    Write-Output "warning: packaging from a dirty tree, the source archive comes from HEAD"
}

$versionSource = Get-Content -LiteralPath (Join-Path $repoRoot 'src\base\version.cpp') -Raw
if ($versionSource -notmatch 'return\s+"([0-9]+(?:\.[0-9]+)+)"') {
    throw "Cannot read the version string from src/base/version.cpp."
}
$version = $Matches[1]
$packageName = "NopeUI-$version-win-x64"
$zipPath = Join-Path $outDir "$packageName.zip"

$stageRoot = Join-Path $outDir $packageName
if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null

# Headers and libraries.
Copy-Item -LiteralPath (Join-Path $repoRoot 'include\nopeui') `
    -Destination (Join-Path $stageRoot 'include\nopeui') -Recurse
$libTarget = Join-Path $stageRoot 'lib\x64\static\md'
New-Item -ItemType Directory -Force -Path $libTarget | Out-Null
$libraries = Get-ChildItem -LiteralPath (Join-Path $buildDir 'Release') -Filter 'nopeui_*.lib' -File
if ($libraries.Count -eq 0) {
    throw "No libraries under '$buildDir\Release'; build preset '$Preset' first."
}
$expectedLibraries = @(
    'nopeui_base', 'nopeui_theme', 'nopeui_layout', 'nopeui_graphics',
    'nopeui_controls', 'nopeui_platform', 'nopeui_markup'
)
foreach ($name in $expectedLibraries) {
    if (-not ($libraries | Where-Object { $_.BaseName -eq $name })) {
        throw "Missing library: $name.lib"
    }
}
foreach ($library in $libraries) {
    Copy-Item -LiteralPath $library.FullName -Destination $libTarget
}

# Sample apps and the page the xaml sample loads.
$binTarget = Join-Path $stageRoot 'bin\x64'
New-Item -ItemType Directory -Force -Path $binTarget | Out-Null
foreach ($sample in @('nopeui_window_smoke', 'nopeui_xaml_smoke')) {
    $exe = Join-Path $buildDir "samples\Release\$sample.exe"
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Missing sample binary: $exe"
    }
    Copy-Item -LiteralPath $exe -Destination $binTarget
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'samples\xaml_smoke\page.xaml') -Destination $binTarget

# Documents and the AGPL source archive.
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination $stageRoot
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination $stageRoot
New-Item -ItemType Directory -Force -Path (Join-Path $stageRoot 'docs') | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\REPRODUCE.md') `
    -Destination (Join-Path $stageRoot 'docs\REPRODUCE.md')

$sourceDir = Join-Path $stageRoot 'source'
New-Item -ItemType Directory -Force -Path $sourceDir | Out-Null
$sourceZip = Join-Path $sourceDir "nopeui-$version-source.zip"
& git -C $repoRoot archive --format=zip --prefix="nopeui-$version/" -o $sourceZip HEAD
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $sourceZip)) {
    throw "git archive failed; the source archive was not written."
}

# Variant notes: the preview ships one variant only.
$info = @(
    "NopeUI $version preview package",
    "",
    "- build date: $(Get-Date -Format 'yyyy-MM-dd HH:mm')",
    "- variant: x64, static libraries, dynamic CRT (/MD)",
    "- toolchain: CMake 4.4.4, MSVC 14.52.37025, Windows SDK 10.0.26100.0",
    "- target machine needs the VC++ x64 runtime",
    "- the sample apps run straight from bin\x64; page.xaml must stay beside nopeui_xaml_smoke.exe"
)
Set-Content -LiteralPath (Join-Path $stageRoot 'package-info.txt') -Value $info -Encoding utf8

# Checks: layout first, then the zip, then an optional launch.
$expected = @(
    'include\nopeui\markup\xaml.h',
    'lib\x64\static\md\nopeui_base.lib',
    'bin\x64\nopeui_window_smoke.exe',
    'bin\x64\nopeui_xaml_smoke.exe',
    'bin\x64\page.xaml',
    'docs\REPRODUCE.md',
    'README.md',
    'LICENSE',
    'package-info.txt',
    "source\nopeui-$version-source.zip"
)
foreach ($relative in $expected) {
    if (-not (Test-Path -LiteralPath (Join-Path $stageRoot $relative))) {
        throw "Package check failed: missing $relative"
    }
}

if ($Smoke) {
    $process = Start-Process -FilePath (Join-Path $binTarget 'nopeui_window_smoke.exe') -PassThru
    Start-Sleep -Seconds 3
    if ($process.HasExited) {
        throw "Smoke check failed: nopeui_window_smoke exited with $($process.ExitCode)."
    }
    Stop-Process -Id $process.Id -Force
    Write-Output "smoke: window stayed up for 3 s"
}

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path $stageRoot -DestinationPath $zipPath -CompressionLevel Optimal
if (-not (Test-Path -LiteralPath $zipPath)) {
    throw "Package zip was not written: $zipPath"
}

# Checksums cover what a preview user actually downloads plus the binaries.
$hashes = @()
$hashes += (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256)
foreach ($name in @('nopeui_window_smoke.exe', 'nopeui_xaml_smoke.exe')) {
    $hashes += (Get-FileHash -LiteralPath (Join-Path $binTarget $name) -Algorithm SHA256)
}
$checksumPath = Join-Path $outDir "$packageName.sha256"
Set-Content -LiteralPath $checksumPath -Encoding utf8 -Value (
    $hashes | ForEach-Object { "{0}  {1}" -f $_.Hash.ToLower(), (Split-Path -Leaf $_.Path) })

$zipSize = [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB, 1)
Write-Output "package: $zipPath"
Write-Output "size: $zipSize MB"
Write-Output "checksums: $checksumPath"
Write-Output "PASS: package checks completed"
