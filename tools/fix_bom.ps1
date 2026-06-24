$ErrorActionPreference = "Continue"

$ProjectRoot = "d:\Program Files (x86)\desktop\metro\meteorbladeenhancer"

# HLSL files: fxc.exe rejects BOM. Strip if present.
$HLSL_FILES = @(
    "ddfix\shaders\GBuffer.hlsl",
    "ddfix\shaders\Shadow.hlsl",
    "ddfix\shaders\SMAA\SMAA_BlendingWeight.fx",
    "ddfix\shaders\SMAA\SMAA_EdgeDetection.fx",
    "ddfix\shaders\SMAA\SMAA_NeighborhoodBlending.fx"
)

# C++ files containing CJK comments: need BOM for MSVC code page 936.
$CPP_FILES_NEED_BOM = @(
    "ddfix\ddraw\ScriptApi.h",
    "ddfix\D3D9Context.cpp"
)

function Remove-BomIfPresent {
    param([string]$relPath)
    $path = Join-Path $ProjectRoot $relPath
    if (-not (Test-Path $path)) {
        Write-Host "[MISSING] $relPath" -ForegroundColor Red
        return
    }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $newBytes = New-Object byte[] ($bytes.Length - 3)
        [Array]::Copy($bytes, 3, $newBytes, 0, $newBytes.Length)
        [System.IO.File]::WriteAllBytes($path, $newBytes)
        Write-Host "[BOM REMOVED] $relPath" -ForegroundColor Yellow
    } else {
        Write-Host "[NO BOM] $relPath" -ForegroundColor Green
    }
}

function Add-BomIfMissing {
    param([string]$relPath)
    $path = Join-Path $ProjectRoot $relPath
    if (-not (Test-Path $path)) {
        Write-Host "[MISSING] $relPath" -ForegroundColor Red
        return
    }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        Write-Host "[BOM PRESENT] $relPath" -ForegroundColor Green
        return
    }
    $newBytes = New-Object byte[] ($bytes.Length + 3)
    $newBytes[0] = 0xEF
    $newBytes[1] = 0xBB
    $newBytes[2] = 0xBF
    [Array]::Copy($bytes, 0, $newBytes, 3, $bytes.Length)
    [System.IO.File]::WriteAllBytes($path, $newBytes)
    Write-Host "[BOM ADDED] $relPath" -ForegroundColor Green
}

Write-Host "=== Step 1: Strip BOM from HLSL files ===" -ForegroundColor Cyan
foreach ($rel in $HLSL_FILES) {
    Remove-BomIfPresent -relPath $rel
}

Write-Host ""
Write-Host "=== Step 2: Add BOM to C++ files with CJK comments ===" -ForegroundColor Cyan
foreach ($rel in $CPP_FILES_NEED_BOM) {
    Add-BomIfMissing -relPath $rel
}

Write-Host ""
Write-Host "=== Done ===" -ForegroundColor Cyan
