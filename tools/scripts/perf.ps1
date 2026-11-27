[CmdletBinding()]
param(
    [ValidateSet("x64-debug", "x64-release")]
    [string]$Preset = "x64-release",
    [int]$Runs = 5,
    [int]$Switches = 200,
    [switch]$SkipBuild,
    [switch]$Software,
    [switch]$Compare,
    [string]$Baseline = "tests\perf\baseline.json",
    [string]$Out
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
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

$themeAverage = 0.0
$themeP99 = 0.0
$themeMax = 0.0
$themeWsPeak = 0.0
if ($themeLine -match "total=([0-9.]+)/([0-9.]+)/([0-9.]+)") {
    $themeAverage = [double]$Matches[1]
    $themeP99 = [double]$Matches[2]
    $themeMax = [double]$Matches[3]
}
if ($themeLine -match "ws_peak=([0-9.]+) MB") {
    $themeWsPeak = [double]$Matches[1]
}

$startupWsPeak = 0.0
foreach ($line in $startupLines) {
    if ($line -match "ws_peak=([0-9.]+) MB") {
        $startupWsPeak = [math]::Max($startupWsPeak, [double]$Matches[1])
    }
}

$renderName = if ($Software) { "software" } else { "hardware" }
$result = [pscustomobject]@{
    date = (Get-Date -Format "yyyy-MM-dd")
    preset = $Preset
    render = $renderName
    startup = [pscustomobject]@{
        runs = $Runs
        min = $min
        median = $median
        max = $max
        average = $average
        ws_peak = $startupWsPeak
    }
    theme = [pscustomobject]@{
        switches = $Switches
        average = $themeAverage
        p99 = $themeP99
        max = $themeMax
        ws_peak = $themeWsPeak
    }
}

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

    $renderLabel = if ($Software) { "软件" } else { "硬件" }
    $lines = @(
        "# 性能基线",
        "",
        "- 日期：$(Get-Date -Format 'yyyy-MM-dd')",
        "- 机器：$osName / $cpu / $totalRam GB",
        "- 预设：$Preset，渲染：$renderLabel，异步提交：开",
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

    # The markdown keeps the raw lines for humans; the json is what -Compare
    # reads, so the thresholds never parse prose.
    $jsonPath = [System.IO.Path]::ChangeExtension($outPath, ".json")
    Set-Content -LiteralPath $jsonPath -Value ($result | ConvertTo-Json -Depth 5) -Encoding utf8
    Write-Output "recorded: $jsonPath"
}

if ($Compare) {
    $baselinePath = $Baseline
    if (-not [System.IO.Path]::IsPathRooted($baselinePath)) {
        $baselinePath = Join-Path $repoRoot $baselinePath
    }
    if (-not (Test-Path -LiteralPath $baselinePath)) {
        throw "baseline not found: $baselinePath (run with -Out tests\perf\baseline.md first)."
    }
    $base = Get-Content -LiteralPath $baselinePath -Raw | ConvertFrom-Json

    $script:regressions = 0
    function Test-Budget([string]$label, [double]$value, [double]$limit, [string]$unit) {
        if ($value -le $limit) {
            Write-Output ("PASS: {0} {1:N1} {2} <= {3:N1} {2}" -f $label, $value, $unit, $limit)
        }
        else {
            Write-Output ("FAIL: {0} {1:N1} {2} > {3:N1} {2}" -f $label, $value, $unit, $limit)
            $script:regressions += 1
        }
    }

    # A baseline from another preset or renderer cannot be compared: the
    # numbers are not on the same path.
    if ($base.preset -ne $Preset -or $base.render -ne $renderName) {
        Write-Output ("FAIL: baseline is {0} {1}, current run is {2} {3}" -f `
            $base.preset, $base.render, $Preset, $renderName)
        $script:regressions += 1
    }

    Test-Budget "startup median" $median ([math]::Min(800.0, [double]$base.startup.median * 1.25)) "ms"
    Test-Budget "startup max" $max ([math]::Min(800.0, [double]$base.startup.max * 1.5)) "ms"
    Test-Budget "startup memory peak" $startupWsPeak ([math]::Min(80.0, [double]$base.startup.ws_peak * 1.2)) "MB"
    Test-Budget "theme average" $themeAverage ([math]::Min(50.0, [double]$base.theme.average * 1.5)) "ms"
    Test-Budget "theme p99" $themeP99 ([math]::Min(50.0, [double]$base.theme.p99 * 1.5)) "ms"
    Test-Budget "theme memory peak" $themeWsPeak ([math]::Min(80.0, [double]$base.theme.ws_peak * 1.2)) "MB"

    if ($script:regressions -ne 0) {
        Write-Output ("FAILED: {0} performance checks regressed" -f $script:regressions)
        exit 1
    }
    Write-Output "PASS: performance stayed within budget"
}

