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
cmake --preset win-amd64
cmake --build --preset win-amd64-release --target install
cmake --build --preset win-amd64-debug --target install

# 3. Project bouwen (codegen draait automatisch mee)
cd ../../nfsc
cmake --preset win-amd64-debug
cmake --build --preset win-amd64-debug
```

## Lokale aanpassingen aan de SDK

Staan als patch in `patches/rexglue-sdk/` (de submodule wijst naar ongewijzigd upstream).

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
cd nfsc/out/build/win-amd64-debug
./nfsc.exe --game_data_root="E:/Need for Speed Carbon - Recomp/game" --gpu_plugin=xenos
```

`rexgpu-xenosd.dll` (Debug) of `rexgpu-xenos.dll` (Release) moet naast `nfsc.exe` staan;
kopieer hem uit `sdk/rexglue-sdk/out/install/win-amd64/bin/`. Smart App Control moet uit
staan, anders blokkeert Windows elke nieuw gebouwde `.exe`.

## Status

24-10-2026: start op, speelt de logo's, intro-video's en de openingscinematic af zonder
crash. Menu, besturing en in-game rendering nog niet getest.

Bekende problemen:
- Audio loopt flink uit sync met de video's (geluid zelf klinkt goed).

Codegen los draaien:
`sdk/rexglue-sdk/out/install/win-amd64/bin/rexglue.exe codegen nfsc/nfsc_manifest.toml`
