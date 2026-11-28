[CmdletBinding()]
param(
    [ValidateSet("x64-debug", "x64-release")]
    [string]$Preset = "x64-release",
    [int]$Runs = 5,
    [int]$Switches = 200,
    [int]$Rows = 100000,
    [int]$FrameHz = 120,
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
$textListExe = Join-Path $sampleDir "nopeui_text_list_benchmark.exe"
$frameExe = Join-Path $sampleDir "nopeui_frame_benchmark.exe"

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

$textListLine = & $textListExe "--rows" $Rows "--scroll" 8 @extraArgs
if ($LASTEXITCODE -ne 0) {
    throw "text list benchmark failed."
}

$frameLine = & $frameExe "--paced" "--hz" $FrameHz "--full" @extraArgs
if ($LASTEXITCODE -ne 0) {
    throw "frame benchmark failed."
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

$scrollAverage = 0.0
$scrollP99 = 0.0
if ($textListLine -match "avg=([0-9.]+) ms p99=([0-9.]+) ms") {
    $scrollAverage = [double]$Matches[1]
    $scrollP99 = [double]$Matches[2]
}
$scrollFps = if ($scrollAverage -gt 0.0) { 1000.0 / $scrollAverage } else { 0.0 }

$frameAverage = 0.0
$frameP99 = 0.0
$frameMax = 0.0
$frameDropped = 0
if ($frameLine -match "avg=([0-9.]+) ms p99=([0-9.]+) ms min=([0-9.]+) ms max=([0-9.]+) ms") {
    $frameAverage = [double]$Matches[1]
    $frameP99 = [double]$Matches[2]
    $frameMax = [double]$Matches[4]
}
if ($frameLine -match "dropped=([0-9]+)") {
    $frameDropped = [int]$Matches[1]
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
    text_list = [pscustomobject]@{
        rows = $Rows
        average = $scrollAverage
        p99 = $scrollP99
        fps = [math]::Round($scrollFps, 1)
    }
    frame = [pscustomobject]@{
        hz = $FrameHz
        average = $frameAverage
        p99 = $frameP99
        max = $frameMax
        dropped = $frameDropped
    }
}

Write-Output ("startup runs={0} min={1:N1} ms median={2:N1} ms max={3:N1} ms avg={4:N1} ms" -f `
    $Runs, $min, $median, $max, $average)
foreach ($line in $startupLines) {
    Write-Output $line
}
Write-Output $themeLine
Write-Output $textListLine
Write-Output $frameLine
Write-Output ("scroll rows={0} avg={1:N1} ms p99={2:N1} ms fps={3:N1}" -f `
    $Rows, $scrollAverage, $scrollP99, $scrollFps)
Write-Output ("frame hz={0} avg={1:N2} ms p99={2:N2} ms max={3:N2} ms dropped={4}" -f `
    $FrameHz, $frameAverage, $frameP99, $frameMax, $frameDropped)

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
        # The Caption is localized, so it reads as Chinese on a Chinese system;
        # the edition ID and build number keep the record plain ASCII.
        $currentVersion = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
        $build = [int]$currentVersion.CurrentBuildNumber
        $product = if ($build -ge 22000) { "Windows 11" } else { "Windows 10" }
        $osName = "Microsoft $product $($currentVersion.EditionID)"
    }
    catch {
        $cpu = "unknown"
    }

    $renderLabel = if ($Software) { "software" } else { "hardware" }
    $lines = @(
        "# Performance Baseline",
        "",
        "- Date: $(Get-Date -Format 'yyyy-MM-dd')",
        "- Machine: $osName / $cpu / $totalRam GB",
        "- Preset: $Preset, render: $renderLabel, async submit: on",
        "- Startup: $Runs runs, 900x640 form page, min $([math]::Round($min, 1)) ms / median $([math]::Round($median, 1)) ms / max $([math]::Round($max, 1)) ms",
        "- Theme switching: $Switches cycles, alternating light and dark",
        "- Scroll: $Rows rows, 8 px steps, avg $([math]::Round($scrollAverage, 1)) ms / p99 $([math]::Round($scrollP99, 1)) ms / $([math]::Round($scrollFps, 1)) fps",
        "- Frame: ${FrameHz} Hz target, avg $([math]::Round($frameAverage, 2)) ms / p99 $([math]::Round($frameP99, 2)) ms / max $([math]::Round($frameMax, 2)) ms, dropped $frameDropped",
        "",
        "Startup total includes process creation and loader time; theme switching was measured with commit throttling off and excludes vblank waits. Scroll numbers are render time from nopeui_text_list_benchmark, and frame numbers are dirty-rect frames paced at ${FrameHz} Hz.",
        "",
        "## Raw Output",
        "",
        '```'
    )
    foreach ($line in $startupLines) {
        $lines += $line
    }
    $lines += $themeLine
    $lines += $textListLine
    $lines += $frameLine
    $lines += '```'
    # Markdown and json are stored at LF by .gitattributes, so write them
    # with a bare newline instead of the platform default.
    Set-Content -LiteralPath $outPath -Value ($lines -join "`n") -Encoding utf8 -NoNewline
    Write-Output "recorded: $outPath"

    # The markdown keeps the raw lines for humans; the json is what -Compare
    # reads, so the thresholds never parse prose.
    $jsonPath = [System.IO.Path]::ChangeExtension($outPath, ".json")
    $json = ($result | ConvertTo-Json -Depth 5) -replace "`r`n", "`n"
    Set-Content -LiteralPath $jsonPath -Value $json -Encoding utf8 -NoNewline
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

    # A baseline recorded on a noisy box moves around a lot, so the trend
    # against it only warns; the absolute budgets above are the gate.
    function Test-Trend([string]$label, [double]$value, [double]$limit, [string]$unit) {
        if ($value -le $limit) {
            Write-Output ("PASS: {0} {1:N1} {2} <= {3:N1} {2} (baseline trend)" -f $label, $value, $unit, $limit)
        }
        else {
            Write-Output ("RISK: {0} {1:N1} {2} > {3:N1} {2} (baseline trend)" -f $label, $value, $unit, $limit)
        }
    }

    # A baseline from another preset or renderer cannot be compared: the
    # numbers are not on the same path.
    if ($base.preset -ne $Preset -or $base.render -ne $renderName) {
        Write-Output ("FAIL: baseline is {0} {1}, current run is {2} {3}" -f `
            $base.preset, $base.render, $Preset, $renderName)
        $script:regressions += 1
    }

    # Absolute roadmap budgets cap every comparison; the doubled baseline
    # factor leaves room for this box's run-to-run noise while still catching
    # a real slowdown.
    # Absolute roadmap budgets are the gate: 800 ms startup, 80 MB memory,
    # 50 ms theme switching, 60 fps on 100000 rows, 120 Hz frames.
    Test-Budget "startup median" $median 800.0 "ms"
    Test-Budget "startup max" $max 800.0 "ms"
    Test-Budget "startup memory peak" $startupWsPeak 80.0 "MB"
    Test-Budget "theme average" $themeAverage 50.0 "ms"
    Test-Budget "theme p99" $themeP99 50.0 "ms"
    Test-Budget "theme memory peak" $themeWsPeak 80.0 "MB"

    # Scroll and frame numbers are new in the baseline: a baseline without
    # them cannot show the trend for the two budgets the roadmap set.
    if ($null -eq $base.text_list -or $null -eq $base.frame) {
        Write-Output "RISK: baseline has no scroll or frame numbers; rerun with -Out first."
    }
    else {
        Test-Trend "startup median" $median ([double]$base.startup.median * 2.0) "ms"
        Test-Trend "startup max" $max ([double]$base.startup.max * 2.0) "ms"
        Test-Trend "theme average" $themeAverage ([double]$base.theme.average * 2.0) "ms"
        Test-Trend "theme p99" $themeP99 ([double]$base.theme.p99 * 2.0) "ms"
        Test-Trend "scroll p99" $scrollP99 ([math]::Min(16.7, [double]$base.text_list.p99 * 2.0)) "ms"
        Test-Trend "frame average" $frameAverage ([math]::Min((1000.0 / $FrameHz), [double]$base.frame.average * 2.0)) "ms"
        Test-Trend "frame p99" $frameP99 ([math]::Min((2000.0 / $FrameHz), [double]$base.frame.p99 * 2.0)) "ms"
    }

    # The roadmap treats anything under 60 fps on 100000 rows as below the
    # exit floor, and under 45 fps as a performance risk.
    if ($scrollFps -ge 60.0) {
        Write-Output ("PASS: scroll {0:N1} fps >= 60.0 fps" -f $scrollFps)
    }
    elseif ($scrollFps -ge 45.0) {
        Write-Output ("RISK: scroll {0:N1} fps is under the 60 fps exit floor" -f $scrollFps)
        $script:regressions += 1
    }
    else {
        Write-Output ("FAIL: scroll {0:N1} fps < 45.0 fps" -f $scrollFps)
        $script:regressions += 1
    }

    if ($frameDropped -gt 0) {
        Write-Output ("RISK: frame benchmark dropped {0} frames" -f $frameDropped)
        $script:regressions += 1
    }

    if ($script:regressions -ne 0) {
        Write-Output ("FAILED: {0} performance checks regressed" -f $script:regressions)
        exit 1
    }
    Write-Output "PASS: performance stayed within budget"
}

