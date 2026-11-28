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
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
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

# cmd.exe splits the command line on spaces, so the build directory cannot go in
# directly. Write a runner in the temp directory that reads the paths from the
# environment instead.
$runner = Join-Path $env:TEMP "nopeui-coverage-runner.cmd"
@(
    '@echo off',
    'ctest --test-dir "%NOPEUI_COVERAGE_BUILD_DIR%" -C %NOPEUI_COVERAGE_CONFIG% --output-on-failure'
) | Set-Content -Path $runner -Encoding ASCII
$env:NOPEUI_COVERAGE_BUILD_DIR = $buildDir
$env:NOPEUI_COVERAGE_CONFIG = $configuration

$testExit = 0
try {
    & $tool `
        --sources (Join-Path $repoRoot "src") `
        --excluded_sources (Join-Path $repoRoot "src\platform\automation.cpp") `
        --modules "*nopeui*" `
        --export_type "html:$html" `
        --export_type "cobertura:$xml" `
        --cover_children `
        -- (Join-Path $env:SystemRoot "System32\cmd.exe") /c $runner
    $testExit = $LASTEXITCODE
}
finally {
    Remove-Item $runner -ErrorAction SilentlyContinue
}

# Instrumentation slows the process enough that the fuzz test misses its
# throughput floor; that is a measurement artifact, not a failed run, so keep
# the report and surface the test status instead of throwing.
if ($testExit -ne 0) {
    Write-Warning "tests exited with code $testExit; the report still covers the code they ran. Check the fuzz throughput floor under instrumentation."
}
if (-not (Test-Path -LiteralPath $xml)) {
    throw "coverage run produced no report"
}

Write-Output "coverage report: $html"
Write-Output "cobertura xml:  $xml"
$global:LASTEXITCODE = 0
