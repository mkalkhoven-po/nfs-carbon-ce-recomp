# v0.751 – public test build

Second public test build of the Need for Speed: Carbon Collector's Edition (Xbox 360)
recompilation. Requires your own disc image or extracted disc of the **Collector's Edition**
(title ID 454107EC, version 0.0.0.7); see the README.

## New in this version

- **PC SETTINGS** in the main menu (or `F1` anywhere): graphics profile (Original or Remix),
  frame rate limit, aspect ratio, fullscreen or window, and under Advanced the resolution,
  sharpening, texture filtering and graphics API. Works with a controller, the keyboard and the
  mouse. **Apply** saves to `nfsc.toml`; settings that need a restart restart the game by
  themselves.
- The `Play (Remix).bat` shortcut is gone: choose Remix in PC SETTINGS instead. Settings you
  already have in `nfsc.toml` keep working.

## What works

- Boot, logos, intro videos, menus, Quick Race (circuit, sprint, canyon), Career up to the first
  races, the Career hub, world map, safehouse, customization and free roam.
- Graphics profiles Original (720p, 30 fps, accurate EDRAM emulation) and Remix (1440p, 16× AF,
  CAS sharpening, 120 fps).
- 60/120/240 fps with real-time race clock, physics and AI.
- Widescreen and ultrawide (21:9, 32:9, custom ratios).
- Start from a disc image (.iso) or an extracted folder; the game asks for it on the first start
  and checks that it is the Collector's Edition.
- EXIT TO DESKTOP in the main menu.

## Not tested yet

- The rest of the Career story (territories, crew, canyon duels).
- Split screen.
- AMD and Intel GPUs.
- PC SETTINGS with a real controller (tested with the keyboard).

## Reporting problems

Open an issue with what happened, your GPU, the profile and settings, and the log file from
the `logs` folder next to `nfsc.exe`. Nothing is sent anywhere automatically.
