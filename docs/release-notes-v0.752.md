# v0.752 – public test build

Third public test build of the Need for Speed: Carbon Collector's Edition (Xbox 360)
recompilation. Requires your own disc image or extracted disc of the **Collector's Edition**
(title ID 454107EC, version 0.0.0.7); see the README.

## New in this version

- **Smoother frame rate on more monitors.** Remix no longer runs at a fixed 120 fps but follows
  your monitor: 60 on 60 Hz, 144 on 144 Hz, 165 on 165 Hz, and the refresh rate divided by a
  whole number above that (120 on 240 Hz). A limit your monitor can't divide shows some frames
  longer than others, which looks jerky even when the counter says 120. Choose *Frame rate limit* in PC SETTINGS to
  set your own.
- **Follows your monitor.** The aspect ratio now matches your monitor by default, also in
  Original, and Remix picks its internal resolution from the screen height (1440p on 1080p and
  1440p screens, 4K on 4K screens).
- **Portable mode.** *Save location* > *Next to nfsc.exe* (PC SETTINGS, Advanced) or
  `portable = true` keeps savegames and the shader cache in a `userdata` folder next to
  `nfsc.exe`, so the whole game moves with its folder. The first time, your savegames and cache
  are copied there.
- The shader cache moves from `Documents` (often synced by OneDrive) to
  `%LOCALAPPDATA%\nfsc\cache`. An existing cache moves along.
- **Pause.** A race pauses when you open PC SETTINGS or switch to another window, and goes on
  when you come back. While the game is in the background, the controller doesn't steer it.
- **Faster start.** Remix skips the logos and intro videos and goes straight to the main menu
  (*Logos and intro videos* in PC SETTINGS). Any key or button stops it, so a new player can
  still create an alias.
- **Keyboard controls** in PC SETTINGS: choose your own keys for the most used buttons.
- The online options (QUICK MATCH, CUSTOM MATCH) are gone from the main menu; they only said
  that Xbox Live can't be reached.
- **Crash reports.** If the game crashes, it writes a small dump file to the `logs` folder and
  tells you where it is. Attach it to an issue together with the log.
- PC SETTINGS warns before *Apply and restart* that unsaved progress is lost, and stays open
  with a message when `nfsc.toml` can't be saved.

## Upgrading

Extract over the old folder or into a new one. Your savegames stay where they were. A
`guest_vblank_rate` you set yourself in `nfsc.toml` still wins over the new frame rate limit.

## Not tested yet

- The rest of the Career story (territories, crew, canyon duels).
- Split screen, and PC SETTINGS with a real controller (tested with the keyboard).
- AMD and Intel GPUs, and ultrawide monitors with the new automatic aspect ratio.

## Reporting problems

Open an issue with what happened, your GPU, your monitor's refresh rate, the profile and
settings, and the log file (and dump file, after a crash) from the `logs` folder next to
`nfsc.exe`. Nothing is sent anywhere automatically.
