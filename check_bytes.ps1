$file = "d:\Program Files (x86)\desktop\metro\MeteorBladeEnhancer\ddfix\ddraw\GBufferRenderer.h"
$content = [System.IO.File]::ReadAllText($file)
$lines = $content -split "`n"
Write-Host "Total lines: $($lines.Count)"

# Get bytes of lines 100-105
for ($i = 99; $i -lt 105 -and $i -lt $lines.Count; $i++) {
    $lineNum = $i + 1
    $line = $lines[$i] -replace "`r", ""
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($line)
    $byteStr = ($bytes | ForEach-Object { $_.ToString('X2') }) -join ' '
    Write-Host ("Line {0} [len={1}]: {2}" -f $lineNum, $bytes.Length, $byteStr)
    Write-Host "  Text: $line"
}

# Also check the line where m_enabled is used
Write-Host "`n=== m_enabled usage lines ==="
for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i] -replace "`r", ""
    if ($line -match 'm_enabled') {
        $lineNum = $i + 1
        Write-Host ("Line {0}: {1}" -f $lineNum, $line)
    }
}

# Check the encoding of the file
$firstBytes = [System.IO.File]::ReadAllBytes($file)[0..2]
$bom = ($firstBytes | ForEach-Object { $_.ToString('X2') }) -join ' '
Write-Host "`nFirst 3 bytes: $bom (BOM EF BB BF = UTF-8 BOM)"

# Check for the specific issue
Write-Host "`n=== Search for 'm_enabled' in file (not in comments) ==="
$inBlockComment = $false
for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i] -replace "`r", ""
    # Strip single-line comments
    $code = $line -replace '//.*$', ''
    if ($code -match 'm_enabled') {
        $lineNum = $i + 1
        Write-Host ("Line {0}: {1}" -f $lineNum, $line)
    }
}
