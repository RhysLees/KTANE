#!/usr/bin/env pwsh
# Build + flash can_test_master and can_test_slave (no manual UF2 drag).
# Requires: both Picos on USB (2E8A:000A COM ports), serial monitors CLOSED on those COMs.
param(
    [switch]$SkipBuild,
    [string]$MasterPort = '',
    [string]$SlavePort = '',
    [string]$MasterSerial = '',
    [string]$SlaveSerial = ''
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'flash_helpers.ps1')

$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$masterUf2 = Join-Path $repo "build\can_test_master\can_test_master\firmware.uf2"
$slaveUf2 = Join-Path $repo "build\can_test_slave\can_test_slave\firmware.uf2"
$mapFile = Join-Path $PSScriptRoot 'can_test_flash_map.ps1'
if (Test-Path $mapFile) { . $mapFile }

if (-not $SkipBuild) {
    Write-Host 'Building can_test_master ...' -ForegroundColor Yellow
    Push-Location (Join-Path $PSScriptRoot 'can_test_master')
    try { & pio run -e can_test_master 2>&1 | Out-Host; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } }
    finally { Pop-Location }
    Write-Host 'Building can_test_slave ...' -ForegroundColor Yellow
    Push-Location (Join-Path $PSScriptRoot 'can_test_slave')
    try { & pio run 2>&1 | Out-Host; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } }
    finally { Pop-Location }
}

$picos = @(Get-PicoUsbSerialDevices)
if ($picos.Count -lt 2) {
    Write-Host "Need two Pico USB serial devices (2E8A:000A); found $($picos.Count)." -ForegroundColor Red
    & pio device list
    exit 1
}

function Resolve-Port {
    param([string]$Port, [string]$Serial, [string]$DefaultFromMap)
    if ($Port) { return $Port }
    if ($Serial) {
        $m = $picos | Where-Object { $_.Serial -eq $Serial }
        if ($m) { return $m.Port }
    }
    if ($DefaultFromMap) {
        $m = $picos | Where-Object { $_.Serial -eq $DefaultFromMap }
        if ($m) { return $m.Port }
    }
    return $null
}

$masterMap = $null
$slaveMap = $null
if (Test-Path variable:FlashMap) {
    $masterMap = $FlashMap.MasterSerial
    $slaveMap = $FlashMap.SlaveSerial
}

if (-not $MasterPort) {
    $MasterPort = Resolve-Port '' $MasterSerial $masterMap
    if (-not $MasterPort) { $MasterPort = $picos[0].Port }
}
if (-not $SlavePort) {
    $SlavePort = Resolve-Port '' $SlaveSerial $slaveMap
    if (-not $SlavePort) { $SlavePort = $picos[1].Port }
}

Write-Host ''
Write-Host 'Flash map:' -ForegroundColor Cyan
Write-Host "  MASTER -> $MasterPort"
Write-Host "  SLAVE  -> $SlavePort"
Write-Host ''
Write-Host 'Close pio device monitor on both COM ports (port locked while open).' -ForegroundColor Yellow
Write-Host ''

Copy-Uf2ToBootloader -Uf2Path $masterUf2 -ComPort $MasterPort -Label 'can_test_master'
Copy-Uf2ToBootloader -Uf2Path $slaveUf2 -ComPort $SlavePort -Label 'can_test_slave'

Write-Host ''
Write-Host 'Done. Start monitors, then reset both boards for the 10s serial grace.' -ForegroundColor Green
