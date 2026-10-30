# press.ps1 <frames tag> <button> [<button> ...]: send each button (200 ms) through the live
# pad file with 700 ms between presses, wait, then save the newest frame as frames\<tag>_now.png.
param([string]$tag, [int]$wait = 4)
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$python = if ($env:PYTHON) { $env:PYTHON } else { "python" }   # needs Pillow
foreach ($b in $args) {
    "${b}:200" | Out-File "$repo\pad_live.txt" -Append -Encoding ascii
    Start-Sleep -Milliseconds 700
}
Start-Sleep $wait
& $python "$repo\scripts\latest.py" "$repo\frames\$tag" "$repo\frames\${tag}_now.png"
