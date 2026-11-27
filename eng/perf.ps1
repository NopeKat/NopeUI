[CmdletBinding()]
param(
    [ValidateSet("x64-debug", "x64-release")]
    [string]$Preset = "x64-release",
    [int]$Runs = 5,
    [int]$Switches = 200,
    [switch]$SkipBuild,
    [switch]$Software,
    [string]$Out
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$configuration = if ($Preset -like "*debug") { "Debug" } else { "Release" }
$sampleDir = Join-Path $repoRoot ".build\$Preset\samples\$configuration"
$startupExe = Join-Path $sampleDir "nopeui_startup_benchmark.exe"
$themeExe = Join-Path $sampleDir "nopeui_theme_benchmark.exe"

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset
}

$extraArgs = @()
if ($Software) {
    $extraArgs = @("--software")
}

$startupLines = @()
$startupTotals = @()
for ($run = 1; $run -le $Runs; ++$run) {
    $line = & $startupExe @extraArgs
    if ($LASTEXITCODE -ne 0) {
        throw "startup benchmark failed on run $run."
    }
    $startupLines += $line
    if ($line -match "total=([0-9.]+) ms") {
        $startupTotals += [double]$Matches[1]
    }
}

$themeLine = & $themeExe "--switches" $Switches @extraArgs
if ($LASTEXITCODE -ne 0) {
    throw "theme benchmark failed."
}

$sorted = @($startupTotals | Sort-Object)
if ($sorted.Count -eq 0) {
    throw "startup benchmark produced no total timings."
}
$min = $sorted[0]
$max = $sorted[$sorted.Count - 1]
$median = if ($sorted.Count % 2 -eq 1) {
    $sorted[[int](($sorted.Count - 1) / 2)]
} else {
    ($sorted[$sorted.Count / 2 - 1] + $sorted[$sorted.Count / 2]) / 2
}
$average = ($startupTotals | Measure-Object -Average).Average

Write-Output ("startup runs={0} min={1:N1} ms median={2:N1} ms max={3:N1} ms avg={4:N1} ms" -f `
    $Runs, $min, $median, $max, $average)
foreach ($line in $startupLines) {
    Write-Output $line
}
Write-Output $themeLine

if (-not [string]::IsNullOrWhiteSpace($Out)) {
    $outPath = $Out
    if (-not [System.IO.Path]::IsPathRooted($outPath)) {
        $outPath = Join-Path $repoRoot $outPath
    }
    $outDir = Split-Path -Parent $outPath
    if (-not (Test-Path -LiteralPath $outDir)) {
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    }

    $cpu = "unknown"
    $totalRam = 0.0
    $osName = "unknown"
    try {
        $cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1).Name.Trim()
        $totalRam = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1)
        $osName = (Get-CimInstance Win32_OperatingSystem).Caption.Trim()
    }
    catch {
        $cpu = "unknown"
    }

    $renderName = if ($Software) { "软件" } else { "硬件" }
    $lines = @(
        "# 性能基线",
        "",
        "- 日期：$(Get-Date -Format 'yyyy-MM-dd')",
        "- 机器：$osName / $cpu / $totalRam GB",
        "- 预设：$Preset，渲染：$renderName，异步提交：开",
        "- 启动：$Runs 次，900×640 表单页，min $([math]::Round($min, 1)) ms / median $([math]::Round($median, 1)) ms / max $([math]::Round($max, 1)) ms",
        "- 主题切换：$Switches 次，浅色与深色互切",
        "",
        "启动 total 含进程创建与加载器；主题切换在关闭提交节流后测量，不含 vblank 等待。",
        "",
        "## 原始输出",
        "",
        '```'
    )
    foreach ($line in $startupLines) {
        $lines += $line
    }
    $lines += $themeLine
    $lines += '```'
    Set-Content -LiteralPath $outPath -Value $lines -Encoding utf8
    Write-Output "recorded: $outPath"
}
