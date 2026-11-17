# v0.76 – public test build

Fourth public test build of the Need for Speed: Carbon Collector's Edition (Xbox 360)
recompilation. Requires your own disc image or extracted disc of the **Collector's Edition**
(title ID 454107EC, version 0.0.0.7); see the README.

## New in this version

- **Newer engine underneath.** The game now runs on version 0.11 of the ReXGlue SDK, with many
  fixes in the recompiler and the emulated Xbox 360 system.
- **Faster.** In our measurement the main menu runs about 13% faster than in v0.752 (Remix,
  RTX 5080). Races have not been measured yet.
- Setting names now have a scope, such as `nfsc.profile` or `ui.fullscreen` (see the README).
  Your existing `nfsc.toml` and command lines keep working: the old names are still understood.

## Changed

- Remix no longer sharpens the picture with AMD CAS: the new SDK version doesn't include it.
  The 2x internal resolution keeps the picture sharp.
- A new frame rate limit takes effect after a restart (PC SETTINGS restarts the game for you).
- `F4` opens the new SDK's own settings window and `` ` `` a console, both for advanced use.
  Saving in the `F4` window rewrites `nfsc.toml` completely (comments are lost) and also stores
  the values of your graphics profile; PC SETTINGS (`F1`) only changes what you change.

## Upgrading

Extract into a new folder, or over the old one: the files `rexruntime.dll`, `rexgpu-xenos.dll`
and `amd_fidelityfx_vk.dll` of earlier versions are no longer used and can be deleted. Your
savegames and settings stay where they were. The shader cache of earlier versions doesn't work
with the new SDK; the game clears it once by itself and builds it up again while you play,
which can stutter briefly at first.

## Not tested yet

- The rest of the Career story (territories, crew, canyon duels).
- Split screen, and a real controller in PC SETTINGS (tested with the keyboard).
- AMD and Intel GPUs, and Direct3D 12.

## Reporting problems

Open an issue with what happened, your GPU, your monitor's refresh rate, the profile and
settings, and the log file (and dump file, after a crash) from the `logs` folder next to
`nfsc.exe`. Nothing is sent anywhere automatically.
