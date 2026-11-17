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
| `sdk/rexglue-sdk/` | ReXGlue SDK v0.11.0 als git-submodule (upstream, tag `v0.11.0`, commit `a3e1d82`) |
| `patches/rexglue-sdk/` | Onze aanpassingen op de SDK, toegepast door `setup-sdk.sh` |
| `nfsc/` | Het recompilatie-project |
| `nfsc/config/functions.toml` | Handmatige functiegrenzen voor `default.xex` |

## Toolchain (Windows)

- LLVM/Clang 23 (`winget install LLVM.LLVM`)
- VS 2022 Build Tools met de workload *Desktop development with C++* (MSVC 14.44, Windows SDK 10.0.26100)
- CMake 4.x, Ninja, Git, Python 3.12
- LunarG Vulkan SDK 1.4.313 of nieuwer (`winget install KhronosGroup.VulkanSDK`, vraagt admin),
  met de component *Vulkan Memory Allocator header*: SDK v0.11 haalt Vulkan-headers en VMA uit
  de Vulkan SDK

Zet `C:\Program Files\LLVM\bin` vooraan in `PATH` voordat je bouwt.

## Opnieuw opbouwen

```bash
# 1. Disc uitpakken
python tools/xdvdfs.py extract "Need for Speed - Carbon - Collector's Edition.iso" game

# 2. SDK ophalen, patchen en bouwen (eenmalig). setup-sdk.sh haalt de submodules op,
#    vervangt git-symlinks (Windows checkt die uit als tekstbestand) en past patches toe.
bash tools/setup-sdk.sh
cd sdk/rexglue-sdk
cmake --preset win-amd64 -DREXGLUE_BUILD_TESTS=OFF   # Ninja Multi-Config
cmake --build out/build/win-amd64 --config Release --target install

# 3. Project bouwen (codegen draait automatisch mee). De eerste build stopt na codegen,
#    omdat de gegenereerde bronnen pas bij de volgende configure meedoen: beide nog eens.
cd ../../nfsc
cmake --preset win-amd64
cmake --build --preset win-amd64-release
```

## Lokale aanpassingen aan de SDK

Staan als patch in `patches/rexglue-sdk/` (de submodule wijst naar ongewijzigd upstream).
Overgezet naar SDK v0.11.0 op 17-11; van de tien patches voor v0.10 zijn er drie over.

- `0001-filesystem-disc-image-game-root.patch` (`file_system`, `title_app`): `title.game_root`
  mag ook een disc-image (`.iso`) zijn; dat wordt dan als `kDiscImage` gemount in plaats van
  als map. Opvolger van v0.10-patch 0009.
- `0002-vulkan-hoist-uploads-no-draw-in-transfer-pass.patch` (Vulkan command processor,
  deferred command buffer, shared memory): de game schrijft vertexdata vlak voor elke draw; de
  backend uploadt die per draw en breekt daarvoor elke keer de render pass af. Nu gaan uploads
  tijdens een render pass naar een plek vóór die render pass, zolang geen draw in die pass de
  pagina's al gelezen heeft (anders de oude route). Uit te zetten met
  `gpu.vulkan.hoist_shared_memory_uploads = false`. Hoofdmenu (Remix, limiet 240): 151 fps
  zonder, 188–196 met (v0.752 op SDK v0.10 met alle patches: 167). Bevat ook de fix uit
  upstream-PR rexglue/rexglue-sdk#535: met dynamic rendering kon een draw doorlopen in een
  ownership-transfer-pass naar hetzelfde render target. Opvolger van 0006 en 0008; 0006 is
  open upstream-PR #536.
- `0003-gpu-present-aspect-setting.patch` (command processors): instelling
  `gpu.xenos.present_aspect` (bv. `16:9`, nooit opgeslagen) presenteert het beeld in die
  verhouding in plaats van die van de videomodus. De app zet hem zolang een video
  (`FEAnyMovie.fng`) of laadscherm (`Loading*.fng`) actief is, want die tekent de game
  uitgerekt over de bredere render. Opvolger van 0004/0005, die video's in de kernel
  herkenden aan open `.wmv`-bestanden.

Vervallen bij de overstap:

- 0001 (codegen: code na `bctr`-tail calls claimen): upstream opgelost (`a86428b04`, uit onze
  PR #554); codegen vindt nu alle functies zonder handmatige lijst.
- 0002 (`guest_vblank_rate`): de vblank-timer volgt nu `ui.video_mode_refresh_rate` (24–240,
  na herstart). `nfsc.frame_rate_limit` zet die.
- 0003 (`log_swap_rate`): niet meer nodig; `nfsc.log_fps` logt de fps.
- 0007 (hot pages): overgezet, maar op v0.11 zakte het menu naar ~30 fps met frames van 550 ms.
  Weggelaten; oorzaak niet onderzocht.
- 0010 (mutex in `InputSystem`): het invoersysteem van v0.11 heeft eigen locks.
- AMD FidelityFX (CAS, FSR) zit niet meer in de SDK; Remix presenteert nu bilineair.

## Spelen

```bash
cd nfsc/out/build/win-amd64/Release
./nfsc.exe --game_data_root="E:/Need for Speed Carbon - Recomp/game"
```

De build zet alle SDK-modules (`rex*.dll`, `SDL3.dll`) zelf naast `nfsc.exe`. De SDK kiest op
Windows standaard Direct3D 12; beide profielen zetten `gpu.driver = "vulkan"`.

### Grafische profielen

`--nfsc.profile=original` (standaard) of `--nfsc.profile=remix`, of `profile = "remix"` onder
`[nfsc]` in `nfsc.toml` naast de exe (de oude naam `profile` zonder scope werkt ook). Een
profiel schrijft zijn waarden in de titellaag van de SDK; wat je zelf zet (toml, commandoregel,
F1, F4) gaat altijd voor.

| Instelling | original | remix |
|---|---|---|
| EDRAM-emulatie (`gpu.vulkan.render_target_path`) | `fsi` (interlock, nauwkeurig) | `fbo` (host render targets, snel) |
| Interne resolutie (`gpu.resolution_scale_x/y`) | 1 (1280×720) | 2 of 3, naar de monitor |
| Anisotrope filtering (`gpu.anisotropic_override`) | -1 (wat de game vraagt) | 5 (16×) |
| Native 2× MSAA | aan | aan |
| Framerate (`nfsc.frame_rate_limit`) | 30 fps (zoals de 360) | `monitor` |
| Intro overslaan (`nfsc.skip_intro`) | uit | aan |

In zij-aan-zij vergelijkingen (menu, race-start) was `fbo` niet van `fsi` te onderscheiden; hoge
snelheid met bewegingseffecten is nog niet vergeleken. Wie toch het nauwkeurige pad wil in
Remix: `gpu.vulkan.render_target_path = "fsi"` (onder `[gpu.vulkan]`: `render_target_path`).

### Hogere framerate

`--nfsc.frame_rate_limit=120` (of 240) verhoogt de cap via de refresh rate die de game ziet
(`ui.video_mode_refresh_rate`, na herstart); de simulatie rekent met de klok, dus
race-klok en physics blijven realtime. Op een RTX 5080 haalt een race op 120 gemiddeld ~117
fps (minimum ~80–90), op 240 ~125 (menu ~175), bij ~50% GPU; de grens is de emulatie van de
GPU-commando's op de CPU (metingen op SDK v0.10). `--nfsc.log_fps=true` logt elke seconde de fps
en het traagste frame.
Eerder is op 240 en hoger een paar keer een GPU-fout opgetreden in de race (NVIDIA Xid 13, zie
ROADMAP); sinds SDK-patch 0006 niet meer gezien (5 runs op 240, ~17 op 1000).

### Ultrawide

`--nfsc.aspect_ratio=21:9` (of `32:9`, of een eigen `B:H`; standaard `auto`) laat de game in die
verhouding renderen: 3D-beeld (meer zicht opzij, niet uitgerekt) en HUD aan de randen.
Video's en laadschermen blijven 16:9 (op een breed scherm met zwarte balken). Op een 16:9-scherm zie je het
beeld in letterbox. Rendert op 720 × B/H breed (21:9 = 1680, 32:9 = 2560, maximum); de
hoofdscène gaat dan van 4× naar 2× MSAA om in de EDRAM te passen. Smaller dan 16:9 kan niet.
Remix (2×) met 21:9 haalt in races op 120 gemiddeld ~117 fps (minimum ~78), RTX 5080.

Vulkan-presentatiemodus *immediate* geeft op NVIDIA een zwart beeld; de profielen zetten
`gpu.vulkan.allow_present_mode_immediate = false` (mailbox/FIFO werken). Smart App Control moet uit staan,
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
`hid.mnk_mode`). Toetsenbord: Start = `Return`, A = `Space`, B = `Backspace`, back = `Tab`,
sturen = `WASD`, gas = `E`, rem = `Q`.

| Toets | Functie |
|---|---|
| `Alt+F4` | Afsluiten (of **EXIT TO DESKTOP**, laatste optie in het hoofdmenu) |
| `F11` | Wisselen tussen schermvullend en venster (met sluitknop) |
| `F1` | PC SETTINGS (`ui.bind.pc_settings`) |
| `F3` | Debug-overlay |
| `F4` | Alle instellingen van de SDK. Let op: *Save to config* daar schrijft `nfsc.toml` helemaal opnieuw, zonder commentaar, en legt ook de waarden van het profiel vast |
| `` ` `` | Console |

Savegames komen in `Documents\nfsc`.

Opgelost: audio liep uit sync met de video's in de Debug-build (video-decodering te traag
op `-O0`; gemeten 11 vs. 30 fps, audio steeds exact 187,5 frames/s = 48 kHz / 256).

Codegen los draaien:
`sdk/rexglue-sdk/out/install/win-amd64/bin/rexglue.exe codegen nfsc/nfsc_manifest.toml`
