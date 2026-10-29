# press.ps1 <frames tag> <button> [<button> ...]: send each button (200 ms) through the live
# pad file with 700 ms between presses, wait, then save the newest frame as frames\<tag>_now.png.
param([string]$tag, [int]$wait = 4)
$repo = "D:\Documents\Xbox Recompilation\rsc2recomp"
foreach ($b in $args) {
    "${b}:200" | Out-File "$repo\pad_live.txt" -Append -Encoding ascii
    Start-Sleep -Milliseconds 700
}
Start-Sleep $wait
& "C:\Users\taxid\AppData\Local\Programs\Python\Python310\python.exe" "$PSScriptRoot\latest.py" "$repo\frames\$tag" "$repo\frames\${tag}_now.png"
