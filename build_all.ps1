#!/usr/bin/env pwsh

# KTANE Module Build Script
# Builds all module firmwares and copies uf2 files to firmware directory

Write-Host "🔧 KTANE Module Build Script" -ForegroundColor Cyan
Write-Host "=============================" -ForegroundColor Cyan

# Define directories to exclude from module detection
$excludeDirs = @("DOCS", "shared_libs", "build", "firmware", ".git", ".vscode", ".pio")
$firmwareDir = "firmware"
$buildDir = "build"

# Auto-detect module directories (directories with platformio.ini files)
Write-Host "🔍 Auto-detecting module directories..." -ForegroundColor Cyan
$modules = @()
Get-ChildItem -Directory | Where-Object { 
    $_.Name -notin $excludeDirs -and 
    (Test-Path (Join-Path $_.FullName "platformio.ini"))
} | ForEach-Object {
    $modules += $_.Name
    Write-Host "  • Found module: $($_.Name)" -ForegroundColor Gray
}

if ($modules.Count -eq 0) {
    Write-Host "❌ No modules found! Make sure your module directories contain platformio.ini files." -ForegroundColor Red
    exit 1
}

Write-Host "📋 Detected $($modules.Count) modules: $($modules -join ', ')" -ForegroundColor Green

# Create firmware directory if it doesn't exist
if (-not (Test-Path $firmwareDir)) {
    New-Item -ItemType Directory -Path $firmwareDir
    Write-Host "📁 Created firmware directory" -ForegroundColor Green
}

# Clean firmware directory
Write-Host "🧹 Cleaning firmware directory..." -ForegroundColor Yellow
Remove-Item -Path "$firmwareDir\*" -Recurse -Force -ErrorAction SilentlyContinue

$successCount = 0
$failCount = 0

# Build each module
foreach ($module in $modules) {
    Write-Host "`n🔨 Building $module module..." -ForegroundColor Yellow
    
    # Check if module directory exists
    if (-not (Test-Path $module)) {
        Write-Host "❌ Module directory '$module' not found" -ForegroundColor Red
        $failCount++
        continue
    }
    
    # Change to module directory and build
    Push-Location $module
    
    try {
        # Run PlatformIO build
        $buildResult = & pio run 2>&1
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ $module build successful" -ForegroundColor Green
            $successCount++
            
            # Find and copy uf2 file
            $uf2Files = Get-ChildItem -Path "..\$buildDir\$module" -Filter "*.uf2" -Recurse
            
            if ($uf2Files.Count -gt 0) {
                foreach ($uf2File in $uf2Files) {
                    $destinationName = "${module}_$($uf2File.Name)"
                    $destinationPath = "..\$firmwareDir\$destinationName"
                    Copy-Item -Path $uf2File.FullName -Destination $destinationPath
                    Write-Host "📦 Copied $($uf2File.Name) to $destinationName" -ForegroundColor Green
                }
            }
            else {
                Write-Host "⚠️  No uf2 files found for $module" -ForegroundColor Yellow
            }
        }
        else {
            Write-Host "❌ $module build failed" -ForegroundColor Red
            Write-Host $buildResult -ForegroundColor Red
            $failCount++
        }
    }
    catch {
        Write-Host "❌ Error building $module`: $($_.Exception.Message)" -ForegroundColor Red
        $failCount++
    }
    finally {
        Pop-Location
    }
}

# Summary
Write-Host "`n📊 Build Summary:" -ForegroundColor Cyan
Write-Host "=================" -ForegroundColor Cyan
Write-Host "✅ Successful builds: $successCount" -ForegroundColor Green
Write-Host "❌ Failed builds: $failCount" -ForegroundColor Red

if ($successCount -gt 0) {
    Write-Host "`n📁 Firmware files available in '$firmwareDir' directory:" -ForegroundColor Green
    Get-ChildItem -Path $firmwareDir -Filter "*.uf2" | ForEach-Object {
        $size = [math]::Round($_.Length / 1KB, 2)
        Write-Host "  • $($_.Name) ($size KB)" -ForegroundColor Gray
    }
}

if ($failCount -eq 0) {
    Write-Host "`n🎉 All modules built successfully!" -ForegroundColor Green
}
else {
    Write-Host "`n⚠️  Some modules failed to build. Check the output above for details." -ForegroundColor Yellow
}
