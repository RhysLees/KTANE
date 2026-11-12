#!/usr/bin/env pwsh

# KTANE Module Build Script
# Builds all module firmwares and copies uf2 files to firmware directory

Write-Host "KTANE Module Build Script" -ForegroundColor Cyan
Write-Host "=============================" -ForegroundColor Cyan

$firmwareDir = "firmware"
$buildDir = "build"

Write-Host "Auto-detecting module directories..." -ForegroundColor Cyan
$modules = @()
foreach ($root in @("modules", "edgework", "support")) {
    if (-not (Test-Path $root)) { continue }
    Get-ChildItem -Path $root -Directory | Where-Object {
        -not ($root -eq "support" -and $_.Name -eq "debugger") -and
        (Test-Path (Join-Path $_.FullName "platformio.ini"))
    } | ForEach-Object {
        $rel = Join-Path $root $_.Name
        $modules += ($rel -replace '\\', '/')
        Write-Host "  Found module: $rel" -ForegroundColor Gray
    }
}

if ($modules.Count -eq 0) {
    Write-Host "No modules found." -ForegroundColor Red
    exit 1
}

Write-Host "Detected $($modules.Count) modules: $($modules -join ', ')" -ForegroundColor Green

if (-not (Test-Path $firmwareDir)) {
    New-Item -ItemType Directory -Path $firmwareDir | Out-Null
}

Get-ChildItem -Path $firmwareDir -Filter "*.uf2" -ErrorAction SilentlyContinue | Where-Object {
    -not ($_.Name -like "*can_debug*" -or $_.Name -like "*can_test*")
} | Remove-Item -Force -ErrorAction SilentlyContinue

$successCount = 0
$failCount = 0

foreach ($module in $modules) {
    $leaf = Split-Path $module -Leaf
    Write-Host ""
    Write-Host "Building $module ..." -ForegroundColor Yellow

    if (-not (Test-Path $module)) {
        Write-Host "Module directory '$module' not found" -ForegroundColor Red
        $failCount++
        continue
    }

    Push-Location $module

    try {
        $buildResult = & pio run 2>&1

        if ($LASTEXITCODE -eq 0) {
            Write-Host "$leaf build successful" -ForegroundColor Green
            $successCount++

            $uf2Search = Join-Path (Join-Path ".." "..") (Join-Path $buildDir $leaf)
            $uf2Files = Get-ChildItem -Path $uf2Search -Filter "*.uf2" -Recurse -ErrorAction SilentlyContinue

            if ($uf2Files.Count -gt 0) {
                foreach ($uf2File in $uf2Files) {
                    $destinationName = "${leaf}_$($uf2File.Name)"
                    $repoRoot = Join-Path ".." ".."
                    $destinationPath = Join-Path (Join-Path $repoRoot $firmwareDir) $destinationName
                    Copy-Item -Path $uf2File.FullName -Destination $destinationPath
                }
            }
        }
        else {
            Write-Host "$leaf build failed" -ForegroundColor Red
            Write-Host $buildResult -ForegroundColor Red
            $failCount++
        }
    }
    catch {
        Write-Host "Error building ${leaf}: $($_.Exception.Message)" -ForegroundColor Red
        $failCount++
    }
    finally {
        Pop-Location
    }
}

Write-Host ""
Write-Host "Successful builds: $successCount" -ForegroundColor Green
Write-Host "Failed builds: $failCount" -ForegroundColor Red

if ($failCount -ne 0) { exit 1 }
