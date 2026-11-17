# v0.76 – public test build

Fourth public test build of the Need for Speed: Carbon Collector's Edition (Xbox 360)
recompilation. Requires your own disc image or extracted disc of the **Collector's Edition**
(title ID 454107EC, version 0.0.0.7); see the README.

## New in this version

- **Newer engine underneath.** The game now runs on version 0.11 of the ReXGlue SDK, with many
  fixes in the recompiler and the emulated Xbox 360 system.
- **Faster.** In our measurements the main menu runs about 13% faster than in v0.752 (Remix,
  RTX 5080).
- **Movies and loading screens on ultrawide** stay 16:9 with black bars at the sides instead of
  being stretched; the game itself still fills the whole width.
- **PC SETTINGS** reacts to the first key press right away; before, the first press only showed
  where you were.
- Settings now have a scope in their name, such as `nfsc.profile` or `ui.fullscreen` (see the
  README). Your existing `nfsc.toml` keeps working: the old names are still understood.

## Changed

- Remix no longer sharpens the picture with AMD CAS: the new SDK version doesn't include it.
  The 2x internal resolution keeps the picture sharp.
- A new frame rate limit takes effect after a restart (PC SETTINGS restarts the game for you).
- `F4` opens the SDK's own settings window; `` ` `` opens a console. Both are for advanced use.

## Upgrading

Extract over the old folder or into a new one. Your savegames and settings stay where they were.
The shader cache is built up again the first time you play, which can stutter briefly.

## Not tested yet

- The rest of the Career story (territories, crew, canyon duels).
- Split screen, and a real controller in PC SETTINGS (tested with the keyboard).
- AMD and Intel GPUs, and Direct3D 12.

## Reporting problems

Open an issue with what happened, your GPU, your monitor's refresh rate, the profile and
settings, and the log file (and dump file, after a crash) from the `logs` folder next to
`nfsc.exe`. Nothing is sent anywhere automatically.
