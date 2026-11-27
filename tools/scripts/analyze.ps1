[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [string]$Out = ".build\analyze.log",
    # -1 only reports the findings; a non-negative value fails the script when
    # there are more of them than allowed.
    [int]$MaxWarnings = -1
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$cmake = Resolve-NopeUICMake

$buildDir = Join-Path $repoRoot '.build\x64-analyze'
$logPath = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $repoRoot $Out }

Push-Location $repoRoot
try {
    if (-not $SkipBuild) {
        & $cmake -S . -B $buildDir -G 'Visual Studio 18 2026' -A x64 `
            -DCMAKE_BUILD_TYPE=Release -DNOPEUI_ANALYZE=ON
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed for the analysis build."
        }

        # /WX is off in this build, so the pass runs to the end and every
        # finding lands in the log instead of stopping at the first one.
        & $cmake --build $buildDir --config Release --parallel 2>&1 |
            Tee-Object -FilePath $logPath | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "The analysis build failed to compile; see '$logPath'."
        }
        Write-Output "build log: $logPath"
    }
}
finally {
    Pop-Location
}

if (-not (Test-Path -LiteralPath $logPath)) {
    throw "analysis log not found: $logPath (drop -SkipBuild for the first run)."
}

$findings = @()
foreach ($line in Get-Content -LiteralPath $logPath) {
    if ($line -match 'warning (C[0-9]+): (.*)$') {
        $findings += [pscustomobject]@{
            Code    = $Matches[1]
            Message = $Matches[2]
            Line    = $line.Trim()
        }
    }
}

if ($findings.Count -eq 0) {
    Write-Output "PASS: code analysis reported nothing"
    exit 0
}

Write-Output ("code analysis: {0} findings" -f $findings.Count)
foreach ($group in ($findings | Group-Object Code | Sort-Object Count -Descending)) {
    Write-Output ("  {0}  {1}  {2}" -f $group.Name, $group.Count, $group.Group[0].Message)
}

if ($MaxWarnings -lt 0) {
    Write-Output "report only: pass -MaxWarnings to make this a gate"
    exit 0
}
if ($findings.Count -gt $MaxWarnings) {
    Write-Output ("FAIL: {0} findings, allowed {1}" -f $findings.Count, $MaxWarnings)
    exit 1
}
Write-Output ("PASS: {0} findings, allowed {1}" -f $findings.Count, $MaxWarnings)
