$ErrorActionPreference = "Continue"
$path = "d:\Program Files (x86)\desktop\metro\MeteorBladeEnhancer\tests\test_shadow.cpp"
$bytes = [System.IO.File]::ReadAllBytes($path)
$hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
if ($hasBom) {
    Write-Host "test_shadow.cpp: BOM already present"
} else {
    $newBytes = New-Object byte[] ($bytes.Length + 3)
    $newBytes[0] = 0xEF
    $newBytes[1] = 0xBB
    $newBytes[2] = 0xBF
    [Array]::Copy($bytes, 0, $newBytes, 3, $bytes.Length)
    [System.IO.File]::WriteAllBytes($path, $newBytes)
    Write-Host "test_shadow.cpp: BOM added"
}
