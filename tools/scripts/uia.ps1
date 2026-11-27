[CmdletBinding()]
param(
    [ValidateSet("x64-debug", "x64-release")]
    [string]$Preset = "x64-debug",
    [switch]$SkipBuild,
    [int]$TimeoutSeconds = 20
)

# Full UIA walkthrough of the window smoke app: walks the tree, checks every
# control type the form page puts on screen, then reads and writes each UIA
# pattern the providers advertise. Manual script: it is not part of CI and it
# does not run with the golden pass. Point -TimeoutSeconds higher on slow VMs.
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
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

$script:failures = 0

function Check([bool]$passed, [string]$message) {
    if ($passed) {
        Write-Output "PASS: $message"
    }
    else {
        Write-Output "FAIL: $message"
        $script:failures += 1
    }
}

function Get-Descendants([System.Windows.Automation.AutomationElement]$window) {
    $walker = [System.Windows.Automation.TreeWalker]::RawViewWalker
    $queue = [System.Collections.Generic.Queue[object]]::new()
    $items = [System.Collections.Generic.List[object]]::new()
    $child = $walker.GetFirstChild($window)
    while ($null -ne $child) {
        $queue.Enqueue($child)
        $child = $walker.GetNextSibling($child)
    }
    while ($queue.Count -gt 0) {
        $element = $queue.Dequeue()
        if ($null -eq $element) {
            continue
        }
        $items.Add($element)
        $child = $walker.GetFirstChild($element)
        while ($null -ne $child) {
            $queue.Enqueue($child)
            $child = $walker.GetNextSibling($child)
        }
    }
    return $items
}

function Get-Records($elements) {
    $records = [System.Collections.Generic.List[object]]::new()
    foreach ($element in $elements) {
        try {
            $records.Add([pscustomobject]@{
                Element = $element
                Name = [string]$element.Current.Name
                TypeId = [int]$element.Current.ControlType.Id
                Framework = [string]$element.Current.FrameworkId
            })
        }
        catch {
            # Elements can disappear while the walk is in flight.
        }
    }
    return $records
}

function Find-Record($records, [string]$name, $typeId = -1) {
    foreach ($record in $records) {
        if ($record.Name -eq $name -and ($typeId -lt 0 -or $record.TypeId -eq $typeId)) {
            return $record
        }
    }
    return $null
}

function Find-FirstOfType($records, $typeId) {
    foreach ($record in $records) {
        if ($record.TypeId -eq $typeId) {
            return $record
        }
    }
    return $null
}

function Get-Pattern($element, $patternId) {
    $pattern = $null
    if ($null -eq $element) {
        return $null
    }
    if ($element.TryGetCurrentPattern($patternId, [ref]$pattern)) {
        return $pattern
    }
    return $null
}

$process = Start-Process -FilePath $smokeExe -PassThru
try {
    $window = $null
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ($null -eq $window -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        $condition = New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::ProcessIdProperty,
            $process.Id)
        $window = $root.FindFirst(
            [System.Windows.Automation.TreeScope]::Children,
            $condition)
    }
    if ($null -eq $window) {
        throw "window 'NopeUI Window Smoke' did not appear within $TimeoutSeconds seconds"
    }

    # UIA can hand out the plain HWND provider on the first query while the
    # custom root is still attaching; re-open by handle until a NopeUI child
    # appears next to the native window chrome.
    $hwnd = [IntPtr]$window.Current.NativeWindowHandle
    $deadline = (Get-Date).AddSeconds(10)
    $attached = $false
    while (-not $attached -and (Get-Date) -lt $deadline) {
        $children = $window.FindAll(
            [System.Windows.Automation.TreeScope]::Children,
            [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($candidate in $children) {
            if ($candidate.Current.FrameworkId -eq "NopeUI") {
                $attached = $true
                break
            }
        }
        if (-not $attached) {
            Start-Sleep -Milliseconds 200
            $window = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
        }
    }
    if (-not $attached) {
        throw "NopeUI automation provider did not attach to the window"
    }

    # The smoke app rewrites its caption with the last frame time, so only the
    # title prefix is stable.
    Check ($window.Current.Name -like "NopeUI Window Smoke*") "window title carries the app name"
    Check ($window.Current.FrameworkId -eq "NopeUI") "window is served by the NopeUI provider"

    $records = Get-Records (Get-Descendants $window)
    $named = @($records | Where-Object { $_.Name }).Count
    Write-Output ("walked {0} elements, {1} named" -f $records.Count, $named)

    # Presence checks: every control the form page shows has to be reachable.
    $types = @{
        Text        = [System.Windows.Automation.ControlType]::Text.Id
        Edit        = [System.Windows.Automation.ControlType]::Edit.Id
        Button      = [System.Windows.Automation.ControlType]::Button.Id
        CheckBox    = [System.Windows.Automation.ControlType]::CheckBox.Id
        RadioButton = [System.Windows.Automation.ControlType]::RadioButton.Id
        Slider      = [System.Windows.Automation.ControlType]::Slider.Id
        ProgressBar = [System.Windows.Automation.ControlType]::ProgressBar.Id
        MenuItem    = [System.Windows.Automation.ControlType]::MenuItem.Id
    }

    foreach ($entry in $types.GetEnumerator()) {
        $present = @($records | Where-Object { $_.TypeId -eq $entry.Value }).Count -gt 0
        Check $present ("{0} elements are reachable" -f $entry.Key)
    }

    foreach ($label in @("NopeUI form", "Name", "Email", "Role", "Plan", "Volume", "Quota")) {
        Check ($null -ne (Find-Record $records $label $types.Text)) "label '$label' is exposed as text"
    }
    foreach ($name in @("Menu bar", "File", "Edit", "View")) {
        Check ($null -ne (Find-Record $records $name)) "'$name' is reachable in the menu tree"
    }

    $edits = @($records | Where-Object { $_.TypeId -eq $types.Edit })
    Check ($edits.Count -ge 2) "two text fields are exposed as edits"

    # Value pattern: write through the provider and read the stored text back.
    $edit = Find-FirstOfType $records $types.Edit
    $value_pattern = Get-Pattern $edit.Element ([System.Windows.Automation.ValuePattern]::Pattern)
    Check ($null -ne $value_pattern) "text field advertises the value pattern"
    if ($null -ne $value_pattern) {
        $value_pattern.SetValue("uia probe")
        Check ($value_pattern.Current.Value -eq "uia probe") "text field accepts a value through UIA"
        $value_pattern.SetValue("Ada Lovelace")
        Check ($value_pattern.Current.Value -eq "Ada Lovelace") "text field restores its original value"
    }

    # Toggle pattern: the checkbox flips state through the provider.
    $check_box = Find-Record $records "Yearly billing" $types.CheckBox
    $toggle = Get-Pattern $check_box.Element ([System.Windows.Automation.TogglePattern]::Pattern)
    Check ($null -ne $toggle) "checkbox advertises the toggle pattern"
    if ($null -ne $toggle) {
        $state_before = $toggle.Current.ToggleState
        $toggle.Toggle()
        Check ($toggle.Current.ToggleState -ne $state_before) "checkbox toggles through UIA"
        $toggle.Toggle()
        Check ($toggle.Current.ToggleState -eq $state_before) "checkbox returns to its original state"
    }

    # The toggle switch is a button with a toggle pattern.
    $toggle_switch = Find-Record $records "Email updates"
    $switch_pattern = Get-Pattern $toggle_switch.Element ([System.Windows.Automation.TogglePattern]::Pattern)
    Check ($null -ne $switch_pattern) "toggle switch advertises the toggle pattern"
    if ($null -ne $switch_pattern) {
        $switch_before = $switch_pattern.Current.ToggleState
        $switch_pattern.Toggle()
        Check ($switch_pattern.Current.ToggleState -ne $switch_before) "toggle switch toggles through UIA"
        $switch_pattern.Toggle()
    }

    # Radio buttons share one group: selecting one clears the others.
    $viewer = Find-Record $records "Viewer" $types.RadioButton
    $owner = Find-Record $records "Owner" $types.RadioButton
    $viewer_pattern = Get-Pattern $viewer.Element ([System.Windows.Automation.TogglePattern]::Pattern)
    $owner_pattern = Get-Pattern $owner.Element ([System.Windows.Automation.TogglePattern]::Pattern)
    Check ($null -ne $viewer_pattern -and $null -ne $owner_pattern) "radio buttons advertise the toggle pattern"
    if ($null -ne $viewer_pattern -and $null -ne $owner_pattern) {
        $viewer_pattern.Toggle()
        Check ($viewer_pattern.Current.ToggleState -eq [System.Windows.Automation.ToggleState]::On) "radio button selects itself through UIA"
        Check ($owner_pattern.Current.ToggleState -eq [System.Windows.Automation.ToggleState]::Off) "radio group clears the previous selection"
        $owner_pattern.Toggle()
        Check ($viewer_pattern.Current.ToggleState -eq [System.Windows.Automation.ToggleState]::Off) "radio group clears the new selection"
    }

    # Range value pattern: the slider accepts a new value, the progress bar
    # reports one without accepting writes.
    $slider = Find-FirstOfType $records $types.Slider
    $range = Get-Pattern $slider.Element ([System.Windows.Automation.RangeValuePattern]::Pattern)
    Check ($null -ne $range) "slider advertises the range value pattern"
    if ($null -ne $range) {
        $range.SetValue(80.0)
        Check ([math]::Abs($range.Current.Value - 80.0) -lt 0.5) "slider accepts a value through UIA"
    }

    $progress = Find-FirstOfType $records $types.ProgressBar
    $progress_range = Get-Pattern $progress.Element ([System.Windows.Automation.RangeValuePattern]::Pattern)
    Check ($null -ne $progress_range) "progress bar advertises the range value pattern"
    if ($null -ne $progress_range) {
        Check ($progress_range.Current.Value -ge 0.0) "progress bar reports its value"
        Check ($progress_range.Current.IsReadOnly) "progress bar value is read only"
    }

    # Invoke pattern: the save button must survive an invocation from UIA.
    $save = Find-Record $records "Save" $types.Button
    $invoke = Get-Pattern $save.Element ([System.Windows.Automation.InvokePattern]::Pattern)
    Check ($null -ne $invoke) "button advertises the invoke pattern"
    if ($null -ne $invoke) {
        $invoke.Invoke()
        Start-Sleep -Milliseconds 200
        Check (-not $process.HasExited) "app stays alive after an invoke"
    }

    # Menu items expand into their popup items, and invoking one closes it.
    $file_menu = Find-Record $records "File" $types.MenuItem
    $expand = Get-Pattern $file_menu.Element ([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
    Check ($null -ne $expand) "menu advertises the expand collapse pattern"
    if ($null -ne $expand) {
        $expand.Expand()
        Start-Sleep -Milliseconds 150
        $expanded_records = Get-Records (Get-Descendants $window)
        $new_item = Find-Record $expanded_records "New" $types.MenuItem
        Check ($null -ne $new_item) "menu shows its items after expanding"
        if ($null -ne $new_item) {
            $new_invoke = Get-Pattern $new_item.Element ([System.Windows.Automation.InvokePattern]::Pattern)
            Check ($null -ne $new_invoke) "menu item advertises the invoke pattern"
            if ($null -ne $new_invoke) {
                $new_invoke.Invoke()
                Start-Sleep -Milliseconds 150
                $closed_records = Get-Records (Get-Descendants $window)
                Check ($null -eq (Find-Record $closed_records "New" $types.MenuItem)) "menu closes after invoking an item"
            }
        }
        else {
            $expand.Collapse()
        }
        Check (-not $process.HasExited) "app stays alive after the menu walkthrough"
    }

    # Bounding rectangles: the layout the providers report has to sit inside
    # the window instead of an empty rect.
    $save_bounds = $save.Element.Current.BoundingRectangle
    Check ($save_bounds.Width -gt 0 -and $save_bounds.Height -gt 0) "button reports a non empty bounding rectangle"

    if ($script:failures -ne 0) {
        Write-Output ("FAILED: {0} checks failed" -f $script:failures)
        exit 1
    }
    Write-Output "PASS: full UIA walkthrough completed"
}
finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
    }
}
