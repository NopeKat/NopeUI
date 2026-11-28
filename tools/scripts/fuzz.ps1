[CmdletBinding()]
param(
    [ValidateSet("x64-debug", "x64-release")]
    [string]$Preset = "x64-release",
    [double]$Hours = 24.0,
    [switch]$SkipBuild,
    [uint64]$Seed = 1,
    [string]$Log = ".build\fuzz\run.log",
    [string]$CrashFile = ".build\fuzz\crash.xaml",
    [string]$Diagnostics = ".build\fuzz\diagnostics"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$configuration = if ($Preset -like "*debug") { "Debug" } else { "Release" }
$fuzzer = Join-Path $repoRoot ".build\$Preset\tools\$configuration\nopeui_xaml_fuzz.exe"
$logPath = if ([System.IO.Path]::IsPathRooted($Log)) { $Log } else { Join-Path $repoRoot $Log }
$crashPath = if ([System.IO.Path]::IsPathRooted($CrashFile)) { $CrashFile } else { Join-Path $repoRoot $CrashFile }
$diagnosticsRoot = if ([System.IO.Path]::IsPathRooted($Diagnostics)) { $Diagnostics } else { Join-Path $repoRoot $Diagnostics }

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Preset $Preset
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for preset '$Preset'."
    }
}

if (-not (Test-Path -LiteralPath $fuzzer)) {
    throw "fuzzer not found: $fuzzer (drop -SkipBuild or build preset '$Preset')."
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $crashPath) | Out-Null
if (Test-Path -LiteralPath $crashPath) {
    Remove-Item -LiteralPath $crashPath -Force
}

# The fuzzer runs in fixed slices so the log gets a progress line even when the
# sweep lasts all night, and a fresh process keeps the heap state clean.
$slice = 1800.0
$deadline = (Get-Date).AddHours($Hours)
$iterations = [uint64]0
$parsed = [uint64]0
$sliceIndex = 0
$missingSlices = 0

Write-Output ("fuzzing for {0:N2} h from preset '{1}', log: {2}" -f $Hours, $Preset, $logPath)

while ((Get-Date) -lt $deadline) {
    $remaining = ($deadline - (Get-Date)).TotalSeconds
    $seconds = [math]::Max(1.0, [math]::Min($slice, $remaining))
    $sliceSeed = $Seed + [uint64]$sliceIndex

    $sliceDiagnostics = Join-Path $diagnosticsRoot ("slice-" + $sliceIndex)
    $output = & $fuzzer --seconds ([math]::Floor($seconds)) --seed $sliceSeed `
        --seed-dir (Join-Path $repoRoot 'samples\xaml_smoke') --crash $crashPath `
        --diagnostics-dir $sliceDiagnostics 2>&1
    $code = $LASTEXITCODE
    $output | Tee-Object -FilePath $logPath -Append | Out-Null

    $done = $output | Select-String -Pattern '^done: iterations=' | Select-Object -Last 1
    if ($done -and $done.Line -match 'iterations=(\d+) parse_ok=(\d+) load_ok=(\d+)') {
        $iterations += [uint64]$Matches[1]
        $parsed += [uint64]$Matches[2]
    } elseif ($code -eq 0) {
        $missingSlices += 1
        Write-Output ("WARN: slice {0} produced no done line; progress counters stay unchanged" -f $sliceIndex)
    }

    if ($code -ne 0) {
        Write-Output ("FAIL: fuzzer exited with {0}; input kept in {1}; diagnostics in {2}" -f `
            $code, $crashPath, $sliceDiagnostics)
        exit 1
    }
    $sliceIndex += 1
}

if ($missingSlices -gt 0 -or $iterations -eq 0) {
    Write-Output ("FAIL: {0} slice(s) without a done line, {1} inputs counted; the run is not evidence" -f `
        $missingSlices, $iterations)
    exit 1
}

Write-Output ("PASS: {0} inputs, {1} parsed, no crash after {2:N2} h" -f $iterations, $parsed, $Hours)
