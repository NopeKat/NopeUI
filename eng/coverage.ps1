[CmdletBinding()]
param(
    [ValidateSet("x64-debug", "x64-release")]
    [string]$Preset = "x64-debug",
    [string]$ToolPath,
    [switch]$SkipBuild,
    [string]$OutputDir
)

# Runs the unit tests under OpenCppCoverage and writes an HTML report. This is a
# manual script: it needs a coverage tool that is not vendored, so it stops with
# instructions instead of downloading anything by itself.
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$configuration = if ($Preset -like "*debug") { "Debug" } else { "Release" }
$buildDir = Join-Path $repoRoot ".build\$Preset"
if (-not $OutputDir) {
    $OutputDir = Join-Path $repoRoot ".build\coverage"
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset
}

$tool = $ToolPath
if (-not $tool) {
    $command = Get-Command OpenCppCoverage -ErrorAction SilentlyContinue
    if ($command) {
        $tool = $command.Source
    }
}
if (-not $tool) {
    throw @"
OpenCppCoverage was not found. Install it from https://github.com/OpenCppCoverage/OpenCppCoverage
and either put it on PATH or pass -ToolPath. This script never downloads tools on its own.
"@
}

$ctest = Join-Path $buildDir "CTestTestfile.cmake"
if (-not (Test-Path -LiteralPath $ctest)) {
    throw "tests are not configured in $buildDir; run cmake --preset $Preset first"
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$html = Join-Path $OutputDir "html"
$xml = Join-Path $OutputDir "cobertura.xml"

& $tool `
    --sources (Join-Path $repoRoot "src") `
    --excluded_sources (Join-Path $repoRoot "src\platform\automation.cpp") `
    --export_type "html:$html" `
    --export_type "cobertura:$xml" `
    --cover_children `
    -- (Join-Path $env:SystemRoot "System32\cmd.exe") /c "ctest --test-dir $buildDir -C $configuration --output-on-failure"
if ($LASTEXITCODE -ne 0) {
    throw "coverage run failed with exit code $LASTEXITCODE"
}

Write-Output "coverage report: $html"
Write-Output "cobertura xml:  $xml"
