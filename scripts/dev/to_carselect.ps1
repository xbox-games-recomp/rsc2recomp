# to_carselect.ps1 <tag>: fresh save dir, boot, create a profile, Career -> Amateur -> first event
# (the car selection screen). Leaves the game running; frames in frames\<tag>, log in <tag>.err.
param([string]$tag, [string]$extraEnv = "")
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$press = "$PSScriptRoot\press.ps1"
Set-Location $repo
Get-Process rsc2_recomp -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1
Remove-Item "$repo\savedata\UDATA" -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "frames\$tag" | Out-Null
"" | Out-File pad_live.txt -Encoding ascii
$env:RECOMP_SAVE_DIR = "$repo\savedata"; $env:RECOMP_WATCHDOG_SECS = "900"; $env:RECOMP_AC97_READY = "1"
$env:RECOMP_HEAP_RECLAIM = "1"; $env:RECOMP_DISPATCH_GATE = "1"; $env:RECOMP_PB_EXEC = "1"; $env:RECOMP_D3D11 = "1"
$env:RECOMP_PAD_LIVE = "$repo\pad_live.txt"; $env:RECOMP_D3D11_DUMP = "frames\$tag\f"; $env:RECOMP_D3D11_DUMP_MS = "2000"
foreach ($kv in ($extraEnv -split ';')) { if ($kv) { $k, $v = $kv -split '=', 2; Set-Item "env:$k" $v } }
$p = Start-Process build\Release\rsc2_recomp.exe -RedirectStandardError "$tag.err" -RedirectStandardOutput "$tag.out" -PassThru -WindowStyle Hidden
"pid $($p.Id)"
Start-Sleep 40
foreach ($i in 1..8) { "start:200" | Out-File pad_live.txt -Append -Encoding ascii; Start-Sleep 4 }
Start-Sleep 20
& $press $tag -wait 2 down down down down right right right right
& $press $tag -wait 6 a        # Done
& $press $tag -wait 6 a        # select the new profile
& $press $tag -wait 6 a        # Career
& $press $tag -wait 8 left a   # Amateur
& $press $tag -wait 12 a       # first event -> car selection
