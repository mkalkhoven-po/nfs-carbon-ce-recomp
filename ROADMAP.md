# Roadmap

Doel: een recompilatie die standaard **visueel trouw** is aan de Xbox 360-versie, met een
optioneel **Remix**-profiel voor moderne hardware. Renderer: **Vulkan**.

## Profielen

- **Origineel**: 1280×720, 30 fps, native 2× MSAA, nauwkeurige EDRAM-emulatie
  (fragment shader interlock), originele gamma, geen extra post-processing.
  Doel: niet te onderscheiden van een echte 360.
- **Remix**: hogere interne resolutie, anisotrope filtering, FSR/CAS bij presentatie,
  120 fps, widescreen/ultrawide naar keuze. Alles los instelbaar.

## Fases

- [x] **1. Vulkan + trouwe basis**: Vulkan-backend in de SDK aan en standaard, EDRAM via
      interlock, vergelijken met D3D12, profiel "Origineel" vastleggen.
      *Klaar (24-10). Vulkan- en D3D12-beeld identiek op het oog. Vergeleken (29-10) met
      360-opnames (YouTube, "NEED FOR SPEED: CARBON | Xbox 360 Gameplay" van BenedictPlays,
      1080p): Career-races met de RX-8, cutscenes, race-intro. Zelfde belichting, bloom,
      motion blur, lakkleur en HUD-layout. De opname is ~10% helderder en het HUD-cyaan is er
      lichter (R 130 i.p.v. 76): kleurverschuiving van opname/YouTube, want de HUD-kleuren
      zijn vast. Geen reden om iets aan te passen; exacte vergelijking kan alleen met eigen
      opnames van dezelfde scène.*
- [x] **2. Remix (bestaande opties)**: resolutieschaal 2×/3×, anisotroop 16×, FSR/CAS,
      vsync; gebundeld in profiel "Remix".
      *Klaar (25-10): Remix = 2×, 16× AF, `fbo`, CAS (`present_effect = cas`, milde standaard-
      scherpte; extra via `present_cas_additional_sharpness`). Effect is subtiel omdat 2× op
      1440p al native is. Bouwnotities hieronder.
      Oorspronkelijke notitie: FSR/CAS vereist de experimentele build-optie
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
- Codegen-opties: `ctr/xer/cr/reserved/non_volatile_as_local` lijken te werken, maar maken de
  video's (FMV's) kapot: groene/paarse blokken (gezien 29-10; eerdere tests sloegen de video's over). Uit;
  `non_argument_as_local` laat de game bij het opstarten vastlopen (r11/r12 worden buiten de
  aanroepconventie gebruikt). Winst in de race klein en ruisig (−4…+8%) omdat de GPU de
  rem was; op 29-10 opnieuw gemeten (+1–2%), maar niet overgenomen vanwege de video's.
- Besluit (25-10): **Remix gebruikt `fbo`**, Origineel blijft `fsi`. Zij-aan-zij op 2560×1440
  (menu en race-start, zelfde toetsenreeks): auto, vloer en gebouwen gemiddeld < 1/255
  verschil; resterende verschillen kwamen door animatie en willekeurige tegenstanders. Geen
  banding in glow/neon. Remix-menu nu ~33% GPU i.p.v. ~53%.
- `fsi` vs `fbo` bij hoge snelheid (29-10): Quick Race-canyon (Knife's Edge) met vol gas, zelfde
  momenten (80–105 mph, bewegingsonscherpte, vonken): op het oog gelijk, gemiddeld 3/255 verschil
  (vooral tegenstander en vonken). Regen is in Quick Race niet te kiezen (geen weeroptie).
- Open: de interlock-shaders zelf optimaliseren (alleen nog relevant voor Origineel).

### Stretch goals

- [x] **4. Widescreen/ultrawide**: aspect ratio + FOV patchen, HUD-ankers, framebuffer
      vergroten. *Werkt (28-10) met `aspect_ratio` (21:9, 32:9, eigen B:H), in
      `nfsc/src/widescreen.cpp`:
      - Rendergrootte: `sub_8250B0D0` en `sub_82500168` zetten 1280×720 in 0x82C651E8/EC en
        0x82C651F0/F4; overschreven na beide.
      - Layout: `sub_824FFAB0` bouwt viewport- en tile-rechthoeken met x1 = 1280; verbreed.
        Daarmee vullen 3D en HUD de breedte, hor+ werkt vanzelf, HUD ankert aan de randen.
      - EDRAM: de hoofdscène gebruikt 4× MSAA-tiles van 1280×256 (precies 10 MiB); descriptors
        worden geregistreerd via `sub_82505998` (tabel 0x82D5B250, +36 = MSAA). Tiles die bij
        4× niet passen gaan naar 2× → geen banden; maximaal 2560 breed (32:9).
      - Video's: 16:9-materiaal werd uitgerekt; SDK-patch 0004 presenteert tijdens het spelen
        van een `.wmv` in `present_movie_aspect` (16:9).
      - Laadschermen (`Loading*.fng`) tekent de game over de volle breedte uitgerekt; zolang
        zo'n FE-pakket actief is presenteert de app op 16:9 (SDK-patch 0005), net als video's.
      Getest: intro-video's, titelscherm, hoofdmenu, race-intro en Quick Race (ook met Remix:
      60 fps, ~80% GPU). Op 21:9 nagelopen (28/29-10), alles correct (niet uitgerekt, HUD aan
      de randen, dialogen gecentreerd): My Cars, Challenge Series, Quick Race-kaart en -opties,
      autokeuze, crew, race-HUD met binnenspiegel, pauzemenu, Options, Statistics, Career-menu,
      aliasinvoer, autosave-dialogen, besturingsscherm, proloog, safehouse, wereldkaart,
      Customization (Performance, winkelwagen), free roam met SMS, pauzemenu en Photo Mode,
      laadschermen, canyon-race met eindscherm, Standings en Player Stats (na een verloren
      race, 29-10; een gewonnen race haalt de testroute zonder sturen niet). Splitscreen
      (Quick Race, Game Mode "Split Screen") vraagt om START op een tweede controller; niet
      getest.*
- [x] **5. 120/240 fps**: simulatie losmaken van de framerate (vaste tick + interpolatie).
      *Klaar (29-10): niet nodig, de simulatie rekent al met de klok. Origineel haalt op 120 Hz
      in races gemiddeld ~117 fps, Remix (nu standaard 120) 105–111; free roam vast 120. Open:
      de oorzaak van de Xid 13-crashes van vóór patch 0006–0008 is nooit gevonden (sindsdien
      niet meer gezien). Details hieronder.
      Verkenning (24-10): de simulatie is al tijdgebaseerd, dus interpolatie is
      waarschijnlijk niet nodig. `guest_vblank_rate=120` werkt: menu 120 fps, race 60–88 fps
      (prestatie is de grens) met realtime race-klok.
      Onderzoek 29-10 (`log_fps` toegevoegd, RTX 5080, 240 Hz-scherm):
      - Op 240 Hz loopt de race-klok exact realtime (12 s per 12 s). Menu ~122 fps: een frame
        van ~7 ms mist een vblank en wacht op de volgende (kwantisatie). Op 1000 Hz menu ~140.
      - Plafond onafhankelijk van cap, resolutie (1×/2×) en `fsi`/`fbo`: menu ~140, race
        60–110 (120 Hz-cap: gemiddeld 69–77, minimum ~45). Occlusion queries uit: geen effect.
      - GPU staat op 96% maar trekt ~100 W van 360 W; GPU-timestamps: in de race is de GPU
        de hele seconde bezig met onze command buffers (idle ~3 ms/s). Geen rekenwerk dus,
        maar serialisatie.
      - Oorzaak: de game schrijft vertexdata vlak voor elke draw; de SDK uploadt die pagina's
        per draw naar het shared-memory-buffer (~90.000 uploads/s, ~650 per menuframe, 1000
        per raceframe), en elke upload breekt de render pass en zet 2 barriers. Per frame
        ~585 render passes van 7–13 µs = ~70% van de GPU-tijd; copies en barriers samen ~25%.
      - Proef "upload-ahead" (bij een upload ook volgende ongeldige pagina's meenemen,
        256 pagina's): render passes en barriers gehalveerd, GPU-tijd per frame −16%, maar
        6× zoveel pagina's geüpload (de CPU schrijft ze nog) en meer paginafouten: netto geen
        winst. Beter idee: alleen pagina's meenemen die sinds de laatst gelezen ring-write
        pointer niet meer zijn beschreven (schrijf-volgnummer per pagina), of vooruitkijken
        in de PM4-ring naar de bereiken van komende draws. Diepe SDK-wijziging.
      - Stabiliteit: 4 van ~15 runs op 240/1000 Hz eindigden in de race of bij het laden
        ervan met NVIDIA Xid 13 "SKEDCHECK36_DEPENDENCE_COUNTER_UNDERFLOW" (device lost,
        exit 0xC0000409); 3 runs op 120 Hz en eerdere dagen op 30/60 nooit. Validatielagen:
        geen fouten. Oorzaak nog onbekend.
      Vervolg 29-10:
      - Let op: de metingen van 28/29-10 hierboven zijn met een release-build zonder
        optimalisatie gedaan (een mislukte configure had `CMAKE_CXX_FLAGS_RELEASE` leeggemaakt;
        de game-code draaide op `-O0`). Hersteld; nu weer `-O3`.
      - `clear_memory_page_state=false` en barriers zonder fragment-stage: geen effect.
      - SDK-patch 0006 "upload hoisting": uploads tijdens een render pass worden verzameld in
        een slot dat vóór die render pass is opgenomen (één barrier + copies + barrier), tenzij
        een draw in de pass de pagina's al las (bitset per pagina; dan de oude route). In de
        race ~95% van de ~90.000 uploads/s verplaatst; GPU 93% → ~50%.
      - Resultaat (`-O3`, Quick Race, wisselend per race door de AI): 120 Hz gemiddeld 78–90,
        minimum ~65–71 (zonder 0006: 72, minimum 53); 240 Hz gemiddeld 95–101, minimum 62–70;
        menu op 240 ~140. Nu CPU-gebonden: de twee game-threads (render en een tweede) zitten op
        ~98%, de GPU-thread op ~57%; ~10–15k schrijffouten/s op beschermde pagina's.
      - Stabiliteit: 5 runs (2× 120, 3× 240) zonder Xid 13.
      - CPU-kant (29-10, benchmark: Quick Race zonder gas op 1000 Hz, menu-mediaan stabiel
        ±1%, race ±7%): codegen-opties `ctr/xer/cr/reserved/non_volatile_as_local`: menu 172 →
        175, race 110 → 115. Weer uitgezet: ze maken de video's kapot (de cijfers hieronder zijn
        nog met de opties gemeten, dus ~1–2% te hoog).
        Upload-ahead opnieuw geprobeerd (16/64 pagina's): menu 3–5% trager, race niet beter.
      - Profiel (`-O3`, PDB's via `-Wl,/DEBUG`): game-thread slaapt ~75%, render-thread wacht
        ~57% in de GPU-wachtlus `sub_826E0088`, GPU-thread (command processor) ~80% bezig:
        ~15% `NtProtectVirtualMemory` (pagina's beschermen na elke upload, ~90.000/s), ~15%
        driver, de rest PM4-verwerking verspreid. De command processor is nu de rem.
      - SDK-patch 0007 "hot pages": pagina's die 3 frames op rij geüpload worden, worden niet
        meer beschermd maar bij elke aanvraag vergeleken met een schaduwkopie (~2000–6500
        pagina's hot; van ~1 miljoen aangevraagde ongeldige pagina's/s worden er ~70–110k echt
        geüpload). Nooit hot: pagina's met een watch (texture cache; een nieuwe watch maakt
        ze weer normaal en beschermd) en GPU-geschreven pagina's; wijzigingen gaan wel naar de
        global watches (scaled resolve). Na 120 frames ongewijzigd weer normaal.
      - `clear_memory_page_state=false` (SDK-standaard true, workaround voor andere games: alle
        CPU-pagina's elk frame ongeldig) nu in beide profielen.
      - Benchmark (menu-mediaan, 1000 Hz): 174,7 → 180,7 (clear uit) → 194,7 (hot pages) →
        208,5 (beide, +19%). Races met gas: 120 Hz gemiddeld ~117 (minimum 81–93), 240 Hz
        124–128 (minimum 86–97), menu op 240 ~174; GPU ~50%. Beeld gecontroleerd (Origineel
        en Remix: menu's, kaart, intro, race); witte flits aan het eind van de race-intro zit
        ook zonder hot pages. 4 runs op 120/240 zonder Xid 13.
      - Career (savegame, 120 Hz): dezelfde Career-race (Main Street, RX-8, met crash) werd met
        en zonder hot pages vrijwel frame voor frame gelijk afgespeeld: zonder 82,3 fps gemiddeld
        (minimum 57), met 97,7 (minimum 67), beeld gelijk. Free roam zelf niet opnieuw gereden:
        de route erheen hangt af van laadtijden (Space in de Career-hub opent soms free roam,
        soms de wereldkaart met een race).
      - Free roam (Career-hub → FREE ROAM, rondrijden door Historic Chinatown en een tunnel):
        120 fps vast (minimum 117), beeld goed. Opstartvideo's weer goed na het terugdraaien van
        de codegen-opties.
      - Remix-profiel nu op 120 fps (was 60): races gemiddeld 105–111 (minimum 62–82) bij
        ~45–50% GPU; met 21:9 gemiddeld 117 (minimum 78); video's goed.
      - Upstream (29-10): 0006 bestond al als open PR rexglue/rexglue-sdk#536 (zelfde ontwerp,
        voor MoltenVK); daar onze NVIDIA-cijfers als reactie gezet. Hot pages (0007) aangeboden:
        issue #548, PR #549 (los gemeten met SDK-standaard: menu 163 → 190, race 87 → 147).
        Fix #535 overgenomen als patch 0008. Op 30-10 ook 0001 (GapFill na `bctr`, issue #552,
        PR #554: op `development` 72.088 → 72.847 functies, `sub_824DA838` ontbrak) en 0002
        (`guest_vblank_rate`, #553 / #555) aangeboden.
      - Review (30-10, review-agent) van #549/#554/#555, verwerkt als tweede commit per PR en in
        de lokale patches: hot pages die niet meer worden opgevraagd gaan na 120 frames terug
        (bleven eerst voor altijd hot en vulden de limiet), limiet in bytes (64 MB), beveiligen
        per reeks pagina's, `shared_memory_hot_pages` standaard uit (alleen op Windows/Vulkan
        getest; ons profiel zet het aan); `guest_vblank_rate` begrensd op 1–1000 (commandoregel
        en NaN omzeilden de range) en geen vblank-salvo bij wisselen; GapFill ontdekt ook de
        aanroepdoelen van gap-functies en probeert elke functie één keer (NFSC-code
        byte-identiek). Getest: canyon-race Remix op 120 Hz met hot pages 117–120 fps, zonder
        97–119; `--guest_vblank_rate=100000` geeft ~200 fps in het menu zonder vastlopen.
        Mergen kan alleen een rexglue-maintainer.*

### Bewust niet

- **DLSS**: vereist motion vectors, depth en jitter die de engine niet levert. Interne
  supersampling via de resolutieschaal is op moderne GPU's ruim snel genoeg en trouwer.

## Wensen

- [x] **Menu-optie "Exit to desktop"** in het hoofdmenu (gevraagd 25-10, klaar 25-10).
      `nfsc/src/exit_option.cpp`: hooks op de menu-opbouw (`sub_82642DB0`), add-option
      (`sub_82313658`), de taal-lookup (`sub_8231F7B8`, eigen label "EXIT TO DESKTOP") en
      option React (`sub_8230EE18`, ID 10 sluit de game via dezelfde route als Alt+F4).
      Herkend aan het eigen label: ID 10 is in het pauzemenu PHOTO MODE (die sloot eerst de
      game af, gevonden en opgelost 29-10). In het pauzemenu niet nodig: daar staat al "terug
      naar het hoofdmenu".

## Onderzocht

- **"Vlekjes" op het eindscherm** (29-10, opgelost 30-10): geen renderfout maar een fout in onze
  screenshots. GDI+ (`Graphics.CopyFromScreen`) gebruikt intern de kleur 0x0D0B0C als markering
  en maakt pixels met precies die kleur zwart (in een 32-bit ARGB-bitmap ook transparant, in een
  viewer met witte achtergrond dus witte vlekken). Het donkere eindscherm heeft veel pixels van
  ongeveer die kleur, vandaar de randjes. Bewezen met een kale GDI-`BitBlt` op hetzelfde moment:
  daar staat gewoon RGB (13,11,12). Onderweg nagelopen met een tijdelijke draw-log en
  resolve-dumps: de frontbuffer van de game en de swap-textuur zijn schoon. Screenshots voortaan
  met kale GDI (`BitBlt` in een DIB).
- **"Texture fetch constant has invalid type"** (~500 waarschuwingen per race-intro, 25-10):
  de game samplet tijdens de intro-cutscene een texture-slot zonder texture (type invalid,
  basisadres 0, afmeting 0). Xenia's standaard (slot negeren) geeft geen zichtbare fout;
  `gpu_allow_invalid_fetch_constants=true` onderdrukt de waarschuwingen maar zou geheugen op
  adres 0 als texture lezen. Standaard gelaten; onschuldig.

## Open problemen

- Career: met een savegame getest tot de eerste Career-race (Main Street, RX-8), de Career-hub,
  wereldkaart, safehouse en free roam. Verder in de verhaallijn (gebieden veroveren, crew,
  canyon-duels) nog niet: daarvoor moeten races gewonnen worden, wat de testroutes (toetsen
  zonder sturen) niet kunnen. Savegames werken (alias en autosave in `Documents\nfsc`).
