#!/usr/bin/env pwsh
# Build then copy firmware.uf2 to Pico BOOTSEL (RPI-RP2). No picotool USB driver needed.
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('can_test_master', 'can_test_slave', 'can_test_occupier')]
    [string]$Sketch
)

$ErrorActionPreference = 'Stop'
$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$proj = Join-Path $PSScriptRoot $Sketch
$uf2 = Join-Path $repo "build\$Sketch\$Sketch\firmware.uf2"

Push-Location $proj
try {
    & pio run 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}

if (-not (Test-Path $uf2)) {
    Write-Host "UF2 not found: $uf2" -ForegroundColor Red
    exit 1
}

$volumes = @(Get-Volume -ErrorAction SilentlyContinue | Where-Object { $_.FileSystemLabel -eq 'RPI-RP2' })
if ($volumes.Count -eq 0) {
    Write-Host ""
    Write-Host "Hold BOOTSEL, plug in ONE Pico, then re-run this script." -ForegroundColor Yellow
    Write-Host "Or copy manually:" -ForegroundColor Yellow
    Write-Host "  $uf2" -ForegroundColor Cyan
    Write-Host "  -> RPI-RP2 drive" -ForegroundColor Cyan
    exit 2
}
if ($volumes.Count -gt 1) {
    Write-Host "Multiple RPI-RP2 drives — leave only one Pico in BOOTSEL." -ForegroundColor Red
    exit 3
}

$dest = "$($volumes[0].DriveLetter):\"
Copy-Item -Path $uf2 -Destination $dest -Force
Write-Host "Flashed $Sketch to $dest" -ForegroundColor Green
