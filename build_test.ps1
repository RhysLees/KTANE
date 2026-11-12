#!/usr/bin/env pwsh

# KTANE Test Build Script — firmware under testing/debugging/

Write-Host "KTANE Test Build Script" -ForegroundColor Cyan

$firmwareDir = "firmware"
$buildDir = "build"
$debuggerRoot = "testing/debugging"

$modules = @()
if (Test-Path $debuggerRoot) {
    Get-ChildItem -Path $debuggerRoot -Recurse -Directory | Where-Object {
        Test-Path (Join-Path $_.FullName "platformio.ini")
    } | ForEach-Object {
        $rel = $_.FullName.Substring((Get-Location).Path.Length + 1) -replace '\\', '/'
        $modules += $rel
        Write-Host "  Found test module: $rel" -ForegroundColor Gray
    }
}

if ($modules.Count -eq 0) {
    Write-Host "No test harnesses found under $debuggerRoot" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $firmwareDir)) {
    New-Item -ItemType Directory -Path $firmwareDir | Out-Null
}

$successCount = 0
$failCount = 0

foreach ($module in $modules) {
    $leaf = Split-Path $module -Leaf
    Write-Host ""
    Write-Host "Building $module ..." -ForegroundColor Yellow

    Push-Location $module

    try {
        $buildResult = & pio run 2>&1

        if ($LASTEXITCODE -eq 0) {
            Write-Host "$leaf build successful" -ForegroundColor Green
            $successCount++

            $depth = ($module -split '/').Count
            $repoRoot = (Get-Item (Join-Path $module ('../' * $depth))).FullName
            $uf2Search = Join-Path $repoRoot (Join-Path $buildDir $leaf)
            $uf2Files = Get-ChildItem -Path $uf2Search -Filter "*.uf2" -Recurse -ErrorAction SilentlyContinue

            if ($uf2Files.Count -gt 0) {
                foreach ($uf2File in $uf2Files) {
                    if ($uf2File.Name -eq 'firmware.uf2') {
                        $destinationName = "$leaf.uf2"
                    }
                    else {
                        $destinationName = "${leaf}_$($uf2File.Name)"
                    }
                    $destinationPath = Join-Path (Join-Path $repoRoot $firmwareDir) $destinationName
                    Copy-Item -Path $uf2File.FullName -Destination $destinationPath -Force
                }
            }
        }
        else {
            Write-Host "$leaf build failed" -ForegroundColor Red
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
Write-Host "Successful: $successCount  Failed: $failCount" -ForegroundColor Cyan
if ($failCount -ne 0) { exit 1 }
