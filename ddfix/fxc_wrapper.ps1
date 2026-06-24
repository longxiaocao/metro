# fxc_wrapper.ps1 - fxc.exe PowerShell 包装器
#
# 背景: MSBuild x86 host process 调 x64 fxc.exe 时报 268435659 (STATUS_DLL_NOT_FOUND
#   或类似错误). 直接 PowerShell 调 fxc.exe 是 OK 的 (因为 PowerShell 进程环境不同).
#
# 解决: MSBuild 调这个 PowerShell 脚本, 脚本转发参数给 fxc.exe 并把 exit code 原样
#   透传给 MSBuild.
#
# 用法: powershell -NoProfile -ExecutionPolicy Bypass -File fxc_wrapper.ps1 <fxc-args...>
#
# 设计: 不做任何"智能"解析, 完全透传参数, 保持 CMake 端 COMMAND 简洁.

$ErrorActionPreference = "Continue"

# 优先用最新 SDK 10.0.26100.0 的 fxc.exe (Phase 9.4 起更新)
$fxc = "D:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe"
if (-not (Test-Path $fxc)) {
    $fxc = "D:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\fxc.exe"
}
if (-not (Test-Path $fxc)) {
    Write-Error "fxc.exe not found in Windows SDK 10.0.26100.0 or 10.0.19041.0"
    exit 2
}

# 透传所有参数给 fxc.exe
& $fxc @args
exit $LASTEXITCODE
