$ErrorActionPreference = "Continue"

$ProjectRoot = "d:\Program Files (x86)\desktop\metro\MeteorBladeEnhancer"

# All HLSL/fx files under ddfix/shaders
$HLSL_FILES = @(
    "ddfix\shaders\Deferred.hlsl",
    "ddfix\shaders\GBuffer.hlsl",
    "ddfix\shaders\Shadow.hlsl",
    "ddfix\ddraw\ColorKey.hlsl",
    "ddfix\shaders\SMAA\SMAA.fx",
    "ddfix\shaders\SMAA\SMAA_BlendingWeight.fx",
    "ddfix\shaders\SMAA\SMAA_EdgeDetection.fx",
    "ddfix\shaders\SMAA\SMAA_NeighborhoodBlending.fx"
)

Write-Host "=== HLSL BOM check ===" -ForegroundColor Cyan
foreach ($rel in $HLSL_FILES) {
    $path = Join-Path $ProjectRoot $rel
    if (-not (Test-Path $path)) {
        Write-Host "[MISSING] $rel" -ForegroundColor Red
        continue
    }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $bom = "no"
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $bom = "YES"
    }
    $firstBytes = if ($bytes.Length -ge 8) {
        (($bytes[0..7] | ForEach-Object { $_.ToString('X2') }) -join ' ')
    } else {
        "($($bytes.Length) bytes)"
    }
    Write-Host ("$rel  BOM={0}  first 8 bytes: {1}" -f $bom, $firstBytes)
}
