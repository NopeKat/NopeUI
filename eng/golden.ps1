[CmdletBinding()]
param(
    [ValidateSet("x64-debug", "x64-release")]
    [string]$Preset = "x64-release",
    [switch]$Update,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$configuration = if ($Preset -like "*debug") { "Debug" } else { "Release" }
$toolDir = Join-Path $repoRoot ".build\$Preset\tools\$configuration"
$goldenExe = Join-Path $toolDir "nopeui_golden.exe"
$diffExe = Join-Path $toolDir "nopeui_image_diff.exe"
$currentDir = Join-Path $repoRoot ".build\golden"
$diffDir = Join-Path $repoRoot ".build\golden\diff"
$baselineDir = Join-Path $repoRoot "tests\golden\baseline"

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset
}

foreach ($path in @($currentDir, $diffDir, $baselineDir)) {
    New-Item -ItemType Directory -Path $path -Force | Out-Null
}

$cases = @(
    @{ Name = "basic-light-1280x800"; Theme = "light"; Width = 1280; Height = 800 },
    @{ Name = "basic-dark-1280x800"; Theme = "dark"; Width = 1280; Height = 800 }
)

$failed = $false
foreach ($case in $cases) {
    $currentPath = Join-Path $currentDir "$($case.Name).png"
    & $goldenExe --out $currentPath --theme $case.Theme --width $case.Width --height $case.Height
    if ($LASTEXITCODE -ne 0) {
        throw "golden render failed: $($case.Name)"
    }

    $baselinePath = Join-Path $baselineDir "$($case.Name).png"
    if ($Update) {
        Copy-Item -LiteralPath $currentPath -Destination $baselinePath -Force
        Write-Output "updated: $($case.Name)"
        continue
    }

    if (-not (Test-Path -LiteralPath $baselinePath)) {
        Write-Output "missing baseline: $baselinePath (run with -Update to create it)"
        $failed = $true
        continue
    }

    $diffPath = Join-Path $diffDir "$($case.Name).png"
    & $diffExe $baselinePath $currentPath --diff $diffPath
    if ($LASTEXITCODE -ne 0) {
        Write-Output "FAILED: $($case.Name) (diff image: $diffPath)"
        $failed = $true
    } else {
        Write-Output "PASS: $($case.Name)"
    }
}

if ($failed) {
    exit 1
}
