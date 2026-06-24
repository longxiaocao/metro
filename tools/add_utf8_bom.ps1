# add_utf8_bom.ps1 - 批量给指定文件加 UTF-8 BOM (EF BB BF)
#
# 背景: MSVC 在 code page 936 下, 含中文注释的 UTF-8 文件需要 BOM 标识
#   文件是 UTF-8 编码. 缺 BOM 时, 编译器把多字节 UTF-8 字符拆成单字节解释,
#   触发 C1004 / C2065 / C2447 等诡异错误.
#
# 警告: HLSL (.hlsl/.fx) 文件不能加 BOM, fxc.exe 解析时会把 BOM 当非法字符 (X3000)
#   编译失败. 脚本自动跳过 HLSL 扩展名.
#
# 用法:
#   powershell -NoProfile -ExecutionPolicy Bypass -File add_utf8_bom.ps1
#
# 维护:
#   - 本文件被本项目的 .github CI workflow / build_local.ps1 引用
#   - 任何新增含中文注释的 .h/.cpp/.hlsl 都应加入 $BOM_FILES 列表
#   - 若文件已含 BOM, 跳过不重复写

$ErrorActionPreference = "Continue"

# 待加 BOM 的文件 (相对项目根)
$ProjectRoot = "d:\Program Files (x86)\desktop\metro\meteorbladeenhancer"
$BOM_FILES = @(
    # Phase 9.4 新增 / 改动的文件
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
    # 单元测试
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

# 严禁加 BOM 的文件: fxc.exe 解析带 BOM 的 HLSL 失败 (X3000)
$EXEMPT_EXTENSIONS = @('.hlsl', '.fx')

$added = 0
$skipped = 0
$missing = 0
$exempted = 0

foreach ($rel in $BOM_FILES) {
    $ext = [System.IO.Path]::GetExtension($rel).ToLowerInvariant()
    if ($EXEMPT_EXTENSIONS -contains $ext) {
        Write-Host "[EXEMPT] $rel (HLSL/fx cannot have BOM)"
        $exempted++
        continue
    }

    $path = Join-Path $ProjectRoot $rel
    if (-not (Test-Path $path)) {
        Write-Host "[MISSING] $rel"
        $missing++
        continue
    }

    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $skipped++
        continue
    }

    $newBytes = New-Object byte[] ($bytes.Length + 3)
    $newBytes[0] = 0xEF
    $newBytes[1] = 0xBB
    $newBytes[2] = 0xBF
    [Array]::Copy($bytes, 0, $newBytes, 3, $bytes.Length)
    [System.IO.File]::WriteAllBytes($path, $newBytes)
    Write-Host "[BOM ADDED] $rel"
    $added++
}

Write-Host ""
Write-Host "===== Summary ====="
Write-Host "Added:    $added"
Write-Host "Skipped:  $skipped (already have BOM)"
Write-Host "Exempted: $exempted (HLSL/fx, cannot have BOM)"
Write-Host "Missing:  $missing"
exit 0
