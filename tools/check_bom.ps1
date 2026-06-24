$ErrorActionPreference = "Continue"

$ProjectRoot = "d:\Program Files (x86)\desktop\metro\meteorbladeenhancer"

# 需要检查的文件列表
$CHECK_FILES = @(
    # HLSL 文件 - fxc.exe 不支持带 BOM 的文件
    "ddfix\shaders\Deferred.hlsl",
    "ddfix\shaders\GBuffer.hlsl",
    "ddfix\shaders\Shadow.hlsl",
    "ddfix\ddraw\ColorKey.hlsl",
    "ddfix\shaders\SMAA\SMAA.fx",
    "ddfix\shaders\SMAA\SMAA_BlendingWeight.fx",
    "ddfix\shaders\SMAA\SMAA_EdgeDetection.fx",
    "ddfix\shaders\SMAA\SMAA_NeighborhoodBlending.fx",
    # C++ 文件 - 含中文注释需要 BOM
    "ddfix\ddraw\GBufferRenderer.h",
    "ddfix\ddraw\GBufferRenderer.cpp",
    "ddfix\ddraw\ShadowRenderer.h",
    "ddfix\ddraw\ShadowRenderer.cpp",
    "ddfix\ddraw\ScriptApi.h",
    "ddfix\ddraw\ScriptApi.cpp",
    "ddfix\ddraw\ExecuteBufferParser.h",
    "ddfix\ddraw\ExecuteBufferParser.cpp",
    "ddfix\ddraw\PostProcess.h",
    "ddfix\ddraw\PostProcess.cpp",
    "ddfix\D3D9Context.h",
    "ddfix\D3D9Context.cpp"
)

Write-Host "=== BOM 检查 ===" -ForegroundColor Cyan
$hlslWithBom = @()
$cppWithoutBom = @()
$cppWithBom = @()

foreach ($rel in $CHECK_FILES) {
    $path = Join-Path $ProjectRoot $rel
    if (-not (Test-Path $path)) {
        Write-Host "[MISSING] $rel" -ForegroundColor Red
        continue
    }

    $bytes = [System.IO.File]::ReadAllBytes($path)
    $hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF

    $isHlsl = $rel -match '\.(hlsl|fx)$'
    if ($isHlsl) {
        if ($hasBom) {
            Write-Host "[HLSL-BOM-NEED-REMOVE] $rel" -ForegroundColor Yellow
            $script:hlslWithBom += $rel
        } else {
            Write-Host "[HLSL-OK] $rel" -ForegroundColor Green
        }
    } else {
        if ($hasBom) {
            Write-Host "[CPP-BOM-OK] $rel" -ForegroundColor Green
            $script:cppWithBom += $rel
        } else {
            Write-Host "[CPP-BOM-MISSING] $rel" -ForegroundColor Magenta
            $script:cppWithoutBom += $rel
        }
    }
}

Write-Host ""
Write-Host "=== 汇总 ===" -ForegroundColor Cyan
Write-Host "HLSL 文件需要移除 BOM: $($hlslWithBom.Count)"
Write-Host "C++ 文件已有 BOM: $($cppWithBom.Count)"
Write-Host "C++ 文件缺少 BOM: $($cppWithoutBom.Count)"

if ($cppWithoutBom.Count -gt 0) {
    Write-Host ""
    Write-Host "缺少 BOM 的 C++ 文件:" -ForegroundColor Yellow
    $cppWithoutBom | ForEach-Object { Write-Host "  $_" }
}
