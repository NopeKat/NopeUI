[CmdletBinding()]
param(
    [ValidateSet("x64-debug", "x64-release")]
    [string]$Preset = "x64-debug"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$ctest = Resolve-NopeUICTest

Push-Location $repoRoot
try {
    & $ctest --preset $Preset --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "CTest failed for preset '$Preset'."
    }
}
finally {
    Pop-Location
}
