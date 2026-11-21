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

## Instellen (je eigen `esphomeremote.yaml`)

```yaml
substitutions:
  noir_aan: "true"                               # "false" = gewone Aggro600-startpagina
  noir_kef_entity: "media_player.kef_lsx_ii"     # exacte entity_id uit HA (leeg = volumetoetsen volgens toetsindeling)
  noir_licht_seite: ""                           # id van een menupagina uit de configurator, of leeg
  noir_scenes_seite: ""
```

In HA: Instellingen → Apparaten → ESPHome → de remote → *Dit apparaat toestaan Home Assistant-acties uit te voeren*
moet aan staan, anders komen de KEF-opdrachten niet aan.

## Nog niet overgezet (volgende stappen uit de bouwgids)

TV-, Audio-, Scènes- en Verlichting-schermen in NOIR-stijl met echte HA-status, letterwiel/QWERTY, Spotify, dimmer,
encoder. Die gebruiken nu de bestaande Aggro600-pagina's (in skin NOIR).
