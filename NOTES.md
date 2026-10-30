# RalliSport Challenge 2 static recompilation -- engineering notes

Scaffolded 2026-10-28 from [mm3recomp](https://github.com/xbox-games-recomp/mm3recomp) (Midtown Madness 3; same
studio, DICE). Items are numbered like mm3recomp's NOTES.md ("MM3 #n" refers to that file).

Layout: toolkit = submodule `xboxrecomp\` pinned to v0.15.0 (6376eb8) with mm3recomp's
`patches\xboxrecomp.patch` (game-agnostic fixes) applied by `scripts\apply_patches.ps1`. Game files:
`D:\Documents\Xbox Recompilation\rsc2_files`, junctioned in as `game_files\` (ignored, never committed).

## 1. XBE fingerprint

| | |
|---|---|
| Title | RalliSport Challenge 2, retail, title ID `0x4D530039` |
| Build | 2004-04-02 18:32:23 UTC (cert 2004-04-06), region NA/JP/RoW, DVD_X2 |
| XDK | **5849** for every library (MM3 was 5233: MM3 byte patterns will *not* match the XDK code 1:1) |
| Debug path | `d:\code\projects\rsc2\code\rsc2\Rsc2\retail\aRSC2retail64.exe` |
| SHA-1 | `45ee3898ac1c6336f241cc8502a7bbd46673fe71` (`default.xbe`, 4,718,592 bytes) |
| Base / size | `0x00010000` / `0x005AE120` |
| Entry | `0x00070315` (set in `src/main.c`) |
| Kernel thunks | `0x0037F960`, 157 imports |
| TLS | index at `0x00415200`, 164 bytes zero fill, no data template |
| Libraries | XAPILIB, XBOXKRNL, LIBCPMT, LIBCMT, XONLINLS, XVOICE, DSOUND, D3D8LTCG, XGRAPHCL |

Sections (34), with MM3's size for comparison where it has the same section:

| Section | VA | Size | MM3 size | Notes |
|---|---|---|---|---|
| .text | 0x00012000 | 0x2DC124 | 0x247180 | game + LTCG'd D3D/CRT |
| XONLINE | 0x002EE140 | 0x21A64 | | Xbox Live |
| XNET | 0x0030FBC0 | 0x12A70 | | network stack |
| DSOUND | 0x00322640 | 0x1E8CC | 0x1FD6C | |
| BINK* (16 sections) | 0x00340F20.. | small | | Bink codec variants |
| WMADEC | 0x00355840 | 0xE10C | 0x18EFC | music is `.wma` inside `Music.fa` |
| D3D | 0x00363960 | 0x12CCC | 0x137E8 | |
| XGRPH | 0x00376640 | **0x214** | 0x1D994 | no runtime shader assembler (MM3 #28 does not apply as such) |
| XPP | 0x00376860 | 0x90FC | 0x8CF0 | USB/XAPI input -> override at API level |
| .rdata / .data | 0x0037F960 / 0x003BEE40 | 0x3F4C8 / 0x197E78 | | |
| XON_RD, DOLBY, .data1, BINKDATA | | | | data |
| $$XTIMAGE, $$XSIMAGE, DIRTY, .XTLID | | | | insertable (not loaded at boot) |

Absent compared with MM3: **XACTENG** (no XACT engine linked), **D3DX**, **WMVDEC**.

## 2. `--extra-sections` (initial choice)

`DSOUND,WMADEC,XGRPH,D3D` (in `regen.ps1`).

- DSOUND, WMADEC: MM3 #19 -- unswept sound libraries were the DirectSound crash.
- XGRPH: only 0x214 bytes here, cheap to include.
- D3D: MM3 left it unswept and its one recompiler flags bug was there (MM3 #37); trying it swept from the start.
  If the build or boot regresses because of it, drop it and record why.
- Not swept: XPP (replaced by XAPI overrides), XONLINE/XNET (stub to "no network"), BINK* (MM3 did not need
  them swept; watch for ICALL bursts there during movies).

## 3. Disc and data formats

- Disc image is a plain XDVDFS game partition at offset 0 (no video partition), 6.66 GB. Extracted with a
  `scripts\xiso_extract.py <iso> <outdir>` (139 files); extract-xiso works too

- Root: `default.xbe` plus `dashupdate.xbe`, `update.xbe`, `downloader.xbe` (Live content/dash updates; ignore).
- `Layer0\`, `Layer1\`: 119 `*.fa` = DICE **FlatArchive** (`FlatArchive.cpp` in the XBE). Header is
  `u32 compressed, u32 uncompressed` then a zlib stream; the first block is a directory of fixed-size records
  (FILETIME, offset, size, index, name). One archive per location (`Location*.fa`), per stage
  (`Track<Country>_<Type>_<n>.fa`), plus `Cars.fa`, `MenuDVD.fa`/`MenuHDD.fa`, `Music.fa`, `TrackMisc.fa`, fonts.
  Paths look like `RSC2\Content\Tracks\...`; `MenuHDD.fa` suggests a copy-to-HDD cache step at boot.
- Member types (all archives' directories): `.prop` 6705, `.res` 2923, `.dat` 1382, `.aip` 106 (AI paths),
  `.cam` 86, `.pn` 78 (pace notes), `.tpl`/`.lpt` (cars), `.wbd` 27 (wave banks: announcer, codriver,
  environment, static), `.wma` 27 (music), `.tree`, `.b`/`.baf`/`.ba` (animation), `.xbx` 5 (save images),
  `.bin` 5 (`dsp3d.bin`, `dspmono.bin`, `dspstereo.bin`, `hotspot.bin`), `.snd` (`global.snd`), `.txt`.
- **No `.psh`/`.vsh`/`.cvsx` anywhere** and no shader-version strings in the XBE: shaders are precompiled,
  presumably inside `.res` resources or `.rdata`. Expect NV2A microcode / register combiner state to arrive
  directly in the pushbuffer.
- **Custom APU DSP programs** (`dsp3d/dspmono/dspstereo.bin`, plus the `DOLBY` section): the game loads its own
  GP DSP images. Audio may need more than MM3's DirectSound path (MM3 #25) if the effects chain depends on
  them running.
- `Movies\`: `dice.bik`, `dolby.bik`, `intro.bik` (97 MB), `mslogo.bik`, `mslogo_ws.bik` (widescreen), `xsn.bik`
  (XSN Sports). Bink -> PVIDEO overlay as in MM3 #29 (needs the overlay-stop override).
- `Media\`: `Sound.xsb`/`Wave.xwb` (no XACT engine linked, not referenced by name in the XBE), `dlcustom.xpr`,
  `ArialUni.Ttf` (23 MB, Live UI / Japanese text).
- `Locale\<lang>\localestrings.str`.
- Engine source names in the XBE (43 `.cpp`): `Renderer.cpp`, `Texture.cpp`, `Track\TrackTextureStreamer.cpp`,
  `Track\TrackMesh.cpp`, `Mesh\MeshManager.cpp`, `Modules\GameModuleEnvmap.cpp`, `Script\ScriptVM.cpp`,
  `Memory\MemoryHeapManager.cpp`, `CacheManager.cpp`, `PersistentContentManager.cpp`, `Win32FileSystem.cpp`,
  `MainOptimatchHostScreen.cpp` / `MainQuickmatchScreen.cpp` (Live), ... Different engine code from MM3 (no Lua
  scripts; own script VM), so expect fewer shared game-side quirks than the studio overlap suggests.

## 4. MM3 overrides still to re-locate

`src/recomp_manual.c` keeps MM3's overrides inside `#if 0 /* MM3 reference */`. Each needs its RSC2 address
(XDK 5849, so use `tools.func_id` output, string xrefs, watchdog dumps and behaviour; byte patterns from the
5233 build are only a hint):

- [ ] D3D_BlockOnTime (GPU fence wait, honest with RECOMP_PB_EXEC) + device-pointer global
- [ ] D3D overlay stop (PVIDEO, Bink)
- [ ] XGetDevices / XInputOpen / XInputClose / XInputGetState / XInputGetCapabilities / XInputSetState
      (+ the XDEVICE_TYPE_GAMEPAD struct address)
- [ ] XNet status ("no network") and the XNet receive callback; RSC2 has Live + System Link, so expect more
      network waits than MM3
- [ ] `RECOMP_DSTRACE` needs an RSC2 `dstrace.inc` if sound tracing is wanted

## 5. First regen + boot (2026-10-28)

Regen 1 (`DSOUND,WMADEC,XGRPH,D3D`): 17,562 functions, 138 unresolved stubs, builds (~10 min end to end).
The `NativeCommandError` in the regen log is PowerShell wrapping the recompiler's stderr progress line: harmless.

Boot 1 hung in a `Sleep(0)` loop (kernel ordinal 99, ~4.6 M calls/s). Cause: XNET `sub_0030FE52` (network
startup) takes a spin lock at `0x415874` with `cmpxchg`, then `je 0x30FE7C` -- a branch *inside the same
function*. XNET was unswept, so the lifter made 0x30FE7C a separate function that was never generated: an
unresolved stub that returned with the lock held, and every later caller spun. Same class as MM3 #19 (half-lifted
library code). Fix: sweep XNET (and XONLINE, BINK, which the disassembler flagged as call targets into unswept
code). `scripts/xdis.py <va> [n]` (capstone over the XBE section table) was enough to read the spin.

## 6. GPU fence wait (D3D_BlockOnTime) re-located

Boot 3 stopped right after D3D init (640x480, interrupt vector 3 connected) with the game thread spinning in guest
code (no kernel calls, no ICALLs). Stack top return `0x00118259` follows `call 0x367F40`. MM3's fence-check byte
sequence `8b4630 8b08 8b462c 8bd0 2bd1` occurs once in RSC2's D3D, at `0x00367F4A`:

- `sub_00367F40` = BlockOnTime. **XDK 5849 passes the fence in EAX** (MM3/5233: on the stack); one stack arg
  (flags), `ret 4`.
- Device pointer global: `0x00375418` (MM3: `0x351F48`). Kick-newest-fence: `sub_00367C60(0)`, `ret 4`.

Override in `src/recomp_manual.c` (honest with `RECOMP_PB_EXEC`, `RECOMP_D3D_FENCE_FAKE=1` restores the fake).
Boot 4 logs `[D3D] GPU semaphore at 0x804EC000 (fence wait is honest)` and gets past it.

## 7. Library sweep, final shape (so far)

Seeding from boot 3 produced 33 stubs inside XPP. Seeding part of a library section is the MM3 #12 trap, so XPP
is swept whole instead (the XInput API overrides still go on top). Current `--extra-sections`:
`DSOUND,WMADEC,XGRPH,D3D,XNET,XONLINE,XPP` + every BINK section. Regen 3: 18,868 functions, 4 stubs.

**Toolkit fix:** `tools/disasm/disasm.py` looked extra sections up with `get_section(name)`, which returns one
section per name. RSC2 has four sections named `BINKYUY2`, so only one was swept and Bink's YUY2 blitter
(`0x00344F19`) stayed a stub. Now every section with a matching name is added (in `patches/xboxrecomp.patch`).

## 8. Boot 4: archives, HDD cache, first movie

With the fence fixed the title opens `d:\Layer0\{Cars,Music,Misc,MenuDVD,FontsEnglish,MenuHDD,TrackMisc}.fa`,
copies `Misc.fa`/`FontsEnglish.fa`/`MenuHDD.fa` to the `z:` utility-drive cache, probes the soundtrack DB
(`TDATA\FFFE0000\MUSIC\ST.DB`, absent: fine), and opens `d:\movies\dice.bik`. Bink then hit the unswept-BINKYUY2
stub (7) and the title crashed in a heap routine (`sub_0007418E+0x507`, write to 0xFFFFFFFF) -- treated as
fallout of the half-run Bink code until shown otherwise. Also seen: ICALL to `0x00376AE4` (XPP, seeded),
`wbinvd` reached twice (no-op, fine).

## 9. Utility-drive cache rebuilt every boot (toolkit fix)

Boot 5 (second run with a populated `<save_dir>\Cache`) hung after
`rename z:\misc.fa.cache -> z:\misc.fa` failed with `0xC0000035` (name collision). XAPI's XMountUtilityDrive
reads the cache database in Partition0 (`@2048`), finds no owner entry for the title (the toolkit never stores
one), and formats Partition5: `FSCTL_DISMOUNT_VOLUME` (0x90020) + a fresh FATX volume. The title therefore
assumes an empty z: and renames with ReplaceIfExists = FALSE. The toolkit stubbed the FSCTL and never emptied the
host directory.

Fix (toolkit, `patches/xboxrecomp.patch`): `bridge_NtFsControlFile` on `FSCTL_DISMOUNT_VOLUME` for a handle on
`Partition3..5.img` calls the new `xbox_path_format_cache()` (kernel_path.c), which empties `<save_dir>\Cache`.
Note: the shared Cache dir is also MM3's; a format wipes it there too, which is what the console does on a
title switch.

Aside: launched from the Claude desktop app, file opens under `%LOCALAPPDATA%` are redirected to the app
package's `LocalCache`, while the rename's resolved target shows the real path; both work.

## 10. XAPI fibers (toolkit has none) -- intro-movie crash

Boot 7 played `dice.bik`, `mslogo.bik`, `xsn.bik`, opened `intro.bik`, then crashed in the audio function
`sub_00235580` reading `[0xC9D9C1DE]` with ebx = 0. A build with `/DRECOMP_ABI_CHECK` (`build_abi\`) showed
`sub_002374F0` returning with ebx/esi/edi changed and **esp moved from 0x00F7FDDC to 0x016C8FF4**: it calls
`0x00082E25`, which is XAPI **SwitchToFiber**. RSC2 runs work on fibers:

| VA | XAPI |
|---|---|
| `0x00082D99` | CreateFiber(stack, start, param), ret 0xC |
| `0x00082E64` | ConvertThreadToFiber(param), ret 4 |
| `0x00082E25` | SwitchToFiber(fiber), ret 4 |
| `0x00082D54` | fiber startup thunk: SEH frame, `start(param)`, ExitThread |

Thread data (TLS slot `[0x415200]`) +0x90 = current fiber; fiber +0xC = saved esp. Swapping the guest esp is
meaningless to lifted code whose control flow lives on the host stack. Fix: `sub_00082E25` override in
`recomp_manual.c` backs each guest fiber with a host fiber (CreateFiberEx, 16 MB reserve) and saves/restores the
guest state per fiber. Details in the comment there; `RECOMP_FIBER_TRACE=1` logs switches. Worth upstreaming as a
generic toolkit feature: XAPI fibers are part of every XDK.

With the override the loading fiber (`0x016C8FF0`, start routine `0x00238610`) starts and hands back, the intro
movie plays through and the title then caches the soundtrack (`z:\music\*.wma`). A 300 s run had no crash.

## 11. Milestone: title screen on the GPU (2026-10-28)

Regen 6 (seeds from boot 8: `0x002A9700`, which failed once per frame, and thread start routines): 18,982
functions, **3 stubs** (`0x00224AB9`, `0x002B8042`, `0xE814D1D2`; none reached in recent runs). Seeds pinned as
`config/seed_functions.boot8.json`.

D3D11 run (`RECOMP_D3D11=1`, dumps every 10 s, `frames\gpu1_sheet.png`), verified in frame dumps:
loading screen with the warning text, DICE / Microsoft / XSN logos, the intro movie, **"PRESS START TO BEGIN"
title screen with correct colours**, then the attract-mode movie loop. No crash in 240 s.

- **Bink draws its frames as 640x480 textures (format 24)** through D3D, not through the PVIDEO overlay, so
  MM3's overlay-stop override (MM3 #9) is not needed here. Each movie frame re-decodes the texture (expected).
- Flips run uncapped with `RECOMP_D3D11_VSYNC=0` (thousands per second on movies); not looked at yet.
- Not checked: audio output (no `RECOMP_AUDIO_WAV` measurement yet).

## 12. Gamepad: XAPI input answered at the API level

`scripts/xrefs.py <xbe> .text <lo> <hi>` lists the game's direct calls into a section. MM3's game calls 9 XPP
functions, RSC2's 12; matched by code shape (identical instruction sequences, other addresses):

| XAPI | MM3 | RSC2 | override |
|---|---|---|---|
| XGetDeviceChanges(type, *ins, *rem), ret 12 | 0035A83F | `00377A69` | pad in port 0 inserted once |
| XGetDeviceEnumerationStatus(), ret | -- | `0037932B` | IDLE (0) |
| XInitDevices | 00361666 | 0037D708 | not overridden |
| XInputOpen, ret 16 | 00361BCF | `0037E356` | handle for gamepad port 0 |
| XInputClose, ret 4 | 00361C25 | `0037E3AC` | no-op |
| XInputGetCapabilities, ret 8 | 00361C31 | `0037E3B8` | SubType gamepad |
| XInputGetState, ret 8 | 00361E0F | `0037E590` | from `usb_gamepad_report` |
| XInputSetState, ret 8 | 00361E7B | `0037E603` | rumble -> host pad 0 |

RSC2 does not link XGetDevices; it polls XGetDeviceChanges every frame. Device types: `0x376898` gamepad
(XInputOpen's argument), `0x376A68` and `0x3769D4` / `0x3769E0` (memory unit / voice; never report changes).
`RECOMP_PAD_TRACE=1` logs the calls.

Verified in frame dumps (`frames\pad2_sheet.png`, `RECOMP_PAD_PRESS=0x10`): Start skips the movies, passes
"PRESS START", opens **Select Profile -> Create new Profile** and the on-screen name keyboard. Not yet checked
with a real pad or the keyboard mapping in the window (needs the user).

Keyboard screen draws the keys without their letters. At the same time an indirect jump to `0x000A2454` failed
~40,000 times: CRT x87 special-case fragments (`fstp; fstp; fldz; ret` = "return 0.0") dispatched through a
table in `.data` at `0x4151AC` (`A24F6 A248F A2454 A248D A24B7`). All five seeded (regen 8): the failures are
gone (pad3 run), **but the key letters are still missing**, so that was not the cause. Other text on the same
screen ("SELECT PROFILE", "Select", "Back") draws. `wbinvd` at the seeded `0x002A9700` (a cache flush at the top
of a function) is reached ~48 times: a no-op, fine.

## 13. Clipped text: SET_SURFACE_CLIP origin was dropped (toolkit + back end fix)

Symptom: the profile name keyboard drew its keys without letters; the profile panel's labels were missing too.

How found: one frame logged with `RECOMP_D3D11_LOGFRAME` (flip 5000 with vsync on, ~145 flips/s here). Each key
is a 9-slice frame (DXT5 `826C2900`) plus a 2-triangle glyph quad from a glyph atlas (`808A8A00` etc., linear A8,
fmt 0x1F; their alpha holds the glyphs -- the BMP texture dump drops alpha, read byte 3 of each pixel). Glyph quads
had sane positions, texel coordinates on real glyphs and opaque colours, and the same combiner/blend state as text
that did show. The difference was the batch header's surface size: `31x31` for every key label, `112x23`, `450x107`
... while full-screen draws had `640x480`. RSC2 sets `SET_SURFACE_CLIP_HORIZONTAL/VERTICAL` to each label's box
(x, y, w, h). The executor kept clip_x/clip_y but `current_surface()` passed only w/h, so the back end placed the
clip at (0,0): its screen-to-clip scale used `2 / 31` and the viewport covered 0..31, and every label outside the
top-left corner vanished.

Fix:
- toolkit (`patches/xboxrecomp.patch`): `Nv2aSurface` gains `clip_x, clip_y`; `current_surface()` fills them.
- `src/d3d11_backend.c`: a draw or clear with a clip origin is treated as covering the surface up to the clip's
  far edge (vertex positions are whole-surface coordinates), and only the clip rectangle is written: rasterizer
  states now enable scissor and every game draw sets the scissor to the clip rectangle (full surface when no clip);
  colour clears use `ClearView` with that rectangle. The present/post passes use the default rasterizer state and
  are unaffected.
- `RECOMP_D3D11_LOGFRAME` now also prints a vsh batch's input slots and first four vertices.

Verified in frame dumps (`frames\kb6_sheet.png`): every key label, the tab row (Symbols / International / A-Z),
Space / BackSpc / Caps / Done, "Enter Name of Profile", and the profile panel labels (Profile Name, Career Points,
Mileage, Hours Driven, License, Unlocks). Loading screen and movies unchanged.

Also learnt: with vsync on (`run_rsc2.bat`) the movies play at real speed; with `RECOMP_D3D11_VSYNC=0` they run
far faster than real time, so flip numbers and timings differ between the two.

## 14. Car selection: preview shown alone on black (toolkit + back end fix)

User report: on the car selection screen the 3D car was drawn on a black background instead of inside the menu.

**Route** (headless, `RECOMP_PAD_LIVE` file, fresh save dir): Start x8 through the movies (lands on the profile
name keyboard when no profile exists), down x4 + right x4 + A (Done), A (select the profile), A (CAREER), left + A
(AMATEUR), A (first event, Novice Rally Invitational) -> CAR screen. Scripted: `scripts\dev\to_carselect.ps1 <tag>
["ENV=v;ENV2=v"]` (leaves the game running; `scripts\dev\press.ps1 <tag> <buttons...>` presses more and saves the
newest frame as `frames\<tag>_now.png`). The frame log trigger is new: `RECOMP_D3D11_LOGFILE=<path>`
logs the next whole flip when that file appears (no flip number needed); during that flip a draw that samples a
render target also prints the target's colour/alpha pixel counts and dumps the drawn-to surface right after it.

**Frame** (one flip): menu into the display buffer (double-buffered `80028000` / `80280000`, pitch 2560), the car
into `803AC000` (640x380, cleared to 0), then into `8099C000` (a real 1280x440 supersampled surface, pitch 5120),
then b199 draws a quad (0,162)-(640,380) sampling `8099C000` with SRC_ALPHA blending into the menu, then the rest of
the menu on top. Two bugs:

1. **Wrong surface presented.** The executor's `note_drawn()` takes the biggest surface drawn since the last flip
   as the frame (MM3 / Burnout 3 heuristics); `8099C000` is bigger than 640x480, so the car pass was presented.
   Fix (toolkit): a surface whose pitch is the display mode's (AvSetDisplayMode) beats one that is not; area
   (now in logical pixels, `drawn_area`) decides only between equals.
2. **Render target sampled as transparent black.** The draw bound its textures (`PSSetShaderResources`) before
   switching its render target. `8099C000` was still bound as the previous draw's target, so D3D11 silently bound
   NULL in its place. Fix (`d3d11_backend.c`): bind the draw's render target before its textures.

Found by: `[FSTAT]`/`RECOMP_D3D11_TRACE` showing flip surface `8099C000`; then the new alpha stats (`8099C000`
had 490k opaque pixels at composite time) plus the after-draw dump showing b199 changed nothing.

Verified in frame dumps (`frames\car12_sheet.png`): the car rotates inside the CAR menu with its floor reflection;
loading screen, logos, intro movie, profile screens and keyboard, Main, Career and the Amateur chart unchanged.
User-confirmed for the first car (2026-10-29); cycling to the next car froze: see 15.

Seen on the way, not fixed (follow-ups):
- ~~Stray glyph fragments near the bottom of the CAR screen~~: they were the stat bars missing a vtable function;
  fixed in 15.
- ~~Saved profiles fail to load in a later run~~: not the signature; an empty directory-query pattern, fixed in 16.
- ~~Single Race -> Rally -> SS1 -> A crashes deterministically in the game's heap free (`sub_00238F10`, called from
  `0x000AA944`, freeing `[obj+0x48]` = `0x000ADF22`, a code address)~~: fixed by 15, re-run in 17.
- Runs started from this tool are redirected by the Claude app package (`%LOCALAPPDATA%` -> package LocalCache),
  which splits the z: cache between two folders and produced rename collisions; `RECOMP_SAVE_DIR=<repo>\savedata`
  keeps saves local (the Cache folder still goes through AppData). `RECOMP_SAVE_ROOT` (16) now moves the whole
  tree, Cache included.

## 15. Car selection: cycling to the next car froze (seeds + game heap patch)

Pressing right on the CAR screen stopped all flips. Two causes, found in order:

1. **Unseeded vtable entries.** The log showed `[ICALL] Failed to resolve VA 0x001BA230` (25k times since the CAR
   screen opened). `0x001BA230` is `float f(int)` through a jump table, `ret 4`: a failed indirect call pushes no
   float and pops no argument, which unbalances the x87 stack and the guest esp. A scan of `.rdata` for pointers
   into `.text` that sit among known function starts found three real ones: `0x001573D0`, `0x001BA230`,
   `0x001BA2B0`, now in `config/seed_functions.json`. This also fixed the stat bars (Handling / Speed /
   Durability / Accel), which were the "stray glyphs" of 14.
2. **Game heap range check vs. a bottom-up contiguous arena.** Still frozen after that, with the crash in the heap
   free (`sub_00238F10` from `sub_0023C920`). The game's allocator (`0x468F28`) sets up heap A with
   `XPhysicalAlloc(0x20000, -1, 0, 4)` (`sub_0023C340`), and its free tests "pointer in heap A" as
   `base <= p < base + ([+0x6A4] << 10)` with `[+0x6A4]` = 0x20000 *bytes*, i.e. 128 MB. On the console
   XPhysicalAlloc allocates top-down, so heap A is at the top of RAM and nothing later is above it. Our arena is
   bottom-up: heap A got the first block (`0x80001000`) and every later XPhysicalAlloc block passed the test. The
   car's textures (`0x80E96000`, 304 KB, and `0x82C5B000`, 1452 KB, allocated when the car loads) were therefore
   freed as heap blocks on the next car, reading a "header" at `p-8` out of the menu movie's YUY2 frame
   (`0x80E00000`, 600 KB, decoded by Bink every frame) and corrupting the heap. Fix (`patch_gen.py`): drop the
   `<< 10`, so the range is heap A's real 128 KB. Only that function reads `[+0x6A4]`. The kernel arena now sees the
   frees (`frees=0` before).

Found by: a temporary print at `loc_000AA93B` (handle, `[+0x48]`, header dwords around the block), then
`RECOMP_WATCH=0x80E95FFC` (writer = Bink at `0x349249`, texture `80E00000 640x480`), the full arena log and
`RECOMP_WATCH=0x004695CC` (the heap size store). New knob: `RECOMP_CONTIG_LOG=1` logs every contiguous
alloc/free (the default log throttles after 16).

Worth knowing for other titles: any game that classifies pointers by "above my first XPhysicalAlloc" relies on
top-down allocation. A top-down arena in the toolkit would fix them all, but would change the high-water
semantics the pushbuffer executor uses (`xbox_ContiguousAllocatedBytes`), so it stays a per-game patch for now.

Verified in frame dumps (`frames\cyc13_sheet.png`): Hyundai -> Skoda -> Mitsubishi -> Ford -> back to Seat, each
with model or lock text and stat bars; no crash and no unresolved calls. **User-confirmed (2026-10-29).**

## 16. Saved profiles "Unable to load" after a restart (toolkit fix)

A profile worked in the run that created it, but after a restart selecting it gave "Unable to load <name>"
(string 0x6EF) with no file I/O at all. The profile list is built at boot by `sub_000BD820`: for each save from
XFindFirstSaveGame (`0x83ACB`) it sets flags `+0x38` = 0x600 and calls `sub_000BD230`, which lists the save
folder (storage object `[0x423708]` vtable +0x28) and types the save by file extension (`profile`, `replay`,
`ghost`, `cartune`, `xsn`; strings at `0x3B57A4`...). No match -> `or 0x500` -> flags 0x700 = unusable, and the
select screen (`sub_001D8AE0`, `0x1D8BF5`) shows the message without trying to load.

The listing is `NtQueryDirectoryFile` with an **empty** file name (Length 0). `kernel_file.c` (Windows) built
`<dir>\` + "" and `FindFirstFileW` matched nothing, so the folder looked empty. On the console, and in the
toolkit's own POSIX version, an empty name means every entry. Fix: treat Length 0 as `*`. The creation run never
re-lists the folder, which is why a new profile worked until the next launch. Existing saves load with the fix;
nothing in the save files was wrong.

Found by: instrumenting the four sites that load the message IDs (0x6EE/0x6EF/0x773, from
`Locale\English\localestrings.str`: u32 count at 0x20, then {u16 id, u32 char offset} entries), then
`RECOMP_WATCH` on the profile's flags word (`0x01020148` in these runs), then entry prints in the candidate
functions. New knobs: `RECOMP_DIR_LOG=1` logs each directory query (pattern, status, returned name);
`RECOMP_SAVE_ROOT=<dir>` (`src/main.c`) puts the whole save tree, z: cache included, in one folder.

Aside, test environment only: with saves in `%LOCALAPPDATA%` (redirected for processes the Claude app starts) the
second boot froze after a failed `z:\music\credits.wma.cache` rename (0xC0000035), with a second SetDisplayMode
and no more flips (the intermittent intro freeze seen earlier was this too). With `RECOMP_SAVE_ROOT` it does not
happen; the user's runs from `run_rsc2.bat` are not redirected.

Verified in frame dumps (`frames\sav15_sheet.png`): profile created in one run, the next run lists it with its
stats (0/2420 points, license E) and A goes to Main.

## 17. Racing works (no changes needed)

First race attempt, on the build from 16 (ae37c8c). Both routes load a stage and drive; no code change was needed.

- **Career**: Main -> CAREER -> left + A (AMATEUR) -> A (Novice Rally Invitational) -> CAR, A -> race-day info, A
  -> stage menu (QUIT / START RACE / CAR TUNING / OPTIONS), A -> SS1 Cooper Hill I (Australia), solo against the
  clock. Countdown lights, then the race timer runs in real time (2.0 s of race time per 2 s dump interval).
- **Single Race**: Main -> SINGLE RACE (left of CAREER) -> RACE TYPE, right + A (RALLY) -> RALLY TRACK, A (SS1) ->
  RACE SETTINGS, A -> CAR, A -> tips/loading, A (START RACE). The same stage with 3 AI cars, their name tags and
  POS n/4. The heap-free crash of 14 is gone, so 15 fixed it.

Pad, through `pad_live.txt`: `rt` = throttle (1st to 4th gear, 119 km/h on the first straight), `lt` = brake /
reverse (gear R), `lleft`/`lright` steer (the car turned off the road into the bushes). The stage file
`LocationAustralia_R1.fa` sits in `Layer1`; the game probes `d:\Layer0\` first, and the other failed opens in the
log (`d:\Tracks\...`, `d:\Cars\Accent.tpl`, `z:\Audio\*.wbd`) are loose-file probes before the `.fa` lookup. No
`[ICALL]` failures or stubs in either run.

Frames look right: lit terrain, shadows, foliage, billboards, opponent cars, HUD (tacho, speed, gear, split
arrows, progress bar, race time). The car's rear window draws flat black; it may be tinted glass, not checked
against the console. Verified in frame dumps (`frames\r1_sheet.png`, `r1_sheet2.png`, `r1_full79.png`,
`s1_pair.png`); still needs the user to drive in `run_rsc2.bat`.

## 18. Car glass drawn opaque black (toolkit + back end fix)

User report after driving: graphics glitches in the race. The rear window (and every car's glass) drew as an
opaque dark grey-blue sheet with no interior or reflection (17 noted it).

The glass draw (vsh `805E31E7`, 8 combiner stages, SRC_ALPHA blend) samples four textures: t0 a 512x512
**palettised** mask (format `0x0B`, SZ_I8_A8R8G8B8), t1 the DXT1 logo strip, t2 the stage's sky cube map, t3 a
64x64 render target. Decoded (`scratchpad rcdec.py`-style, see the batch log), its alpha is
`v1.a + 2 * t0.a - 1`, with `t0.a = t0.r >= 0.5 ? t0.r : 0.5`; the reflection weight is `lerp(1, v1.a, t1.a)`.
`Nv2aTexture` (the executor -> back end texture) had no palette field and `nv2a_backend_decode_texture` forced
`palette = 0`, so every P8 texture failed to decode (`texture format 0x0B (512x512) not decodable` x8 in the log),
the stage went invalid and the shader read white: t0.a = 1, alpha 1, opaque glass with no cube term.

Fix: `Nv2aTexture.palette` (toolkit `nv2a_backend.h`), filled from the stage's SET_TEXTURE_PALETTE in both
places the executor builds one, and used by the decoder; the back end folds the 1 KB CLUT into the texture's
cache hash (a new palette is a new texture). Now the glass is translucent with the sky reflection, and the
roll cage shows through. No more undecodable textures in a race.

New debug knobs (`d3d11_backend.c`), all used to find this:
- `RECOMP_D3D11_DBGFILE=<file>`: `<mode> [<vsh hash>]`, re-read every 30 flips, so the debug view switches
  while the game runs and only for one vertex programme. Modes: 1 d0, 3 d1, 7 d0.a, 9 d1.a, 10-13 stage
  0-3 colour after the fetch, 14-17 their alpha, 18 t2 as a direction.
- `RECOMP_D3D11_LOGSTEP=<prefix>,<a>-<b>`: in the `RECOMP_D3D11_LOGFILE` flip, the surface after each batch
  a..b; writing `a-b` into the trigger file replaces the range. VSH-on-GPU batches have no CPU vertices, so
  PICK cannot find them; the filmstrip can.
- `RECOMP_D3D11_DROPDUMP=<pct>`: dump a frame whose triangle count falls under pct% of the last one (for
  one-frame vanishing scenery; logs `[DROP]`).
- With `RECOMP_D3D11_TEXDUMP`, cube maps are written too (`cube_<va>_<w>.bmp`, six faces stacked).

Also learnt: the stage's sky cube (`80C0F100`, DXT5 128) decodes right (+Y sky, -Y ground). The four 64x64
targets `80821000 + n*0x4000` are top-down terrain colour renders each frame, sampled as t3 by car and terrain.

Verified in frame dumps (`frames\glass_pair.png`, before g2 / after g6): the player's rear window shows the
roll cage and the sky tint; the AI cars' windows look like glass.

## 19. Race glitches still open (investigated, not fixed)

Found or reported in the same pass as 18; each is written up with what is known.

**AI cars without wheels**: fixed in 20 (visibility tests). The wheels were never drawn, in the flyby either: a
dark backdrop behind the arches there looked like tyres, which sent the search above the wrong way for a while.

**Car-shadow quads with inf texture coordinates.** The shadow quads (vsh `20DD05E9`, inline `DrawPrimitiveUP`
data written by the game's CPU code) carry `-inf/inf` texture coordinates for two of the four cars (`attr range ...
NONFINITE` in the batch log). Not looked at further; it may be an off-screen car's projection and harmless.

**Terrain vanishing for 10-20 frames (user report).** Not reproduced in ~10 race runs. Ruled out: dropped batches
(FRAMESTATS `d` always 0), a sudden triangle-count drop (`RECOMP_D3D11_DROPDUMP=50`: only the loading screen fired),
the async vertex-shader compile window (the CPU fallback draws terrain correctly), visibility queries
(`GET_REPORT` is only used with zpass counting off, as timestamps), the vertex/index ring wraps (DISCARD on wrap is
correct) and the double-buffer present order (strictly alternating). A 200-frame every-flip burst while driving
(`RECOMP_D3D11_BURSTFILE`) showed no vanishing frame. Next: a screenshot or the stage/position from the user, then
a burst there.

Second pass (after 20; user: "a few frames every 10-20 seconds", still there): still not reproduced. A 75 s
every-flip capture of scripted driving at RES 1 (`frames\fl4`, 2234 frames) and the RES 2 ring saves (`flicker\`,
~1100 frames) were scanned for frames that break from both neighbours (8x8 blocks that jump and come back); every
hit was the chase camera passing a tree, a post or a fence, or the car spinning. The per-flip triangle counts
(`[FVSH]`) show the distant-landscape programme `64A6D53E` (512x256 texture plus the sky cube, world-space hills)
dropping for 20-30 flips every 10-15 s, but in the frames that is the camera swinging away from the hills.
Ruled out besides the list above: the PVIDEO overlay (never on in RSC2) and the idle hitch present (needs 250 ms).
Lead from the user's own `run.log`: terrain tile textures (DXT1 128x256 .. 512x1024) are re-decoded up to 6 times
each, i.e. the game streams terrain textures into reused slots while driving; a tile drawn while its slot is being
rewritten would flicker for a few frames. Not confirmed.

Third pass: two user ring saves pinned it down, and it was not the terrain. Far trees, not ground, drop out for a
few flips, and the cause is a rounding bug in 20's visibility-test results. Fixed in 21.

Flight recorder for the user's runs (`d3d11_backend.c`, on in `run_rsc2.bat`): `RECOMP_D3D11_RING=<n>` keeps the last
n presented frames on the GPU (120 at RES 2 is ~590 MB); F12 in the game window writes them to
`flicker\<date_time>\NNN_f<flip>.bmp` with `stats.txt` (batches, triangles, triangles per vertex programme each
flip). `RECOMP_D3D11_RING_AUTO=<pct>` saves by itself when a programme's triangles drop under pct% from one flip to
the next (`RECOMP_D3D11_RING_VSH=<hash>` watches one programme), and the first trigger also fires
`RECOMP_D3D11_LOGFILE`; `RECOMP_D3D11_FRAMEVSH=1` prints each flip's stats line as `[FVSH]`.

**Black band along a road edge.** Seen once (g1, `frames\g1_crop.png`): a uniform near-black strip ~25 px wide
between the road and the grass verge near the first house. Not seen again from other angles; may be a verge
polygon the game shades dark. Not investigated further.

**Performance.** The race runs at about 25-30 flips/s with dumps on (the menus run at 145 with vsync), so race
time advances faster than the dumps suggest. Not a rendering bug; noted for the user's report.

New debug knobs (`d3d11_backend.c`): `RECOMP_D3D11_BURSTFILE=<path>` dumps every flip while that file exists;
`RECOMP_D3D11_LOGCONST=<a>-<b>` adds a vertex programme's constants to the batch log; the batch log now also gives
each attribute slot's largest magnitude and non-finite count (`attr range`, tagged `NONFINITE`).

## 20. AI cars drawn without wheels (toolkit + back end fix)

In the race every AI car's wheel arches were empty, showing the ground through them; the player's wheels drew.
RSC2 culls each AI wheel with a visibility test. Per wheel it draws an 89-triangle proxy (vsh `9A7BEA7C`, colour
mask 0, depth write off) between `SET_ZPASS_PIXEL_COUNT_ENABLE` 1 and 0, reads the count with `GET_REPORT` and
draws the wheel only if pixels passed. The executor counted zpass pixels only in its own software rasteriser,
which does not run when a back end draws, so every report said 0 and no AI wheel was ever drawn (the flyby too).
The player car's wheels are not tested.

Found by: a flyby log and a countdown log of one run (same cars, same texture addresses) showed every car batch
submitted in both, then a LOGSTEP pixel trace showing the flyby's "tyre" pixel was only the dark backdrop (b56).
The untextured, colour-masked per-wheel proxies were the giveaway. A test build that reported any draw as visible
brought the wheels back, but also showed the sun flare when the sun was behind trees, so it was not kept.

Fix: `Nv2aBackend.zpass_begin` / `zpass_end` (toolkit `nv2a_backend.h`, optional). The executor opens a back end
query when counting turns on (and after a CLEAR_REPORT_VALUE while on), closes it when counting turns off or a
GET_REPORT reads while on, and adds the result to `zpass_count`. The D3D11 back end runs a
`D3D11_QUERY_OCCLUSION` and waits for it in end (the title polls the report right away), scaling samples by
`RECOMP_D3D11_RES`^2 to guest pixels. ~~No visible cost: race flip rate unchanged in dumped runs.~~ Wrong: the wait
at every test cost a third of the frame rate and caused steady stutter; fixed in 22.

Verified in frame dumps (`frames\wheels_pair.png`; before g9, after w4): the AI cars have wheels with rims in the
flyby and at the start line; the sunset flyby shots show no flare with the sun behind the trees.

## 21. Far trees vanish for a few frames at RES 2 (back end fix)

User report (19's "terrain vanishing"), now with two F12 ring saves from `run_rsc2.bat` (RES 2):
`flicker\20261029_143947` frames 083-086 lose the small trees behind the ridge (left of centre, just above the
horizon), and 087 has them back. `stats.txt` shows the same thing: programme `6B1E852A` draws ~470 triangles on flips 079-082,
~348 on 083-086 and ~480 again from 087. Nothing else changes on those flips, so the title itself skipped those
draws. `20261029_143918` 142-149 is the same kind of event with far hills on a desert stage.

Cause: RSC2 runs ~28 visibility tests every race flip (the sphere proxy `9A7BEA7C` from 20, also in a solo race,
so world objects are tested, not only AI wheels). `be_zpass_end` scaled the D3D11 occlusion count to guest pixels
with `n / RES^2`. At RES 2 a test that passes 1-3 samples (a sliver of a proxy peeking over a ridge) became
**0 pixels**, so the title culled an object that was on screen. While driving these come in runs of consecutive
flips (e.g. 14408-14411), which is the "a few frames, then back" pattern. It cannot happen at RES 1, which is
why 19's RES 1 captures never showed it.

Fix (`d3d11_backend.c`): round up, `(n + RES^2 - 1) / RES^2`, so any passing sample counts as one guest pixel.
`RECOMP_D3D11_ZPASS_FLOOR=1` restores the old flooring. `RECOMP_D3D11_ZPASS_LOG=1` prints each test with 1 to RES^2-1
samples (`[ZQ] flip N: k samples -> 1 pixel`).

Found by: the ring's per-programme stats around the user's frames, then `RECOMP_D3D11_ZPASS_LOG` at RES 2 (z1: 11
floored tests in ~20 s of driving on SS1), then a temporary print of the first draw after each test. In an
old-rounding run (zb) the tree programme also dropped ~100 triangles two flips after a floored test (flip 14409).
Not proven at the user's exact spot: scripted driving does not reach it. The fixed build (z2) logged the same
kind of tests as `-> 1 pixel`, and its frames are unchanged otherwise. Needs the user to confirm in
`run_rsc2.bat`.

**User (2026-10-29): still happens**, so rounding was not the cause, or not all of it; the rounding fix stays (it is
correct either way). In the trees sample, `6B1E852A` keeps its draw count (19, 18, 19) but draws fewer triangles,
which looks more like a lower level of detail than a culled object. RSC2 may lower detail when frames run slow,
and the user's runs had steady 36-48 ms frames (22). Not tested: printing per-flip stats (`FRAMEVSH`) slows the
run to ~32 flips/s either way, so a slow/fast comparison needs a cheaper probe. The other sample
(`20261029_143918` 142-149) is not a dropout: the car pitches downhill and the far hills slide up behind the
HUD's semi-transparent red progress bar.

Third sample, after 22 (`flicker\20261029_155125`, user-circled): the dropped trees are **distant** trees at
the end of the road, gone in frames 38-51 and back at 52, while the car sits reversing with wheelspin (0 km/h,
nearly static camera). `6B1E852A` draws ~126 fewer triangles on exactly those frames. What is known so far:
- `6B1E852A` is tree foliage: per tree a trunk draw (`9A1B30D6`), then a foliage draw of camera-facing cards
  (2 triangles each) from a 512x1024 atlas of 8x8 views (`81A64800`). Far trees get one card; near trees
  40-80 triangles. The cards are written inline into the pushbuffer every frame (no vertex buffer), so a missing
  tree is the title not asking for it.
- Ruled out: the back end dropping batches (every foliage batch it receives is drawn), contiguous-memory
  allocations (none during the race), frame time (no slow frames near the drop), the host timer resolution
  (`timeBeginPeriod(1)`: same dropout rate), the one-CPU gate (`RECOMP_DISPATCH_GATE=0`: same), the TSC
  (already scaled to 733 MHz), and visibility tests: RSC2's main-view tests are all pairs of flare-type proxies
  (`9A7BEA7C` 89/48/49/7/8 triangles, `938821B6`, `B278CC79`), none around trees. The `9A7BEA7C` batches in the
  512x512 target are a depth-only (shadow) pass, not tests.
- A dust cloud from the wheelspin (`306EDB61`, growing from 512 to 1056 triangles through the sample) hangs over
  the same spot; it does not write depth. Not shown to be related.
- Scripted driving has not reproduced a dropout on a straight; the drop detector also fires on flyby camera
  cuts, so earlier "every run has a 6-flip dropout" counts were flyby cuts.
Found in 23: the game's own size cull of distant tree groups.

## 22. Race frame rate: visibility tests no longer wait for the GPU (toolkit + back end fix)

User report: the frame rate regressed. It came from 20, not 21: `be_zpass_end` spun on `GetData` at every test,
and RSC2 runs ~28 tests per race flip, so the GPU queue was drained ~28 times a frame and the CPU and GPU never
overlapped. The launcher's hitch log (`RECOMP_D3D11_HITCH`) showed it as steady 36-48 ms frames with no texture
or shader work in them.

Measured on a fixed scripted 48 s drive on SS1 (RES 2, 240-frame ring, vsync on; flips/s from the dump file names,
`[HITCH]` = frames over 33 ms):

| build | flips/s | frames > 33 ms |
|---|---|---|
| 21, rounding floored (`RECOMP_D3D11_ZPASS_FLOOR=1`) | 39.1 | 171 |
| 21 | 42.7 | 140 |
| test hack: every test "visible", no wait | 162.0 | 0 |
| 22 | 62.9 | 1 |

Fix: optional `Nv2aBackend.zq_begin / zq_end / zq_poll` (toolkit `nv2a_backend.h`). The D3D11 back end gives
each test its own occlusion query from a pool of 256 and polls without blocking. The executor collects a report's
queries, and at GET_REPORT writes the slot only if nothing is pending. Otherwise it leaves the slot as D3D set it
(incomplete) and writes timestamp, count and then the done word once every query has answered, polled from later
reports and from the ack loop's idle hook (`nv2a_backend_idle`, which flushes). As on the console, the title sees a
test incomplete for a while. A second GET_REPORT without a CLEAR first waits for the earlier report, because the
count is a running total. A CLEAR hands its open queries to an empty report so their ids are freed. A full pool or
report list falls back to waiting. `RECOMP_ZPASS_SYNC=1` restores the old wait at every test.

The rest of the gap to 162 flips/s is probably the title waiting on some results itself (it polls the slot).

Verified in frame dumps: the SS1 drive (pd) looks the same as before and race time now keeps pace with the dumps;
the Single Race start line (w1, `frames\w1_now.png`) has all three AI cars with their wheels (the tests 20 is for).

## 23. Distant trees popping out: the game's own group cull (found; draw-distance knob)

The trees in the user's third sample (21) are a whole group of distant trees that the game stops submitting.
Traced from the draw back to the decision:

- Foliage draws come from `sub_00114C80` (cards written inline into the pushbuffer by `sub_001101E0`). It walks a
  per-frame list (`[queue+0x2974]` entries of 24 bytes at `+0x2978`: object, byte, fade at +0x14; fade 1.0 draws,
  > 0 draws faded, <= 0 not at all) that `sub_00273890` copies out of the render queue (type-1 items). Every queued
  tree had fade 1.0: missing trees are missing from the queue.
- The queue is filled by the render traversal `sub_00273E70`, whose tree candidates come from the frustum cull
  `sub_002B0E00` -> `sub_002B0D10` (main thread only, one cull context, view 0 = the screen with factor 1.0,
  views 4-9 = cube-map faces with factor 4.0 on alternate frames). It walks a quadtree with per-node, per-view
  plane-coherency state (3 bits at `node+0x10`; test variants `0x225470`..`0x226190`, exact) and, for a node
  fully inside the frustum, `sub_002AF9B0` drops the **whole subtree** when its projected size
  `radius * cot(fov/2) / distance` is at most `[0x49C89C] * [0x49C8D8]` = 1.0 * 0.019 (0.02 at init by
  `sub_0010D640`, then the track's value, stored by the loading thread). About a 9-pixel radius at 480 lines:
  a distant group at that size drops out and comes back as the camera moves, here the wheelspin shake.
- Checked and ruled out on the way: the visibility tests (flares only), the dust (no depth writes), x87 `fptan`
  (pushes 1.0), the inherited-flags scan (no hits on this path), the 1.0/4.0 factor ever being the cube map's for
  the screen (one frame per run sees a zone value of ~1.27 from `sub_0029C5F0`; not the cause), threads.

So this is the console's behaviour, not an emulation fault, as far as the code shows. Not proven at the user's
exact spot.

Knob (`patch_gen.py` + `recomp_tree_lod_scale()` in `src/recomp_manual.c`): `RECOMP_TREE_DETAIL=<n>` divides the
threshold where the culls read it (the five group culls and the per-tree cull in `sub_0010F0D0`), so groups stay
until they are n times smaller. Default 1 = console. With 4 on the scripted SS1 drive: 1079 tree objects drawn
over the run instead of 835, queue 193 instead of 175, 58 flips/s instead of 65. `patch_gen.py` entries can now give
an expected count for identical sites (`(anchor, replacement, n)`).

How found: a RAM scan for the foliage vertices (they are in the pushbuffer), `RECOMP_WATCH` on the pushbuffer and
then on a render-queue bucket's end pointer for the call chains, temporary prints in the generated code (queue,
candidates, cull results, view factors). New toolkit knob: `RECOMP_WATCH_AFTER=<s>` arms `RECOMP_WATCH` that many
seconds after start (a watched heap page that is busy during boot otherwise crawls).

**User (2026-10-29): still drops at 4x** (`flicker\20261029_192355` 66-92): the distant band of trees at the end
of the road goes when the car rolls back a few metres and returns when it rolls forward. That is the trees'
**far plane**, not the size test: the cull frustum (`sub_00273E70` at `0x2741D0`) ends at (0.2 + 1) * the zone's
view distance (`[0x3DF758]` = 0.2, `[view + 0x24268]` from the zone record), about 525-533 units on SS1 (logged
frustum planes). The foliage fog cannot hide the cut: linear fog to 900 units (params 2, -1/900), but the
foliage programme writes `oFog = min(depth, c[4].x)` with `c[4].x` = 765, so trees are at most 85% fogged, and
about 40% at 530. Same code and data as the console, so the console shows the same pop.

`RECOMP_TREE_DETAIL` now also multiplies that far distance (fourth `patch_gen.py` entry): 4 gives a tree frustum
about 2060 units deep (logged), 58.7 flips/s on the scripted drive; the countdown frame shows the tree line along
the road running on to the vanishing point (`frames\sn` vs `frames\f4`, f00051). Launcher help text updated.

**User (2026-10-29): still drops at 4x** with this build (`flicker\20261029_210912` 80-232). Not a cull: see 25.

## 24. Window took ~20 s to open: XNet's DHCP wait (patch_gen fix)

User report: from the launcher, the game window appeared only ~20 s after Run. Timed headless (launch to
`MainWindowHandle`): 24.6 s. The D3D device came up at 20.5 s; before it, the log showed only a polling loop.
A temporary print in XAPI `SleepEx` (`sub_00074ED1`) found 202 `Sleep(100)` calls, all from `0x207542`:

- `sub_002074E0` (network init) calls `XNetStartup` (`sub_00310058`), `WSAStartup(0x101)` (`sub_0031006F`), then
  loops `Sleep(100)` until the check `sub_002071F0` (vtable `0x39F6FC` slot 3) is true.
- `sub_002071F0` calls `XNetGetTitleXnAddr` (`sub_0030FD2A`) into a local `XNADDR` and is true when the status,
  minus `GATEWAY` (0x20), is non-zero, i.e. once it is no longer `XNET_GET_XNADDR_PENDING` (0).
- With no NIC, XNet's DHCP keeps it pending until it gives up at ~21 s and answers `0x8002` (ETHERNET |
  TROUBLESHOOT), logged.

Fix (`patch_gen.py`): in `sub_002071F0` a pending status reads as `0x8002`, the answer XNet gives 20 s later. The
`XNADDR` is a local nobody reads, and XNet itself is untouched (its DHCP still times out in the background).
Window now at 4.3 s cold, 1.2 s warm. The scripted Career -> SS1 drive still races. Its flips/s were not
comparable between runs: 45.9 with the fix, 30.1 without it, back to back. The drives diverged, so those numbers
say nothing about the fix.

Also: `patch_gen.py` skips an entry whose replacement is already present, so a second run over the same generated
files is safe. Before this, the far-plane entry (23), whose replacement contains its anchor, was applied twice.

## 25. Distant trees at 4x: the model -> card switch; render-queue overruns (patch_gen fix)

User sample at 4x (`flicker\20261029_210912`, Career -> Pro -> SS1 near the start, bonnet camera, reversing):
between flips 3166 and 3168 one `6B1E852A` batch of 124 triangles (a tree model of ~62 cards) goes and nothing
takes its place; the trees at the end of the road open onto the buildings behind.

Traced with temporary prints over a scripted drive at the same place (Pro SS1 start, forward then reverse):
- At 4x every cull keeps the tree: quadtree node and box tests, the group size cull, the per-tree size cull.
- `sub_0010F0D0` builds the per-frame tree list (24-byte entries: object, distance, ..., alpha byte at +0x12,
  fade at +0x14), in two loops (`0x10F1D0` near list, `0x10F6D0` main list). The fade crossfades model and
  card by projected size: 1.0 = card only (`sub_00114C80` queues it at `+0x1F14` for `sub_001101E0`),
  below 1 = the model drawn by `sub_00112530` (plus the card at alpha `255 * sin(fade * pi / 2)`). The size
  thresholds (`[type + 0x2F8] * [0x4947D0 + 0x80E0] * [+0x80D0]`, times `[+0x80DC]` for the other end) and the
  model's own size window (`[+0x8114]..[+0x8110]` in `sub_00114390`, `sub_00114860`, `sub_00114C80`) were not
  scaled by `RECOMP_TREE_DETAIL`. While reversing, each drop in the scripted drive was a model whose fade
  reached 1.0.
- The alpha byte also carries a distance fade, from `[cam + 0x14] * [+0x812C]` (339 on SS1) to `[cam + 0x14]`
  (424, the zone's view distance), beyond which the entry is removed. Unscaled too, so trees still end at
  ~424 units whatever the far plane (23) allows.
- Forcing every queued object to its card (test toggle) turns buildings, cars and barriers into cards as well,
  and those cards do render; why the card left nothing at the user's spot was not found.

Two env-gated test switches in a test build, tried by the user at their spot: scaling the distance fade by the
knob did **not** help; scaling the model -> card switch did. **User-confirmed (2026-10-29).** (The fade test
was not valid: the switch went into the launcher's extra arguments, not its environment. See 26.)

Fix (`patch_gen.py`): the model -> card thresholds above are multiplied by `recomp_tree_lod_scale()`, so tree
models last n times further. The distance fade is left as the console has it.

Also found: plain 4x (23's far plane) crashed a scripted drive in the start flyby at ~2280 draws per flip
(null ICALLs, then an access violation in `sub_00273360`). The render queue (`sub_00273E70`) keeps 24-byte items
in fixed arrays after their counts: 1536 at `+0x12254`, 1024 at `+0x1B258`, 256 at `+0x2125C` and `+0x22A60`
(the last followed by the zone's view distance at `+0x24268`). The tree list holds 3104 (`+0x10`) and
`sub_00273890`'s copy of it 2048 (`+0x2978`). The console never fills them; the knob can. Every insert is now
clamped (an overflow rewrites the last slot). With the clamps, 4x drives finished without a crash.
Final build, scripted 48 s drive: 4x 26.3 flips/s, 1x 28.5, both without a crash. The machine was slow that
evening (1x was ~65 earlier), so these numbers don't compare with 23's.

## 26. Trees at 4x, cheaper: the distance fade instead of longer models (patch_gen fix)

User (2026-10-30): 25 works, but the frame rate fell to 35-40 fps. Fixed benchmark (`bench.ps1`: Pro SS1 start
line for 10 s, then a short drive; RES 2, vsync off; flips/s at the line / driving; draws per flip):

| build | flips/s | draws |
|---|---|---|
| console (1x) | 56 / 64 | 807 |
| 4x, models at the console distance | 51 / 55 | 974 |
| 4x, models 2x | 43 / 47 | 1113 |
| 4x, models 4x (25) | 33 / 34 | 1348 |
| 4x, models at the console distance + distance fade 4x (26) | 41 / 47 | 1240 |
| same, fade 2x | 46 / 52 | 1067 |
| 26 as committed (defaults), 4x | 41 / 45 | 1205 |

RES 1 is no faster than RES 2, so the cost is on the CPU side, about 28 us per extra model draw. The game thread
spends ~80% of a frame in `sub_003683A0` (D3D spinning on a GPU status register until the executor is idle).
(Later found: the scripted runs' window is hidden, and Windows throttles presenting there, so vsync-on numbers
from them mean nothing; see 28.) With vsync on, frame times sit at ~25 ms or ~44 ms for hundreds of flips with the same scene, so single runs
mislead.

The cause of the blank card in 25 is most likely the distance fade: from `0.8 * [cam + 0x14]` (339 on SS1) to
`[cam + 0x14]` (424) a tree's alpha byte drops to 0. Full models ignore that alpha and cards do not, so a distant
model that turned into a card came back faint or invisible. Retested by the user with the switch in the launcher's
environment this time: models at the console distance plus the fade 4x keeps the trees. **User-confirmed
(2026-10-30)** (frame rate about the same with vsync on: frames still over 16.7 ms).

Fix (`patch_gen.py`, `src/recomp_manual.c`): `recomp_tree_fade_scale()` (RECOMP_TREE_FADE, default the knob)
multiplies both reads of `[cam + 0x14]` in `sub_0010F0D0`. `recomp_tree_model_scale()` (RECOMP_TREE_MODEL) now
defaults to 1, the console's model distance. Launcher help text back to "draws them further".

## 27. Frame rate: texture lookups hashed once per flip (back end fix)

Timed inside a frame at tree detail 4 (bench as in 26, RES 2, vsync off; temporary timers, since removed):
~20-25 ms per flip, of which `be_draw_ext` 7.5 ms, and 4.7 ms of that in `get_texture` (~2500 binds a flip,
~1.9 us each). Each bind searched the ~600-entry texture cache linearly and hashed up to 2048 scattered words of
the texture's guest memory to catch CPU writes.

Fix (`d3d11_backend.c`): a 4096-slot index finds the cache entry (checked, falls back to the search), and a
texture's bytes are hashed once per flip (`checked_flip`); later binds in the same flip reuse the entry. A
texture the CPU rewrites mid-frame shows from the next frame. P8 textures (CLUT can change between draws) are
still hashed at every bind; `RECOMP_D3D11_TEXCHECK=draw` restores the old check for all.

| build (bench, vsync off) | 4x flips/s | 1x flips/s |
|---|---|---|
| 26 | 41 / 45 | 56 / 64 |
| 27 | 51 / 57 | 69 / 76 |

`get_texture` is now ~1 ms a flip. (A "4x with vsync on: 51 / 56" first written here was a vsync-off run:
the bench's environment variables carried over between runs in one shell. Fixed in the bench; see 28.)

Where the rest goes at 4x (~17 ms): the executor's vertex gather in `raster_batch_vsh_gpu` (~7 ms: ~320,000
vertices a frame read from guest memory and written out as float4 attributes; reading each attribute's range
once per batch instead of per vertex changed nothing, so it is the data volume), the back end flush (~4 ms) and
the draw (~3.7 ms). The game thread spends ~80% of a frame in `sub_003683A0`, D3D spinning on a GPU status
register until the executor is idle. Next step would be caching converted static geometry across frames. The
game-thread sampler (`RECOMP_SAMPLE`) names toolkit functions unreliably (folded/static symbols).

## 28. Frame rate: converted geometry kept across frames (toolkit + back end)

The rest of 27's 4x frame was the executor's gather (~7 ms: ~320,000 vertices a frame read from guest memory,
deduplicated and written out as float4 attributes) and its upload (part of the ~4 ms flush). Almost all of it is
static geometry, the same bytes every frame.

Toolkit (`nv2a_pb_exec.c`, `nv2a_backend.h`): `raster_batch_vsh_gpu` computes `Nv2aVshBatch.geom_key`, a 64-bit hash
of the primitive, the index list, each array attribute's layout and every guest byte the gather reads for it (the
span between the lowest and highest index). Hashing reads the same bytes the gather did, but sequentially and
without converting or writing anything. No key (0) for inline arrays, attributes without address or stride, spans
outside guest RAM or much larger than the vertices they hold, or `RECOMP_VSH_GEOM_CACHE=0`. New optional hook
`Nv2aBackend.vsh_geom_cached(key, &nverts, &nindices)`: when the back end holds the key, the executor skips the gather
and sends the batch with `attrs`/`indices` NULL.

Back end (`d3d11_backend.c`): a key gets its own immutable attribute buffer (+ SRV) and index buffer the second flip
it is drawn with data, so geometry the title rewrites every frame (new bytes, new key) never does. Up to 16384 keys,
384 MB; entries unused for 300 flips, and keys seen only once for over two flips, go when either fills. The
periodic `RECOMP_D3D11_TRACE` line shows the count and size. Debug dumps that read `attrs` skip held batches.
Frames compared with the cache off: same picture (start line and driving).

Bench (Pro SS1 start line + short drive, RES 2, vsync off; the bench now clears RECOMP_* between runs):

| build | 4x flips/s | 1x flips/s |
|---|---|---|
| 27 | 51 / 57 | 69 / 76 |
| 28 | 74 / 79 | 98 / 100 |

4x: 12.2 ms a frame, ~5700 keys held, 44 MB. Vsync-on numbers from the scripted runs are not usable (their window
is hidden; presenting there is throttled to ~25 fps whatever the frame time). Needs the user's check in the
launcher with vsync on.
User check: 4x holds 60 fps or more with vsync on, in the launcher.

## 29. Trees at 4x dropped again: models kept 2x further by default (recomp_manual.c)

With 28 in, the user saw the dark band of trees at the end of the road (Pro SS1, near the start) vanish again at 4x,
driving forward (`flicker/20261030_022327`, frames 116-219: foliage vertex program 6B1E852A 762 -> 634 triangles).
Not the geometry cache (`RECOMP_VSH_GEOM_CACHE=0` drops too) and not 27 (`RECOMP_D3D11_TEXCHECK=draw` drops too).

Probed run (bumper camera, the user's spot): every drop caught was one tree model (~124 foliage triangles) switching
to its card in `sub_0010F0D0` (fade 1.0, size ~0.150, card alpha byte 255; e.g. objects 013EC650 and 013EF670), with
no cull removing the tree. The card that replaces it does not show. 26's longer distance fade only hid this by
luck. Why those cards are empty is still open; the per-model LOD flags (+0x2B8/+0x2C0) are 0 for all types, so it
is not a LOD pick.

Fix: `recomp_tree_model_scale()` now defaults to the square root of RECOMP_TREE_DETAIL (models 2x further at 4x,
1.41x at 2x, console at 1x); `RECOMP_TREE_MODEL=<n>` still overrides. Cost at 4x (bench as in 28, vsync off):

| model distance | 4x flips/s | draws |
|---|---|---|
| console | 76 / 82 | 1219 |
| 2x (new default) | 67 / 73 | 1348 |
| 4x (25) | 52 / 55 | 1797 |

User check with `RECOMP_TREE_MODEL=2`: the trees stay and it holds 60 fps or more.

## Next

1. Profile creation with real input: **user-confirmed working (2026-10-28)**. Car selection (14, 15):
   **user-confirmed (2026-10-29)**. Racing (17): verified in frame dumps, needs the user to drive.
2. Gamepad and keyboard in the live window need the user (`rsc2_launcher.exe`; RECOMP_KEYBOARD: Enter = Start,
   Z/X = A/B, arrows = D-pad).
3. Audio check with `RECOMP_AUDIO_WAV` (custom DSP images, NOTES 3).
4. XNet "no network" handling once the Live / System Link screens are reached. In RSC2's D3D no code
uses 0x8700/0x8704 as an immediate, so 5849's overlay code is shaped differently: find it from the watchdog when
it hangs. `0x36F64E` initialises the PVIDEO registers (0x8900..0x8930 block) and is a good starting point.
