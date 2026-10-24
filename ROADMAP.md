# Roadmap

Doel: een recompilatie die standaard **visueel trouw** is aan de Xbox 360-versie, met een
optioneel **Remix**-profiel voor moderne hardware. Renderer: **Vulkan**.

## Profielen

- **Origineel**: 1280×720, 30 fps, native 2× MSAA, nauwkeurige EDRAM-emulatie
  (fragment shader interlock), originele gamma, geen extra post-processing.
  Doel: niet te onderscheiden van een echte 360.
- **Remix**: hogere interne resolutie, anisotrope filtering, FSR/CAS bij presentatie,
  vsync naar keuze, later hogere fps en widescreen/ultrawide. Alles los instelbaar.

## Fases

- [x] **1. Vulkan + trouwe basis**: Vulkan-backend in de SDK aan en standaard, EDRAM via
      interlock, vergelijken met D3D12, profiel "Origineel" vastleggen.
      *Klaar (24-10). Vulkan- en D3D12-beeld identiek op het oog. Nog te doen: vergelijken
      met referentiebeelden van een echte 360 of Xenia.*
- [ ] **2. Remix (bestaande opties)**: resolutieschaal 2×/3×, anisotroop 16×, FSR/CAS,
      vsync; gebundeld in profiel "Remix".
      *Profiel staat (2×, 16× AF). FSR/CAS nog niet: vereist de experimentele build-optie
      `REXGLUE_ENABLE_FIDELITYFX` plus de Vulkan SDK. Op een 1440p-scherm is 2× al native,
      dus CAS (verscherpen) is de enige echte winst.*
- [ ] **3. 60 fps**: frame-cap van de game vinden en patchen; physics, AI en FMV's testen.

### Stretch goals

- [ ] **4. Widescreen/ultrawide**: aspect ratio + FOV patchen, HUD-ankers, framebuffer
      vergroten (aanpak van de PC-widescreenfixes voor dezelfde engine als referentie).
- [ ] **5. 120/240 fps**: simulatie losmaken van de framerate (vaste tick + interpolatie).

### Bewust niet

- **DLSS**: vereist motion vectors, depth en jitter die de engine niet levert. Interne
  supersampling via de resolutieschaal is op moderne GPU's ruim snel genoeg en trouwer.

## Open problemen

- Career/races nog niet getest.
