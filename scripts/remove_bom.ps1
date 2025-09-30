Param([string]$Path)
$bytes = [System.IO.File]::ReadAllBytes($Path)
if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
    $new = New-Object byte[] ($bytes.Length - 3)
    [System.Array]::Copy($bytes, 3, $new, 0, $new.Length)
    [System.IO.File]::WriteAllBytes($Path, $new)
}