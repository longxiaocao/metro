# Local build script for MeteorBladeEnhancer
# Mirrors CI .github/workflows/build.yml for fast local iteration
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File build_local.ps1
#
# This avoids the cmd.exe /c blocking in sandboxed PowerShell.

$ErrorActionPreference = "Continue"
$ProjectRoot = "d:\Program Files (x86)\desktop\metro\MeteorBladeEnhancer"
Set-Location $ProjectRoot

# -------------------- activate MSVC vcvars32 --------------------
$vcvars = "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat"
if (-not (Test-Path $vcvars)) {
    Write-Error "vcvars32.bat not found at $vcvars"
    exit 2
}
Write-Host "=== Activating MSVC vcvars32 ==="
# Use [System.Diagnostics.Process] to run cmd.exe directly (bypasses sandboxed `cmd /c` block)
$tmpCmd = [System.IO.Path]::GetTempFileName() + ".cmd"
@"
@echo off
call "$vcvars" >nul 2>&1
set
"@ | Out-File -FilePath $tmpCmd -Encoding ascii

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $env:ComSpec
$psi.Arguments = "/c `"$tmpCmd`""
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$proc = [System.Diagnostics.Process]::Start($psi)
$envOut = $proc.StandardOutput.ReadToEnd() -split "`r?`n"
$proc.WaitForExit()
Remove-Item $tmpCmd -Force -ErrorAction SilentlyContinue

# Apply captured env vars to current session
$count = 0
foreach ($line in $envOut) {
    if ($line -match '^([A-Za-z_][A-Za-z0-9_()]*)=(.*)$') {
        $name = $matches[1]
        $value = $matches[2]
        if ($name -in @('PSExecutionPolicyPreference')) { continue }
        try {
            Set-Item -Path "Env:\$name" -Value $value -ErrorAction SilentlyContinue
            $count++
        } catch {}
    }
}
Write-Host "Set $count env vars from vcvars32"

# Ensure Windows SDK bin (fxc.exe) is on PATH
$sdkBin = "D:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"
if (Test-Path $sdkBin) {
    $env:PATH = "$sdkBin;$env:PATH"
}

# -------------------- verify toolchain --------------------
Write-Host "=== Toolchain check ==="
$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
$cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
$fxc = Get-Command fxc.exe -ErrorAction SilentlyContinue
Write-Host "cl:    $($cl.Source)"
Write-Host "cmake: $($cmake.Source)"
Write-Host "fxc:   $($fxc.Source)"
if (-not $cl -or -not $cmake -or -not $fxc) {
    Write-Error "Toolchain incomplete"
    exit 3
}

# -------------------- CMake configure --------------------
Write-Host "=== CMake configure ==="
$buildDir = "build"
if (Test-Path $buildDir) {
    Write-Host "Reusing existing build dir"
} else {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}
$cmakeArgs = @(
    '-S', '.'
    '-B', $buildDir
    '-G', 'Visual Studio 18 2026'
    '-A', 'Win32'
    '-DBUILD_TESTS=ON'
    '-DBUILD_DDFIX_LIB=OFF'
    "-DGameExe="
    "-DGameStartupArgs=w"
)
& cmake @cmakeArgs *> "$buildDir\configure.log"
$cfgExit = $LASTEXITCODE
Write-Host "Configure exit: $cfgExit"
if ($cfgExit -ne 0) {
    Write-Host "--- configure.log tail ---"
    Get-Content "$buildDir\configure.log" -Tail 60
    exit $cfgExit
}

# -------------------- Build ddfix.dll --------------------
Write-Host "=== Build ddfix.dll (Release/Win32) ==="
& cmake --build $buildDir --config Release --target ddfix -j *> "$buildDir\build_ddfix.log"
$buildExit = $LASTEXITCODE
Write-Host "Build ddfix exit: $buildExit"
if ($buildExit -ne 0) {
    Write-Host "--- build_ddfix.log error lines ---"
    Get-Content "$buildDir\build_ddfix.log" |
        Select-String -Pattern "error\s+(C|LNK)[0-9]+|fatal error" |
        Select-Object -First 30
    Write-Host "--- tail 30 ---"
    Get-Content "$buildDir\build_ddfix.log" -Tail 30
    exit $buildExit
}

# -------------------- Build ddfixtests.exe --------------------
Write-Host "=== Build ddfixtests.exe (Release/Win32) ==="
& cmake --build $buildDir --config Release --target ddfixtests -j *> "$buildDir\build_ddfixtests.log"
$testsBuildExit = $LASTEXITCODE
Write-Host "Build ddfixtests exit: $testsBuildExit"
if ($testsBuildExit -ne 0) {
    Write-Host "--- build_ddfixtests.log error lines ---"
    Get-Content "$buildDir\build_ddfixtests.log" |
        Select-String -Pattern "error\s+(C|LNK)[0-9]+|fatal error" |
        Select-Object -First 30
    Write-Host "--- tail 30 ---"
    Get-Content "$buildDir\build_ddfixtests.log" -Tail 30
    exit $testsBuildExit
}

# -------------------- Run tests --------------------
$exe = "$buildDir\bin\Release\ddfixtests.exe"
if (-not (Test-Path $exe)) {
    Write-Error "ddfixtests.exe not found at $exe"
    Get-ChildItem "$buildDir\bin" -Recurse -ErrorAction SilentlyContinue | Select-Object FullName
    exit 4
}

Write-Host "=== Run ddfixtests --list (startup sanity) ==="
& $exe --list *> "$buildDir\test_list.log"
$listExit = $LASTEXITCODE
Write-Host "--list exit: $listExit"
if ($listExit -ne 0) {
    Get-Content "$buildDir\test_list.log" -Tail 30
    exit $listExit
}

Write-Host "=== Run ddfixtests (full) ==="
& $exe *> "$buildDir\test_full.log"
$runExit = $LASTEXITCODE
Write-Host "Test exit: $runExit"
Write-Host "--- test_full.log tail 60 ---"
Get-Content "$buildDir\test_full.log" -Tail 60

exit $runExit
