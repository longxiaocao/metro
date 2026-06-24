# ci_check_bom.ps1 - CI BOM 守护脚本
#
# 目的:
#   1. HLSL (.hlsl/.fx) 文件不能有 UTF-8 BOM — fxc.exe 报 X3000 非法字符
#   2. 含中文注释的 C++ 文件必须有 UTF-8 BOM — MSVC 936 code page 解析要求
#   3. 本脚本在 CI workflow 中作为构建前门: 失败时 exit 1 阻止构建
#
# 约定:
#   - 入口: tools/ci_check_bom.ps1
#   - C++ BOM 要求文件: 复刻 tools/add_utf8_bom.ps1 的 $BOM_FILES
#     (即本地手工加 BOM 的清单; 缺 BOM = 警告而非失败, 由开发者补)
#   - HLSL 严禁 BOM: 任意 HLSL 含 BOM = 失败
#
# 与 add_utf8_bom.ps1 的分工:
#   - add_utf8_bom.ps1: 本地补 BOM 工具 (手工触发)
#   - ci_check_bom.ps1: CI 验证 (自动失败门)
#   - 两者不共享状态, 各自独立维护列表

$ErrorActionPreference = "Stop"

$ProjectRoot = "d:\Program Files (x86)\desktop\metro\meteorbladeenhancer"

# HLSL 文件严禁 BOM (硬错误, exit 1)
$HLSL_GLOB = @(
    "ddfix\shaders\*.hlsl"
    "ddfix\ddraw\*.hlsl"
    "ddfix\shaders\SMAA\*.fx"
)

# C++ 文件必须有 BOM (与 add_utf8_bom.ps1 同步)
# 若 C++ 缺 BOM → 输出 WARNING, 不失败 (避免本地快速修复被 CI 阻)
$CPP_NEED_BOM = @(
    "ddfix\ddraw\ScriptApi.h"
    "ddfix\ddraw\ScriptApi.cpp"
    "ddfix\ddraw\ShadowRenderer.h"
    "ddfix\ddraw\ShadowRenderer.cpp"
    "ddfix\ddraw\GBufferRenderer.h"
    "ddfix\ddraw\GBufferRenderer.cpp"
    "ddfix\ddraw\ExecuteBufferParser.h"
    "ddfix\ddraw\ExecuteBufferParser.cpp"
    "ddfix\ddraw\PostProcess.h"
    "ddfix\ddraw\PostProcess.cpp"
    "ddfix\D3D9Context.h"
    "ddfix\D3D9Context.cpp"
    "ddfix\Common\Pillarbox.h"
    "ddfix\Common\Pillarbox.cpp"
    "ddfix\Config\ConfigManager.h"
    "ddfix\Config\ConfigManager.cpp"
    "ddfix\Config\IniParser.h"
    "ddfix\Debug\HudRenderer.h"
    "ddfix\Debug\PerfCounter.h"
    "ddfix\Debug\PerfCounter.cpp"
    "ddfix\Common\Logging.h"
    "ddfix\Common\Logging.cpp"
    "tests\SingleTest.h"
    "tests\main.cpp"
    "tests\test_d3d9_context.cpp"
    "tests\test_idirectdraw.cpp"
    "tests\test_smaa.cpp"
    "tests\test_execute_buffer.cpp"
    "tests\test_gbuffer.cpp"
    "tests\test_deferred_lighting.cpp"
    "tests\test_script_api.cpp"
    "tests\test_shadow.cpp"
)

function Test-HasBom {
    param([string]$path)
    if (-not (Test-Path $path)) { return $false }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -lt 3) { return $false }
    return ($bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
}

$hlslViolations = @()
$cppWarnings = @()

# === 阶段 1: HLSL 严禁 BOM (硬错误) ===
Write-Host "=== Stage 1: HLSL must NOT have BOM ===" -ForegroundColor Cyan
foreach ($pattern in $HLSL_GLOB) {
    $fullPattern = Join-Path $ProjectRoot $pattern
    $files = Get-ChildItem -Path $fullPattern -ErrorAction SilentlyContinue
    foreach ($f in $files) {
        $rel = $f.FullName.Substring($ProjectRoot.Length).TrimStart('\', '/')
        if (Test-HasBom $f.FullName) {
            Write-Host "  [FAIL] $rel has BOM (X3000 violation)" -ForegroundColor Red
            $hlslViolations += $rel
        } else {
            Write-Host "  [OK]   $rel" -ForegroundColor Green
        }
    }
}

# === 阶段 2: C++ 应有 BOM (警告) ===
Write-Host ""
Write-Host "=== Stage 2: C++ files should have BOM (warning only) ===" -ForegroundColor Cyan
foreach ($rel in $CPP_NEED_BOM) {
    $path = Join-Path $ProjectRoot $rel
    if (-not (Test-Path $path)) {
        Write-Host "  [SKIP] $rel (not found)" -ForegroundColor DarkGray
        continue
    }
    if (Test-HasBom $path) {
        Write-Host "  [OK]   $rel" -ForegroundColor Green
    } else {
        Write-Host "  [WARN] $rel missing BOM (MSVC 936 will misparse CJK comments)" -ForegroundColor Yellow
        $cppWarnings += $rel
    }
}

# === 总结 ===
Write-Host ""
Write-Host "===== Summary =====" -ForegroundColor Cyan
Write-Host "HLSL violations: $($hlslViolations.Count) (must be 0)"
Write-Host "C++ warnings:    $($cppWarnings.Count) (recommend 0)"

if ($hlslViolations.Count -gt 0) {
    Write-Host ""
    Write-Host "FAILED: HLSL files with BOM will break fxc.exe (X3000)." -ForegroundColor Red
    Write-Host "Run tools/fix_bom.ps1 to strip BOM from HLSL files." -ForegroundColor Red
    exit 1
}

if ($cppWarnings.Count -gt 0) {
    Write-Host ""
    Write-Host "WARNING: C++ files missing BOM may cause MSVC 936 parse errors." -ForegroundColor Yellow
    Write-Host "Run tools/add_utf8_bom.ps1 to add BOM." -ForegroundColor Yellow
    # NOTE: missing BOM on C++ does NOT block CI, only warning
}

Write-Host "OK: BOM policy satisfied." -ForegroundColor Green
exit 0
