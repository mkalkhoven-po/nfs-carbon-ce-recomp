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
| `tools/re/` | Reverse-engineering: `strings.py`, `xrefs.py` (lis/addi-verwijzingen), `disasm.py` (disassembly uit de gegenereerde code) |
| `sdk/rexglue-sdk/` | ReXGlue SDK v0.10.0 als git-submodule (upstream, commit `c94f5eb`) |
| `patches/rexglue-sdk/` | Onze aanpassingen op de SDK, toegepast door `setup-sdk.sh` |
| `nfsc/` | Het recompilatie-project |
| `nfsc/config/functions.toml` | Handmatige functiegrenzen voor `default.xex` |

## Toolchain (Windows)

- LLVM/Clang 23 (`winget install LLVM.LLVM`)
- VS 2022 Build Tools met de workload *Desktop development with C++* (MSVC 14.44, Windows SDK 10.0.26100)
- CMake 4.x, Ninja, Git, Python 3.12
- LunarG Vulkan SDK (`winget install KhronosGroup.VulkanSDK`, vraagt admin), nodig voor
  FidelityFX (CAS) op Vulkan

Zet `C:\Program Files\LLVM\bin` vooraan in `PATH` voordat je bouwt.

## Opnieuw opbouwen

```bash
# 1. Disc uitpakken
python tools/xdvdfs.py extract "Need for Speed - Carbon - Collector's Edition.iso" game

# 2. SDK ophalen, patchen en bouwen (eenmalig). setup-sdk.sh haalt de submodules op,
#    vervangt git-symlinks (Windows checkt die uit als tekstbestand) en past patches toe.
bash tools/setup-sdk.sh
cd sdk/rexglue-sdk
# Vulkan staat op Windows standaard uit; FidelityFX levert CAS (Remix). VULKAN_SDK moet gezet zijn.
cmake --preset win-amd64 -DREXGLUE_USE_VULKAN=ON -DREXGLUE_ENABLE_FIDELITYFX=ON -DREXGLUE_FIDELITYFX_BACKEND=vk
python ../../tools/fix-ffx-rc.py   # FidelityFX-resourcebestand is UTF-16; llvm-rc wil UTF-8
cmake --build --preset win-amd64-release --target install
cmake --build --preset win-amd64-debug --target install

# 3. Project bouwen (codegen draait automatisch mee)
cd ../../nfsc
cmake --preset win-amd64-debug
cmake --build --preset win-amd64-debug
```

## Lokale aanpassingen aan de SDK

Staan als patch in `patches/rexglue-sdk/` (de submodule wijst naar ongewijzigd upstream).

- `0007-shared-memory-hot-pages.patch` (`shared_memory`, Vulkan): pagina's die de game elk frame
  opnieuw schrijft (3 frames op rij geüpload) worden niet meer tegen schrijven beschermd, maar
  bij elke aanvraag vergeleken met een schaduwkopie van de laatste upload; alleen gewijzigde
  pagina's gaan opnieuw naar de GPU. Scheelt een `VirtualProtect` per upload en een
  access violation per CPU-schrijfactie. Bewaakte (texture cache) en door de GPU beschreven
  pagina's worden nooit hot; na 120 frames zonder wijziging weer normaal. Menu +8%, samen met
  `clear_memory_page_state=false` (nu in beide profielen) +19%. Uit met
  `shared_memory_hot_pages=false`.
- `0006-vulkan-hoist-shared-memory-uploads.patch` (Vulkan command processor, deferred command
  buffer, shared memory): de game schrijft vertexdata vlak voor elke draw; de backend uploadde die
  per draw en brak daarvoor elke keer de render pass af (~1000 keer per raceframe). Nu gaan
  uploads tijdens een render pass naar een plek vóór die render pass, zolang geen draw in die
  pass de pagina's al gelezen heeft (anders de oude route). ~95% van de uploads gaat zo; GPU in
  de race van ~93% naar ~50%. Uit te zetten met `vulkan_hoist_shared_memory_uploads=false`.
- `0005-present-aspect-override.patch` (`xboxkrnl_video`): `SetPresentAspectOverride()`
  laat de app hetzelfde doen voor ander 16:9-materiaal (Carbon: de laadschermen).
- `0004-present-movie-aspect-setting.patch` (`xfile`, `xboxkrnl_video`, command processors):
  telt open `.wmv`-bestanden; zolang er een video speelt presenteert de runtime in
  `present_movie_aspect` (bv. `16:9`) in plaats van de verhouding van de videomodus.
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
| EDRAM-emulatie (`render_target_path_vulkan`) | `fsi` (interlock, nauwkeurig) | `fbo` (host render targets, snel) |
| Interne resolutie (`draw_resolution_scale_x/y`) | 1 (1280×720) | 2 (2560×1440) |
| Anisotrope filtering (`anisotropic_override`) | -1 (wat de game vraagt) | 5 (16×) |
| Native 2× MSAA | aan | aan |
| Presentatie (`present_effect`) | `bilinear` | `cas` (AMD CAS-verscherping) |
| Framerate (`guest_vblank_rate`) | 30 fps (zoals de 360) | 120 fps |

GPU-belasting (RTX 5080): Origineel ~23% in het menu (30 fps). Remix haalt in races op 120
gemiddeld ~105–110 fps (minimum ~60–80) bij ~45–50% GPU, het menu 120 (gemeten met
SDK-patches 0006 en 0007; daarvoor ~68% GPU op 60 fps). In zij-aan-zij
vergelijkingen (menu, race-start) was `fbo` niet van `fsi` te onderscheiden; hoge snelheid
met bewegingseffecten is nog niet vergeleken. Wie toch het nauwkeurige pad wil in Remix:
`render_target_path_vulkan = "fsi"`. Voor een 4K-scherm kan `draw_resolution_scale_x/y = 3`.

### Hogere framerate

`--guest_vblank_rate=120` (of 240) verhoogt de cap; de simulatie rekent met de klok, dus
race-klok en physics blijven realtime. Op een RTX 5080 haalt een race op 120 gemiddeld ~117
fps (minimum ~80–90), op 240 ~125 (menu ~175), bij ~50% GPU; de grens is de emulatie van de
GPU-commando's op de CPU. `--log_fps=true` logt elke seconde de fps en het traagste frame.
Eerder is op 240 en hoger een paar keer een GPU-fout opgetreden in de race (NVIDIA Xid 13, zie
ROADMAP); sinds SDK-patch 0006 niet meer gezien (5 runs op 240, ~17 op 1000).

### Ultrawide

`--aspect_ratio=21:9` (of `32:9`, of een eigen `B:H`; standaard `16:9`) laat de game in die
verhouding renderen: 3D-beeld (meer zicht opzij, niet uitgerekt) en HUD aan de randen.
Video's en laadschermen blijven 16:9 (op een breed scherm met zwarte balken). Op een 16:9-scherm zie je het
beeld in letterbox. Rendert op 720 × B/H breed (21:9 = 1680, 32:9 = 2560, maximum); de
hoofdscène gaat dan van 4× naar 2× MSAA om in de EDRAM te passen. Smaller dan 16:9 kan niet.
Remix (2×) met 21:9 haalt in races op 120 gemiddeld ~117 fps (minimum ~78), RTX 5080.

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

### Besturing en sneltoetsen

Een Xbox-controller en het toetsenbord werken tegelijk (toetsenbord staat standaard aan via
`mnk_mode`). Toetsenbord: Start = `Return`, A = `Space`, B = `Backspace`, back = `Tab`,
sturen = `WASD`, gas = `E`, rem = `Q`.

| Toets | Functie |
|---|---|
| `Alt+F4` | Afsluiten (of **EXIT TO DESKTOP**, laatste optie in het hoofdmenu) |
| `F11` | Wisselen tussen schermvullend en venster (met sluitknop) |
| `F3` | Debug-overlay |
| `F4` | Instellingen (alle cvars, opslaan naar `nfsc.toml`) |
| `` ` `` | Console |

Savegames komen in `Documents\nfsc`.

Opgelost: audio liep uit sync met de video's in de Debug-build (video-decodering te traag
op `-O0`; gemeten 11 vs. 30 fps, audio steeds exact 187,5 frames/s = 48 kHz / 256).

Codegen los draaien:
`sdk/rexglue-sdk/out/install/win-amd64/bin/rexglue.exe codegen nfsc/nfsc_manifest.toml`
