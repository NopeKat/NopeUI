[CmdletBinding()]
param(
    [ValidateSet("x64-debug", "x64-release")]
    [string]$Preset = "x64-debug"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$cmake = Resolve-NopeUICMake

Push-Location $repoRoot
try {
    & $cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed for preset '$Preset'."
    }

    & $cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed for preset '$Preset'."
    }
}
finally {
    Pop-Location
}
