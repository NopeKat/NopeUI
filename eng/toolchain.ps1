function Resolve-NopeUICMake {
    $candidates = @()

    if (-not [string]::IsNullOrWhiteSpace($env:NOPEUI_CMAKE)) {
        $candidates += $env:NOPEUI_CMAKE
    }

    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($null -ne $command -and -not [string]::IsNullOrWhiteSpace($command.Source)) {
        $candidates += $command.Source
    }

    $packageRoot = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
    if (Test-Path -LiteralPath $packageRoot) {
        $packages = Get-ChildItem -LiteralPath $packageRoot -Directory -Filter 'Kitware.CMake*' -ErrorAction SilentlyContinue
        foreach ($package in $packages) {
            $candidate = Get-ChildItem -LiteralPath $package.FullName -Filter 'cmake.exe' -Recurse -ErrorAction SilentlyContinue |
                Select-Object -First 1 -ExpandProperty FullName
            if (-not [string]::IsNullOrWhiteSpace($candidate)) {
                $candidates += $candidate
            }
        }
    }

    $foundVersions = @()
    foreach ($candidate in $candidates) {
        if (-not (Test-Path -LiteralPath $candidate)) {
            continue
        }

        $versionLine = & $candidate --version | Select-Object -First 1
        if ($versionLine -match 'cmake version 4\.4\.4') {
            return $candidate
        }
        if (-not [string]::IsNullOrWhiteSpace($versionLine)) {
            $foundVersions += $versionLine
        }
    }

    $found = if ($foundVersions.Count -eq 0) { 'none' } else { $foundVersions -join '; ' }
    throw "CMake 4.4.4 is required. Found: $found. Set NOPEUI_CMAKE to the correct cmake.exe."
}

function Resolve-NopeUICTest {
    $cmake = Resolve-NopeUICMake
    $ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
    if (-not (Test-Path -LiteralPath $ctest)) {
        throw "ctest.exe was not found next to CMake at '$cmake'."
    }
    return $ctest
}
