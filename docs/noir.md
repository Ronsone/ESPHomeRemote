# NOIR / SIGNATURE op de OpenRemote Rev6

Het NOIR-startscherm uit het bouwpakket (`simulator/main.c`, LVGL 8.3) is overgezet naar LVGL 9.5 op deze firmware.
Display, touch, toetsen (TCA8418), IR, Bluetooth, dock en Home Assistant blijven de bestaande Aggro600-drivers.

## Wat er is

- **NOIR Thuis** als startscherm: statusbalk (klok, HA-punt, accu), titel *NOIR / SIGNATURE*, tandwiel, vier tegels
  (TV, Audio, Verlichting, Scènes) en onderaan *Terug* / *Thuis*. Posities, maten, kleuren, Inter-letters, Lucide-iconen
  en de Obsidian-wallpaper komen 1-op-1 uit de simulator (240 × 320).
- **Tegels:** TV → de gewone Aggro600-startpagina van de activiteit (nu-afspelen, activiteitenbalk, toetsen naar de tv);
  Audio → de mediapagina; Verlichting / Scènes → een menupagina uit de configurator (`noir_licht_seite`,
  `noir_scenes_seite`, leeg = menu); tandwiel → menu. Menutoets → menu → Terug brengt je terug op NOIR Thuis.
- **KEF LSX II:** de fysieke toetsen Lauter / Leiser / Stumm gaan op **elk** scherm naar `noir_kef_entity`
  (`media_player.volume_up` / `volume_down` / `volume_mute`), vóór alle scherm- en invoerlogica en vóór de
  toetsindeling. Ingedrukt houden herhaalt elke 300 ms. Ze lopen over dezelfde route als andere HA-acties van de remote
  (wifi, Bluetooth-link of dock, met wachtrij na het wekken). In de toetsentest worden ze niet doorgestuurd.
- **Skin NOIR** (nr. 6) voor de rest van de firmware (menu, instellingen): dezelfde grafietkleuren.

## NOIR-builder: tegels van NOIR Thuis in de configurator

Configurator → je remote → **NOIR**: voorbeeld in NOIR-stijl en de knop *Eigen tegels aanmaken* / *Tegels bewerken*.
De tegels zijn de items van de menupagina **`noir`** en worden in de gewone menu-editor bewerkt (toevoegen, slepen,
grootte, naam, icoon, stappen). Opslaan = direct op de remote, **zonder compileren**.

- Raster 6 kolommen; hoogte 2 = grote tegel (icoon boven, naam onder), hoogte 1 = smalle balk. Te veel = scrollen.
- Tik: *Actie* voert de stappen uit, *Pagina* opent een menupagina, *Ingebouwde pagina* bijv. Mediaspeler of
  *NOIR: TV-scherm*, *Schakelaar/Licht/Rolluik* schakelt de entiteit.
- TV zoals voorheen: Actie → Functie van de remote → *NOIR: TV-scherm (TV + KEF aan)* (`noir_tv`).
- Iconen: alle `mdi:`-iconen uit de icoonkeuze, plus `noir:tv`, `noir:audio`, `noir:licht`, `noir:scenes`.
- Geen pagina `noir` = de ingebouwde tegels.

## NOIR TV-scherm

Tik op **TV** in NOIR Thuis:
1. De remote start de activiteit **TV** uit de configurator (`noir_tv_activiteit`). Zet daar bij *Beim Starten*
   de stappen die de tv en de KEF aanzetten: IR LG Power On, `media_player.turn_on` en `media_player.select_source`
   (`tv`) op de KEF, eventueel een wachttijd. Loopt de activiteit al maar staat de tv uit, dan lopen de stappen opnieuw.
2. Het NOIR TV-scherm opent: status uit HA (*Aan · bron* / *Uit* / *Niet bereikbaar*), aan/uit-knop
   (uit = `media_player.turn_off`, aan = de activiteit), zes app-tegels (PS5, Switch, Netflix, YouTube, Cookie IPTV, Plex)
   die `noir_tv_script` aanroepen met `tv_source` uit `noir_tv_bronnen`.
3. Fysieke toetsen op dit scherm: D-pad, OK, Terug en Home gaan via `webostv.button` naar de tv (pijlen herhalen bij
   ingedrukt houden). Volume blijft naar de KEF gaan. Terug/Thuis op het scherm gaan naar NOIR Thuis.

## Instellen (je eigen `esphomeremote.yaml`)

```yaml
substitutions:
  noir_aan: "true"                               # "false" = gewone Aggro600-startpagina
  noir_kef_entity: "media_player.kef_lsx_ii"     # exacte entity_id uit HA (leeg = volumetoetsen volgens toetsindeling)
  noir_licht_seite: ""                           # id van een menupagina uit de configurator, of leeg
  noir_scenes_seite: ""
  noir_airco_seite: ""                           # menupagina met je airco-knoppen (IR/HA), leeg = menu
  noir_tv_entity: "media_player.woonkamer_lg_webos_tv_oled65b56la_deuqljp"
  noir_tv_activiteit: "TV"                       # naam van de activiteit in de configurator
  noir_tv_script: "script.tv_bron_kef"
  noir_tv_bronnen: "PS5 Gameconsole|HDMI 2|Netflix|YouTube|Cookie IPTV|Plex"
```

In HA: Instellingen → Apparaten → ESPHome → de remote → *Dit apparaat toestaan Home Assistant-acties uit te voeren*
moet aan staan, anders komen de KEF-opdrachten niet aan.

## Nog niet overgezet (volgende stappen uit de bouwgids)

Audio-, Scènes- en Verlichting-schermen in NOIR-stijl met echte HA-status, letterwiel/QWERTY, Spotify, dimmer,
encoder. Die gebruiken nu de bestaande Aggro600-pagina's (in skin NOIR).
