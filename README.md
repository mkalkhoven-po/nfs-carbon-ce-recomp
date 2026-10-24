# Need for Speed Carbon – Xbox 360 recompilatie

Static recompilation van NFS Carbon (Xbox 360, US, title ID `454107EC`, v0.0.0.7)
met de [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

> Game-bestanden (de ISO, `game/`) en alles wat eruit gegenereerd wordt
> (`nfsc/generated/default/`) zijn auteursrechtelijk beschermd: nooit committen of delen.

## Mappen

| Map | Inhoud |
|---|---|
| `*.iso` | Eigen dump van de disc (XGD2) |
| `game/` | Volledig uitgepakte disc: `default.xex`, `NFS/ZZDATA*.BIN`, `Movies/` |
| `tools/` | `xdvdfs.py` (disc uitpakken), `xexinfo.py` (XEX-header dumpen), `setup-sdk.sh` |
| `sdk/rexglue-sdk/` | ReXGlue SDK v0.10.0 als git-submodule (upstream, commit `c94f5eb`) |
| `patches/rexglue-sdk/` | Onze aanpassingen op de SDK, toegepast door `setup-sdk.sh` |
| `nfsc/` | Het recompilatie-project |
| `nfsc/config/functions.toml` | Handmatige functiegrenzen voor `default.xex` |

## Toolchain (Windows)

- LLVM/Clang 23 (`winget install LLVM.LLVM`)
- VS 2022 Build Tools met de workload *Desktop development with C++* (MSVC 14.44, Windows SDK 10.0.26100)
- CMake 4.x, Ninja, Git, Python 3.12

Zet `C:\Program Files\LLVM\bin` vooraan in `PATH` voordat je bouwt.

## Opnieuw opbouwen

```bash
# 1. Disc uitpakken
python tools/xdvdfs.py extract "Need for Speed - Carbon - Collector's Edition.iso" game

# 2. SDK ophalen, patchen en bouwen (eenmalig). setup-sdk.sh haalt de submodules op,
#    vervangt git-symlinks (Windows checkt die uit als tekstbestand) en past patches toe.
bash tools/setup-sdk.sh
cd sdk/rexglue-sdk
cmake --preset win-amd64 -DREXGLUE_USE_VULKAN=ON   # Vulkan staat op Windows standaard uit
cmake --build --preset win-amd64-release --target install
cmake --build --preset win-amd64-debug --target install

# 3. Project bouwen (codegen draait automatisch mee)
cd ../../nfsc
cmake --preset win-amd64-debug
cmake --build --preset win-amd64-debug
```

## Lokale aanpassingen aan de SDK

Staan als patch in `patches/rexglue-sdk/` (de submodule wijst naar ongewijzigd upstream).

- `0003-kernel-log-swap-rate-setting.patch` (`src/kernel/xboxkrnl/xboxkrnl_video.cpp`):
  cvar `log_swap_rate=N` logt elke N seconden de framerate (swaps/s). Handig voor metingen.
- `0002-graphics-guest-vblank-rate-setting.patch` (`src/graphics/graphics_system.cpp`):
  nieuwe cvar `guest_vblank_rate` (Hz, 0 = refresh rate van de video mode). Carbon
  presenteert op elke vblank en rekent met de klok, dus dit is de frame-cap.
- `0001-gapfill-claim-code-after-bctr-tail-calls.patch` (`src/codegen/phase_gapfill.cpp`): GapFill miste code direct na functies die eindigen
  op een indirecte tail call (`bctr`, veel gebruikt voor virtuele calls); de eerste
  crash (call naar `0x824DA838`) kwam daardoor. Aanpassingen:
  - over bekende functies in een segment heen stappen (alleen ontdekte blocks tellen)
    en de rest van het segment alsnog als gap-functie claimen;
  - GapFill herhalen tot er niets nieuws bijkomt (convergeert na ~21 rondes);
  - in latere rondes alleen nieuwe gap-functies (en wat ze overlappen) opschonen,
    zodat dit snel blijft (~85 s codegen);
  - ongebruikte slots in de import-thunktabel overslaan.
  Resultaat: 73.202 functies, geen onopgeloste calls of `REX_FATAL`s.

## Spelen

```bash
cd nfsc/out/build/win-amd64-release   # of win-amd64-debug
./nfsc.exe --game_data_root="E:/Need for Speed Carbon - Recomp/game"
```

De build zet de GPU-plugin (`rexgpu-xenos*.dll`) zelf naast `nfsc.exe`; de app gebruikt
standaard de Vulkan-backend (`--gpu_backend=d3d12` voor D3D12).

### Grafische profielen

`--profile=original` (standaard) of `--profile=remix`, of `profile = "remix"` in
`nfsc.toml` naast de exe. Instellingen die je zelf zet (toml, commandoregel, F4-menu)
gaan altijd voor op het profiel.

| Instelling | original | remix |
|---|---|---|
| EDRAM-emulatie (`render_target_path_vulkan`) | `fsi` (interlock, nauwkeurig) | `fsi` |
| Interne resolutie (`draw_resolution_scale_x/y`) | 1 (1280×720) | 2 (2560×1440) |
| Anisotrope filtering (`anisotropic_override`) | -1 (wat de game vraagt) | 5 (16×) |
| Native 2× MSAA | aan | aan |
| Framerate (`guest_vblank_rate`) | 30 fps (zoals de 360) | 60 fps |

GPU-belasting in het hoofdmenu (RTX 5080): ~36% bij 1×, ~53% bij 2×, ~90% bij 3×.
Voor een 4K-scherm kan `draw_resolution_scale_x/y = 3`, maar dat laat weinig marge.

Vulkan-presentatiemodus *immediate* geeft op NVIDIA een zwart beeld; de profielen zetten
`vulkan_allow_present_mode_immediate = false` (mailbox/FIFO werken). Smart App Control moet uit staan,
anders blokkeert Windows elke nieuw gebouwde `.exe`.

De gegenereerde game-code wordt ook in Debug met `-O2` gecompileerd: op `-O0` haalden de
video's maar ~11 fps in plaats van 30 en liepen ze steeds verder achter op de audio.

## Status

24-10-2026: start op, speelt de logo's, intro-video's en de openingscinematic af en komt via
het titelscherm in het **hoofdmenu** (3D-garage rendert correct). Navigeren werkt met controller
en toetsenbord (o.a. MY CARS openen en terug). Stabiel en zonder fouten in de log. "Cannot
connect to Xbox Live!" bij het opstarten is verwacht. Career/races nog niet getest.

Besturing: een Xbox-controller werkt direct. Toetsenbord met `--mnk_mode=true`:
Start = `Return`, A = `Space`, B = `Backspace`, linker stick = `WASD`.
Savegames komen in `Documents
fsc`.

Opgelost: audio liep uit sync met de video's in de Debug-build (video-decodering te traag
op `-O0`; gemeten 11 vs. 30 fps, audio steeds exact 187,5 frames/s = 48 kHz / 256).

Codegen los draaien:
`sdk/rexglue-sdk/out/install/win-amd64/bin/rexglue.exe codegen nfsc/nfsc_manifest.toml`
