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
$xamlPath = Join-Path $repoRoot "samples\xaml_smoke\page.xaml"

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset
}

foreach ($path in @($currentDir, $diffDir, $baselineDir)) {
    New-Item -ItemType Directory -Path $path -Force | Out-Null
}

$cases = @(
    @{ Name = "basic-light-1280x800"; Renderer = "software"; Theme = "light"; Scene = "placeholder"; Width = 1280; Height = 800 },
    @{ Name = "basic-dark-1280x800"; Renderer = "software"; Theme = "dark"; Scene = "placeholder"; Width = 1280; Height = 800 },
    @{ Name = "hardware-light-1280x800"; Renderer = "hardware"; Theme = "light"; Scene = "placeholder"; Width = 1280; Height = 800 },
    @{ Name = "hardware-dark-1280x800"; Renderer = "hardware"; Theme = "dark"; Scene = "placeholder"; Width = 1280; Height = 800 },
    @{ Name = "controls-light-1280x800"; Renderer = "software"; Theme = "light"; Scene = "controls"; Width = 1280; Height = 800 },
    @{ Name = "controls-dark-1280x800"; Renderer = "software"; Theme = "dark"; Scene = "controls"; Width = 1280; Height = 800 },
    @{ Name = "controls-contrast-1280x800"; Renderer = "software"; Theme = "contrast"; Scene = "controls"; Width = 1280; Height = 800 },
    @{ Name = "controls-contrast-hardware-1280x800"; Renderer = "hardware"; Theme = "contrast"; Scene = "controls"; Width = 1280; Height = 800 },
    @{ Name = "form-light-1280x800"; Renderer = "software"; Theme = "light"; Scene = "form"; Width = 1280; Height = 800 },
    @{ Name = "form-dark-1280x800"; Renderer = "software"; Theme = "dark"; Scene = "form"; Width = 1280; Height = 800 },
    @{ Name = "form-hardware-light-1280x800"; Renderer = "hardware"; Theme = "light"; Scene = "form"; Width = 1280; Height = 800 },
    @{ Name = "form-hardware-dark-1280x800"; Renderer = "hardware"; Theme = "dark"; Scene = "form"; Width = 1280; Height = 800 },
    @{ Name = "media-light-1280x800"; Renderer = "software"; Theme = "light"; Scene = "media"; Width = 1280; Height = 800 },
    @{ Name = "media-dark-1280x800"; Renderer = "software"; Theme = "dark"; Scene = "media"; Width = 1280; Height = 800 },
    @{ Name = "media-hardware-light-1280x800"; Renderer = "hardware"; Theme = "light"; Scene = "media"; Width = 1280; Height = 800 },
    @{ Name = "media-hardware-dark-1280x800"; Renderer = "hardware"; Theme = "dark"; Scene = "media"; Width = 1280; Height = 800 },
    @{ Name = "xaml-light-1280x800"; Renderer = "software"; Theme = "light"; Scene = "xaml"; Width = 1280; Height = 800 },
    @{ Name = "xaml-dark-1280x800"; Renderer = "software"; Theme = "dark"; Scene = "xaml"; Width = 1280; Height = 800 },
    @{ Name = "xaml-hardware-light-1280x800"; Renderer = "hardware"; Theme = "light"; Scene = "xaml"; Width = 1280; Height = 800 },
    @{ Name = "xaml-hardware-dark-1280x800"; Renderer = "hardware"; Theme = "dark"; Scene = "xaml"; Width = 1280; Height = 800 },
    @{ Name = "form-contrast-1280x800"; Renderer = "software"; Theme = "contrast"; Scene = "form"; Width = 1280; Height = 800 },
    @{ Name = "form-contrast-hardware-1280x800"; Renderer = "hardware"; Theme = "contrast"; Scene = "form"; Width = 1280; Height = 800 },
    @{ Name = "panels-light-1280x800"; Renderer = "software"; Theme = "light"; Scene = "panels"; Width = 1280; Height = 800 },
    @{ Name = "panels-dark-1280x800"; Renderer = "software"; Theme = "dark"; Scene = "panels"; Width = 1280; Height = 800 },
    @{ Name = "panels-hardware-light-1280x800"; Renderer = "hardware"; Theme = "light"; Scene = "panels"; Width = 1280; Height = 800 },
    @{ Name = "panels-hardware-dark-1280x800"; Renderer = "hardware"; Theme = "dark"; Scene = "panels"; Width = 1280; Height = 800 },
    @{ Name = "panels-contrast-1280x800"; Renderer = "software"; Theme = "contrast"; Scene = "panels"; Width = 1280; Height = 800 },
    @{ Name = "panels-contrast-hardware-1280x800"; Renderer = "hardware"; Theme = "contrast"; Scene = "panels"; Width = 1280; Height = 800 }
)

$failed = $false
foreach ($case in $cases) {
    $currentPath = Join-Path $currentDir "$($case.Name).png"
    & $goldenExe --out $currentPath --renderer $case.Renderer --theme $case.Theme `
        --scene $case.Scene --xaml $xamlPath --width $case.Width --height $case.Height
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

$pathPairs = @(
    @{ Software = "basic-light-1280x800"; Hardware = "hardware-light-1280x800" },
    @{ Software = "basic-dark-1280x800"; Hardware = "hardware-dark-1280x800" },
    @{ Software = "media-light-1280x800"; Hardware = "media-hardware-light-1280x800" },
    @{ Software = "media-dark-1280x800"; Hardware = "media-hardware-dark-1280x800" },
    @{ Software = "form-light-1280x800"; Hardware = "form-hardware-light-1280x800" },
    @{ Software = "form-dark-1280x800"; Hardware = "form-hardware-dark-1280x800" },
    @{ Software = "xaml-light-1280x800"; Hardware = "xaml-hardware-light-1280x800" },
    @{ Software = "xaml-dark-1280x800"; Hardware = "xaml-hardware-dark-1280x800" },
    @{ Software = "form-contrast-1280x800"; Hardware = "form-contrast-hardware-1280x800" },
    @{ Software = "controls-contrast-1280x800"; Hardware = "controls-contrast-hardware-1280x800" },
    @{ Software = "panels-light-1280x800"; Hardware = "panels-hardware-light-1280x800" },
    @{ Software = "panels-dark-1280x800"; Hardware = "panels-hardware-dark-1280x800" },
    @{ Software = "panels-contrast-1280x800"; Hardware = "panels-contrast-hardware-1280x800" }
)
foreach ($pair in $pathPairs) {
    $softwarePath = Join-Path $currentDir "$($pair.Software).png"
    $hardwarePath = Join-Path $currentDir "$($pair.Hardware).png"
    if (-not (Test-Path -LiteralPath $softwarePath) -or -not (Test-Path -LiteralPath $hardwarePath)) {
        continue
    }
    $pathDiff = Join-Path $diffDir "path-$($pair.Software).png"
    & $diffExe $softwarePath $hardwarePath --diff $pathDiff
    if ($LASTEXITCODE -ne 0) {
        Write-Output "FAILED: software and hardware paths differ ($($pair.Software))"
        $failed = $true
    } else {
        Write-Output "PASS: software and hardware paths agree ($($pair.Software))"
    }
}

if ($failed) {
    exit 1
}
