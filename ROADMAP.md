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
      `REXGLUE_ENABLE_FIDELITYFX` met `REXGLUE_FIDELITYFX_BACKEND=vk`, en de FidelityFX
      Vulkan-backend heeft de LunarG Vulkan SDK nodig (`vulkan-1.lib`). Installatie
      (`winget install KhronosGroup.VulkanSDK`, vraagt admin) is op 25-10 geannuleerd bij de
      UAC-prompt. Na een mislukte configure `CMAKE_GENERATOR_PLATFORM` uit de CMake-cache
      halen. Op een 1440p-scherm is 2× al native, dus CAS (verscherpen) is de enige echte winst.*
- [x] **3. 60 fps**: frame-cap van de game vinden en patchen; physics, AI en FMV's testen.
      *Klaar (24-10). Er is geen 30 fps-cap in de code: de game presenteert op elke vblank
      (60 Hz) en rekent met de klok; de 360 haalde 60 gewoon niet. Nieuwe SDK-cvar
      `guest_vblank_rate` bepaalt de cap: Origineel 30, Remix 60. Getest in een Quick Race:
      race-klok loopt exact realtime op 30, 60 en 120 fps, AI racet, auto rijdt normaal;
      FMV's blijven op hun eigen 30 fps met audio in sync.*

## Prestaties (meting 24-10, RTX 5080, Quick Race Main Street)

- **Bottleneck is de host-GPU in het nauwkeurige EDRAM-pad.** In de race zit de GPU op
  ~96% (Vulkan `fsi`) of ~98% (D3D12 `rov`) al bij 1× resolutie; ongeremd ~75–92 fps.
  Met `fbo` (host render targets): ~88 fps bij ~80%. De kosten schalen nauwelijks met
  resolutie (interlock-serialisatie, niet pixelaantal).
- Remix (2×, 60 fps cap) houdt 60 fps in de race, maar met `fsi` op 94–97% GPU (dipjes
  naar 56); met `fbo` op ~68%.
- CPU-profiel (eigen sampling-profiler): de game-threads staan ~1/3 te slapen/yielden en
  ~22% in de D3D-"wacht op GPU"-lus (`sub_826E0088`); ze wachten dus vooral op de GPU.
- Codegen-opties: `ctr/xer/cr/reserved/non_volatile_as_local` werken;
  `non_argument_as_local` laat de game bij het opstarten vastlopen (r11/r12 worden buiten de
  aanroepconventie gebruikt). Winst in de race klein en ruisig (−4…+8%) omdat de GPU de
  rem is; nog niet overgenomen.
- Besluit (25-10): **Remix gebruikt `fbo`**, Origineel blijft `fsi`. Zij-aan-zij op 2560×1440
  (menu en race-start, zelfde toetsenreeks): auto, vloer en gebouwen gemiddeld < 1/255
  verschil; resterende verschillen kwamen door animatie en willekeurige tegenstanders. Geen
  banding in glow/neon. Remix-menu nu ~33% GPU i.p.v. ~53%.
- Open: `fsi` vs `fbo` bij hoge snelheid (bewegingsonscherpte), canyon en regen vergelijken;
  de interlock-shaders zelf optimaliseren (alleen nog relevant voor Origineel).

### Stretch goals

- [ ] **4. Widescreen/ultrawide**: aspect ratio + FOV patchen, HUD-ankers, framebuffer
      vergroten (aanpak van de PC-widescreenfixes voor dezelfde engine als referentie).
- [ ] **5. 120/240 fps**: simulatie losmaken van de framerate (vaste tick + interpolatie).
      *Verkenning (24-10): de simulatie is al tijdgebaseerd, dus interpolatie is
      waarschijnlijk niet nodig. `guest_vblank_rate=120` werkt: menu 120 fps, race 60–88 fps
      (prestatie is de grens) met realtime race-klok. Volledig ongeremd (`vsync=false`,
      1000 vblanks/s) hangt het spel bij het laden van een race. Te doen: race-prestaties
      profileren, physics bij hoge fps controleren (o.a. botsingen), hang onderzoeken.*

### Bewust niet

- **DLSS**: vereist motion vectors, depth en jitter die de engine niet levert. Interne
  supersampling via de resolutieschaal is op moderne GPU's ruim snel genoeg en trouwer.

## Wensen

- [ ] **Menu-optie "Exit to desktop"** in het hoofdmenu van de game (gevraagd 25-10). Nu kan
      afsluiten alleen met Alt+F4 of via F11 (venster) en de sluitknop. Aanpak uitzoeken: een
      extra item in de frontend van de game (EA FE-schermen in `ZZDATA`, plus een hook op de
      menu-actie), of als terugvaloptie een eigen pauze-overlay met "Afsluiten".

## Open problemen

- Career/races nog niet getest.
