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
    # Use the .NET UIAutomation client assemblies. The CUIAutomation COM
    # ProgID is not registered on every Windows install.
    Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
    $root = [System.Windows.Automation.AutomationElement]::RootElement
}
catch {
    throw "UIAutomation client is unavailable; run this script on Windows with the desktop automation components installed."
}

$process = Start-Process -FilePath $smokeExe -PassThru
try {
    $window = $null
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ($window -eq $null -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        $condition = New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::ProcessIdProperty,
            $process.Id)
        $window = $root.FindFirst(
            [System.Windows.Automation.TreeScope]::Children,
            $condition)
    }
    if ($window -eq $null) {
        throw "window 'NopeUI Window Smoke' did not appear within $TimeoutSeconds seconds"
    }

    # UIA can hand out the plain HWND provider on the first query while the
    # custom root is still attaching; re-open by handle until a NopeUI child
    # appears next to the native window chrome.
    $hwnd = [IntPtr]$window.Current.NativeWindowHandle
    $walker = [System.Windows.Automation.TreeWalker]::RawViewWalker
    $deadline = (Get-Date).AddSeconds(10)
    $has_custom = $false
    while (-not $has_custom -and (Get-Date) -lt $deadline) {
        $children = $window.FindAll(
            [System.Windows.Automation.TreeScope]::Children,
            [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($candidate in $children) {
            if ($candidate.Current.FrameworkId -eq "NopeUI") {
                $has_custom = $true
                break
            }
        }
        if (-not $has_custom) {
            Start-Sleep -Milliseconds 200
            $window = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
        }
    }
    if (-not $has_custom) {
        throw "NopeUI automation provider did not attach to the window"
    }

    $names = New-Object System.Collections.Generic.List[string]
    $types = New-Object System.Collections.Generic.List[int]
    $pending = New-Object System.Collections.Queue
    $child = $walker.GetFirstChild($window)
    while ($child -ne $null) {
        $pending.Enqueue($child)
        $child = $walker.GetNextSibling($child)
    }
    $walked = 0
    while ($pending.Count -gt 0 -and $walked -lt 200) {
        $element = $pending.Dequeue()
        if ($element -eq $null) {
            continue
        }
        ++$walked
        $name = $element.Current.Name
        $type = $element.Current.ControlType
        if ($name) {
            $names.Add([string]$name)
        }
        $types.Add([int]$type.Id)
        $child = $walker.GetFirstChild($element)
        while ($child -ne $null) {
            $pending.Enqueue($child)
            $child = $walker.GetNextSibling($child)
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
