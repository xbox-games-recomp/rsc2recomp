# RalliSport Challenge 2 static recompilation -- engineering notes

Scaffolded 2026-10-28 from [mm3recomp](https://github.com/taxidriverhk/mm3recomp) (Midtown Madness 3; same
studio, DICE). Items are numbered like mm3recomp's NOTES.md ("MM3 #n" refers to that file).

Layout: toolkit = submodule `xboxrecomp\` pinned to v0.15.0 (6376eb8) with mm3recomp's
`patches\xboxrecomp.patch` (game-agnostic fixes) applied by `apply_patches.ps1`. Game files:
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
(AMATEUR), A (first event, Novice Rally Invitational) -> CAR screen. Scripted: `scripts\to_carselect.ps1 <tag>
["ENV=v;ENV2=v"]` (leaves the game running; `scripts\press.ps1 <tag> <buttons...>` presses more and saves the
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
`RECOMP_D3D11_RES`^2 to guest pixels. No visible cost: race flip rate unchanged in dumped runs.

Verified in frame dumps (`frames\wheels_pair.png`; before g9, after w4): the AI cars have wheels with rims in the
flyby and at the start line; the sunset flyby shots show no flare with the sun behind the trees.

## Next

1. Profile creation with real input: **user-confirmed working (2026-10-28)**. Car selection (14, 15):
   **user-confirmed (2026-10-29)**. Racing (17): verified in frame dumps, needs the user to drive.
2. Gamepad and keyboard in the live window need the user (`run_rsc2.bat`; RECOMP_KEYBOARD: Enter = Start,
   Z/X = A/B, arrows = D-pad).
3. Audio check with `RECOMP_AUDIO_WAV` (custom DSP images, NOTES 3).
4. XNet "no network" handling once the Live / System Link screens are reached. In RSC2's D3D no code
uses 0x8700/0x8704 as an immediate, so 5849's overlay code is shaped differently: find it from the watchdog when
it hangs. `0x36F64E` initialises the PVIDEO registers (0x8900..0x8930 block) and is a good starting point.
