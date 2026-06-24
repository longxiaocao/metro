$ErrorActionPreference = "Continue"
$path = "d:\Program Files (x86)\desktop\metro\meteorbladeenhancer\tools\ci_check_bom.ps1"
$bytes = [System.IO.File]::ReadAllBytes($path)
$hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
Write-Host "BOM: $hasBom (length=$($bytes.Length))"
if ($hasBom) {
    $newBytes = New-Object byte[] ($bytes.Length - 3)
    [Array]::Copy($bytes, 3, $newBytes, 0, $newBytes.Length)
    [System.IO.File]::WriteAllBytes($path, $newBytes)
    Write-Host "BOM stripped"
}
