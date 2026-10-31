# Need for Speed: Carbon Collector's Edition – Recompiled

A native PC port of **Need for Speed: Carbon Collector's Edition for the Xbox 360**, made by
static recompilation with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk). The
game's PowerPC code is translated to C++ ahead of time and compiled for x86-64; graphics run
on Vulkan (or Direct3D 12).

**Version 0.751 – public test build.** The game is playable from start to finish as far as we
know, but only a part of the Career has been tested. Please report what you find (see
[Reporting problems](#reporting-problems)).

> **No game files are included.** You need your own copy of the game: a disc image (`.iso`) of
> the **Collector's Edition** (title ID `454107EC`, `default.xex` version 0.0.0.7) or the
> files extracted from it. Other editions (the regular release, other regions) have a
> different executable and will not run; the game checks this at startup.

This project is not affiliated with or endorsed by Electronic Arts, Microsoft or Xbox. All
trademarks belong to their respective owners. It does not promote piracy: use a dump of a disc
you own.

## Features

- Runs natively, no emulator: 30 fps like the Xbox 360, or 60/120/240 fps with the race clock,
  physics and AI still in real time.
- Two graphics profiles:
  - **Original** (default): 1280×720, the console's 30 fps, accurate EDRAM emulation,
    nothing added. Meant to look exactly like the Xbox 360.
  - **Remix**: 2560×1440 internal resolution, 16× anisotropic filtering, AMD CAS
    sharpening, 120 fps.
- Widescreen and ultrawide (21:9, 32:9 or any ratio wider than 16:9): more view to the
  sides, HUD at the screen edges, videos and loading screens kept at 16:9.
- Controller and keyboard at the same time.
- "PC SETTINGS" and "EXIT TO DESKTOP" in the main menu.

## Requirements

- Windows 10/11, 64-bit.
- A GPU with Vulkan 1.3 (or Direct3D 12). Tested on an NVIDIA RTX 5080; Original needs
  little, Remix at 120 fps wants a recent mid-range card or better.
- The [Microsoft Visual C++ Redistributable 2015–2022 (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).
- Your Collector's Edition disc image (about 7.3 GB).

## Installing and starting

1. Download the release zip and extract it anywhere.
2. Start `nfsc.exe`. Choose the Remix profile and other options later under PC SETTINGS
   in the main menu.
3. On the first start a file dialog asks for the game. Choose either
   - the disc image (`.iso`), or
   - `default.xex` in a folder with the extracted disc.

   The choice is remembered (in `Documents\nfsc\game_path.txt`; delete that file to choose
   again). Instead of using the dialog you can also put the `.iso` or a folder named `game`
   next to `nfsc.exe`, or start with `--game_data_root="path to the iso or folder"`.

"Cannot connect to Xbox Live!" at startup is expected. Savegames are stored in
`Documents\nfsc`.

## Controls

An Xbox controller and the keyboard work at the same time.

| Keyboard | |
|---|---|
| `W` `A` `S` `D` | Steer / navigate |
| `E` / `Q` | Throttle / brake |
| `Space` / `Backspace` | A / B (accept / back) |
| `L` / `P` | X / Y |
| `Return` / `Tab` | Start / Back |
| `F11` | Fullscreen ↔ window |
| `F1` | PC settings (also in the main menu) |
| `F4` | All settings, for advanced use |
| `F3` | Performance overlay |
| `Alt+F4` | Quit (or EXIT TO DESKTOP in the main menu) |

## Settings

The main ones are in **PC SETTINGS** (main menu or `F1`): graphics profile, frame rate limit,
aspect ratio, fullscreen or window, and under Advanced the resolution, sharpening, texture
filtering and graphics API. Use a controller (A select, B back), the keyboard (WASD or arrows,
`Space`, `Backspace`) or the mouse. "Profile (...)" means the graphics profile's value.
**Apply** saves to `nfsc.toml` next to `nfsc.exe`; settings marked with `*` restart the game.

Every setting can also be set in the advanced settings window (`F4`), in `nfsc.toml`, or on the
command line (`--name=value`). Settings you change yourself always win over the profile.

| Setting | Values | |
|---|---|---|
| `profile` | `original`, `remix` | Graphics profile (restart) |
| `aspect_ratio` | `16:9`, `21:9`, `32:9`, `W:H` | Render aspect ratio (restart) |
| `guest_vblank_rate` | `30`, `60`, `120`, `240` | Frame rate cap (changes right away) |
| `draw_resolution_scale_x` / `_y` | `1`, `2`, `3` | Internal resolution: 720p, 1440p, 4K |
| `fullscreen` | `true`, `false` | |
| `gpu_backend` | `vulkan`, `d3d12` | (restart) |

Example `nfsc.toml` for an ultrawide monitor:

```toml
profile = "remix"
aspect_ratio = "21:9"
```

## Known issues

- Only tested on NVIDIA (RTX 5080, Windows 11). AMD and Intel GPUs have not been tested.
- Career: tested up to the first races, the Career hub, the world map, the safehouse and free
  roam; the rest of the story (territories, crew, canyon duels) has not been played through.
- Split screen has not been tested (needs a second controller).
- The Vulkan "immediate" present mode shows a black screen on NVIDIA; the profiles turn it
  off.
- Smart App Control can block the executable; it is not signed.

## Reporting problems

Open an issue on GitHub with what you did, what happened, your GPU and the profile and
settings you used. The log files are in the `logs` folder next to `nfsc.exe`; attach the one
from the session where it happened.

**Privacy:** nothing is ever sent anywhere. The game only writes log files on your own PC;
you decide whether to share one. `--log_fps=true` adds the frame rate to the log once a second.

## Building from source

The repository contains only our own code: the project (`nfsc/`), the hooks for widescreen,
frame rate and the menu option (`nfsc/src/`), and our changes to the ReXGlue SDK
(`patches/rexglue-sdk/`). The recompiled game code is generated from your own `default.xex`
during the build.

Toolchain (Windows): LLVM/Clang 23, Visual Studio 2022 Build Tools (*Desktop development
with C++*), CMake 4, Ninja, Git, Python 3.12 and the LunarG Vulkan SDK. Put
`C:\Program Files\LLVM\bin` first in `PATH`.

```bash
# 1. Extract the disc
python tools/xdvdfs.py extract "path/to/your/disc.iso" game

# 2. Get, patch and build the SDK (once)
bash tools/setup-sdk.sh
cd sdk/rexglue-sdk
cmake --preset win-amd64 -DREXGLUE_USE_VULKAN=ON -DREXGLUE_ENABLE_FIDELITYFX=ON -DREXGLUE_FIDELITYFX_BACKEND=vk
python ../../tools/fix-ffx-rc.py
cmake --build --preset win-amd64-release --target install

# 3. Build the game (the recompilation runs as part of the build)
cd ../../nfsc
cmake --preset win-amd64-release
cmake --build --preset win-amd64-release
```

`tools/package-release.ps1` puts a build together as a release folder.

Development notes (in Dutch): [docs/DEVELOPMENT.nl.md](docs/DEVELOPMENT.nl.md) and
[ROADMAP.md](ROADMAP.md).

## License

Our code is licensed under the BSD 3-Clause License (see [LICENSE](LICENSE)), like the ReXGlue
SDK. Release builds include third-party libraries under their own licenses (see the
`licenses` folder in the release). The game itself and everything generated from it remain
the property of Electronic Arts.
