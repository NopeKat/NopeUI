[CmdletBinding()]
param(
    [ValidateSet("x64-debug", "x64-release")]
    [string]$Preset = "x64-debug",
    [switch]$SkipBuild,
    [int]$TimeoutSeconds = 20
)

# Walks the accessibility tree of the window smoke app and checks the menu rows
# show up as menu items. This is a manual script: it is not part of CI and it
# does not run with the golden pass. Point -TimeoutSeconds higher on slow VMs.
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$configuration = if ($Preset -like "*debug") { "Debug" } else { "Release" }
$smokeExe = Join-Path $repoRoot ".build\$Preset\samples\$configuration\nopeui_window_smoke.exe"

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset
}

if (-not (Test-Path -LiteralPath $smokeExe)) {
    throw "window smoke app not found: $smokeExe"
}

try {
    $uia = New-Object -ComObject UIAutomationClient.CUIAutomation
}
catch {
    throw "UIAutomation COM object is unavailable; run this script on Windows with the desktop automation components installed."
}

$process = Start-Process -FilePath $smokeExe -PassThru
try {
    $root = $uia.GetRootElement()
    $window = $null
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ($window -eq $null -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        $condition = $uia.CreatePropertyCondition(0x30007, "NopeUI Window Smoke")
        $window = $root.FindFirst(0x4, $condition)
    }
    if ($window -eq $null) {
        throw "window 'NopeUI Window Smoke' did not appear within $TimeoutSeconds seconds"
    }

    $names = New-Object System.Collections.Generic.List[string]
    $types = New-Object System.Collections.Generic.List[int]
    $walker = $uia.RawViewWalker
    $pending = New-Object System.Collections.Queue
    $pending.Enqueue($walker.GetFirstChildElement($window))
    $walked = 0
    while ($pending.Count -gt 0 -and $walked -lt 200) {
        $element = $pending.Dequeue()
        if ($element -eq $null) {
            continue
        }
        ++$walked
        $name = $element.CurrentName
        $type = $element.CurrentControlType
        if ($name) {
            $names.Add([string]$name)
        }
        $types.Add([int]$type)
        $child = $walker.GetFirstChildElement($element)
        while ($child -ne $null) {
            $pending.Enqueue($child)
            $child = $walker.GetNextSiblingElement($child)
        }
    }

    Write-Output ("walked {0} elements, {1} named" -f $walked, $names.Count)
    for ($index = 0; $index -lt $names.Count; ++$index) {
        Write-Output ("  {0}: {1}" -f $types[$index], $names[$index])
    }

    $failed = $false
    if (-not ($names -contains "Menu bar")) {
        Write-Output "FAILED: menu bar element missing from the tree"
        $failed = $true
    }
    foreach ($item in @("File", "Edit", "View")) {
        if (-not ($names -contains $item)) {
            Write-Output "FAILED: menu item '$item' missing from the tree"
            $failed = $true
        }
    }
    if ($failed) {
        exit 1
    }
    Write-Output "PASS: menu bar and menu items are exposed through UIA"
}
finally {
    if ($process -ne $null -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
    }
}
