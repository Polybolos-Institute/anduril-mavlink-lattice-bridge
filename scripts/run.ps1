#requires -Version 5.1
param(
    [switch]$SimOnce,
    [switch]$AuthOnly,
    [int]$MavlinkUdp = 14550
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

# Optional: reuse Polybolos HOTL sandbox secrets without copying.
$HotlLocal = "P:\POLYBOLOS_PLATFORM\tools\lattice.local.ps1"
if (Test-Path -LiteralPath $HotlLocal) {
    . $HotlLocal
    Write-Host "Loaded HOTL tools\lattice.local.ps1 into process env"
}

if (-not (Test-Path ".venv\Scripts\python.exe")) {
    python -m venv .venv
    .\.venv\Scripts\python.exe -m pip install -r requirements.txt
}

$py = ".\.venv\Scripts\python.exe"
if ($AuthOnly) {
    & $py -m bridge --auth-only
    exit $LASTEXITCODE
}
if ($SimOnce) {
    & $py -m bridge --sim --once
    exit $LASTEXITCODE
}
& $py -m bridge --mavlink-udp $MavlinkUdp
exit $LASTEXITCODE
