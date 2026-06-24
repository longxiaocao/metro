$file = "d:\Program Files (x86)\desktop\metro\MeteorBladeEnhancer\ddfix\ddraw\GBufferRenderer.h"
$lines = Get-Content $file -Encoding UTF8
Write-Host "=== Lines 100-105 (raw content) ==="
for ($i = 99; $i -lt 105; $i++) {
    $lineNum = $i + 1
    $line = $lines[$i]
    Write-Host ("{0:D3}: [{1}]" -f $lineNum, $line)
}

Write-Host "`n=== Lines 115-120 (raw content) ==="
for ($i = 114; $i -lt 120; $i++) {
    $lineNum = $i + 1
    $line = $lines[$i]
    Write-Host ("{0:D3}: [{1}]" -f $lineNum, $line)
}

Write-Host "`n=== Lines 160-170 (raw content) ==="
for ($i = 159; $i -lt 170; $i++) {
    $lineNum = $i + 1
    $line = $lines[$i]
    Write-Host ("{0:D3}: [{1}]" -f $lineNum, $line)
}
