# Shared helpers for flashing RP2040 over USB (BOOTSEL via 1200 baud touch + RPI-RP2 UF2 copy).
$ErrorActionPreference = 'Stop'

function Get-PicoUsbSerialDevices {
    $text = (& pio device list 2>&1 | Out-String)
    $blocks = $text -split '(?=^COM\d+)', 0, 'Multiline' | Where-Object { $_ -match '^COM' }
  $devices = @()
  foreach ($block in $blocks) {
    if ($block -notmatch '^(?<port>COM\d+)') { continue }
    $port = $Matches.port
    $ser = $null
    if ($block -match 'SER=(?<ser>[0-9A-F]+)') { $ser = $Matches.ser }
    if ($block -notmatch 'VID:PID=2E8A:000A') { continue }
    $devices += [pscustomobject]@{ Port = $port; Serial = $ser }
  }
  $devices | Sort-Object Port
}

function Invoke-Pico1200Reset {
    param([Parameter(Mandatory)][string]$ComPort)
    $parity = [System.IO.Ports.Parity]::None
    $stop = [System.IO.Ports.StopBits]::One
    $port = New-Object System.IO.Ports.SerialPort @($ComPort, 1200, $parity, 8, $stop)
    $port.DtrEnable = $true
    $port.RtsEnable = $true
    try {
        $port.Open()
        Start-Sleep -Milliseconds 400
        $port.Close()
    }
    finally {
        $port.Dispose()
    }
}

function Wait-RpiRp2Volume {
    param(
        [int]$TimeoutSec = 25,
        [string]$ExpectAbsentLetter = ''
    )
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $vols = @(Get-Volume -ErrorAction SilentlyContinue | Where-Object { $_.FileSystemLabel -eq 'RPI-RP2' -and $_.DriveLetter })
        if ($ExpectAbsentLetter -ne '' -and $vols.Count -gt 1) {
            $vols = @($vols | Where-Object { $_.DriveLetter -ne $ExpectAbsentLetter })
        }
        if ($vols.Count -eq 1) {
            return $vols[0]
        }
        if ($vols.Count -gt 1) {
            throw "Multiple RPI-RP2 drives (${vols.Count}). Flash one Pico at a time (close other BOOTSEL devices)."
        }
        Start-Sleep -Milliseconds 400
    }
    return $null
}

function Copy-Uf2ToBootloader {
    param(
        [Parameter(Mandatory)][string]$Uf2Path,
        [Parameter(Mandatory)][string]$ComPort,
        [string]$Label = 'Pico'
    )
    if (-not (Test-Path $Uf2Path)) {
        throw "UF2 not found: $Uf2Path"
    }
    Write-Host "Flashing $Label via $ComPort ..." -ForegroundColor Cyan
    Invoke-Pico1200Reset -ComPort $ComPort
    Start-Sleep -Seconds 1
    $vol = Wait-RpiRp2Volume -TimeoutSec 30
    if (-not $vol) {
        throw "No RPI-RP2 volume after reset on $ComPort. Close serial monitors using that port and retry."
    }
    $dest = "$($vol.DriveLetter):\"
    Copy-Item -Path $Uf2Path -Destination $dest -Force
    Write-Host "Copied UF2 to $dest — waiting for reboot ..." -ForegroundColor Green
    $deadline = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $deadline) {
        $still = Get-Volume -ErrorAction SilentlyContinue | Where-Object { $_.DriveLetter -eq $vol.DriveLetter -and $_.FileSystemLabel -eq 'RPI-RP2' }
        if (-not $still) { break }
        Start-Sleep -Milliseconds 300
    }
    Start-Sleep -Seconds 2
}
