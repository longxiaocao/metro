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

# -------------------- Build ddfix.dll --------------------
Write-Host "=== Build ddfix.dll (Release/Win32) ==="
$buildDir = "build"
& cmake --build $buildDir --config Release --target ddfix -j 2>&1 | Tee-Object -FilePath "$buildDir\build_ddfix.log" | Select-Object -Last 20
$buildExit = $LASTEXITCODE
Write-Host "Build ddfix exit: $buildExit"
exit $buildExit
