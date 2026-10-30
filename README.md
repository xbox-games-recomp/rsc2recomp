# RalliSport Challenge 2 — Static Recompilation

A native Windows (x64) port of the original-Xbox *RalliSport Challenge 2* (Digital Illusions / Microsoft, 2004).
Every x86 function in the retail `default.xbe` is translated to C, compiled with MSVC and linked against a
replacement Xbox kernel, audio, input and GPU runtime; the NV2A command stream is rendered with Direct3D 11.
No emulator, interpreter or JIT.

Built on [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) v0.15.0 (MIT, git submodule) plus
`patches/xboxrecomp.patch`, scaffolded from [mm3recomp](https://github.com/xbox-games-recomp/mm3recomp).

> **No game content is in this repository.** You need your own copy of the game. Everything derived from it
> (generated C, builds, dumps) is git-ignored.

**Status: early, playable.** Intro movies, menus, profiles (saved and reloaded), car selection and full stages
(Career and Single Race) with gamepad and keyboard; 60 fps or more with vsync at 2× internal resolution,
resizable 4:3 window, optional longer tree draw distance.

## Quick start

1. Download `rsc2recomp-<version>-windows.zip` from [Releases](https://github.com/xbox-games-recomp/rsc2recomp/releases)
   and unzip it to a short path such as `C:\Games`.
2. Run `rsc2_launcher.exe`, choose your disc image or extracted disc folder, press **Run setup**, then **Play**.

Setup installs the build tools with winget if needed and builds the game (about 20 minutes). The launcher's
[player guide](https://github.com/xbox-games-recomp/xbox-recomp-launcher#readme) covers each step and
troubleshooting.

## Game version

The overrides name guest addresses, so only this executable works:

| | |
|---|---|
| Title ID | `0x4D530039` (XBE 2004-04-02, XDK 5849) |
| `default.xbe` SHA-1 | `45ee3898ac1c6336f241cc8502a7bbd46673fe71` |

Extract the whole disc into `game_files\`, e.g. `python scripts\xiso_extract.py "RalliSport Challenge 2.iso" game_files`
(`extract-xiso -x` works too). The image is a plain XDVDFS partition.

## Building from source

Needs Windows 10/11, Visual Studio 2022 (C++ workload), CMake 3.20+, Python 3.10+ with `pip install capstone`,
and a Direct3D 11 GPU.

```bash
git clone --recursive https://github.com/xbox-games-recomp/rsc2recomp.git
```

```powershell
& .\scripts\regen_once.ps1             # patch toolkit, lift x86 to C, build (15–20 min, prints BUILD_EXIT=0)
cmake --build build --config Release   # code-only rebuild (~1 min)
```

The output is `build\Release\rsc2_recomp.exe`; `rsc2_launcher.exe` is copied to the repository root.

## Running

`rsc2_launcher.exe` sets resolution, FXAA, tree draw distance, vsync, keyboard, rumble and logging, saves them to
`launcher.user.ini` and logs to `run.log`; `--play` skips the window. The required environment variables are
in `launcher.ini`. Useful extras for the *Extra environment* box:

| Variable | Effect |
|---|---|
| `RECOMP_D3D11_RES=<n>` | Internal resolution n × 640×480 (default 2, max 6), averaged down to the window |
| `RECOMP_D3D11_SCALE=<f>` | Initial window size f × 640×480 (default 1.5) |
| `RECOMP_D3D11_STRETCH=1` | Fill the window instead of keeping 4:3 |
| `RECOMP_D3D11_HITCH=<ms>` | Log every frame slower than this |
| `RECOMP_TREE_DETAIL=<n>` | Tree draw distance, n × the console's (the launcher's 1/2/4 choice) |

**Controls:** the first XInput pad maps 1:1 to the Xbox pad. Keyboard: WASD steer, IJKL right stick, arrows
D-pad, Z/X/C/V = A/B/X/Y, Q/E = White/Black, 1/3 = brake/gas, Enter = Start, Backspace = Back. In a stage,
Black cycles the camera and Back resets the car. F12 saves the last few seconds of frames to `flicker\`
(flight recorder, for bug reports).

## How it works

```
default.xbe ─► disasm ─► func_id ─► abi_analysis ─► recomp ─► patch_gen.py ─► MSVC ─► rsc2_recomp.exe
               (capstone + seeds in config/seed_functions.json)      (x86 → C)         + xboxrecomp runtime
```

- **xboxrecomp** supplies the Xbox kernel, an emulated MCPX APU (→ XAudio2), XInput and the NV2A pushbuffer
  executor.
- **`src/recomp_manual.c`** replaces guest functions that depend on hardware or the XDK (GPU fence, XAPI
  fibers and gamepad calls) and holds the tree draw-distance knobs.
- **`scripts/patch_gen.py`** applies anchored fixes to the generated C (game heap layout, XNet's DHCP wait,
  tree culling and render-queue limits).
- **`src/d3d11_backend.c`** translates NV2A vertex programs to HLSL, runs the register combiners in one
  data-driven pixel shader, and handles render-to-texture, supersampling, occlusion queries, Bink movies and
  a cache of static geometry.

`NOTES.md` is the engineering log (every bug, cause and fix, plus debug recipes). Read it before changing
anything.

## Open issues

1. Audio has not been checked against the console (the game uses its own APU DSP images).
2. Some distant tree cards draw nothing where their model was; hidden by keeping models further out (NOTES 29).
3. A reported terrain dropout was not reproduced; shadow quads with invalid UVs (NOTES 19).
4. No Xbox Live / System Link.

## Legal

An independent, non-commercial preservation project, not affiliated with Microsoft, Xbox Game Studios or
Digital Illusions CE. *RalliSport Challenge* is the property of its owners. Do not open issues or pull
requests containing game files or generated code. The toolkit in `xboxrecomp/` is MIT-licensed.
