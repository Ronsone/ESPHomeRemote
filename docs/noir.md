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

## NOIR Verlichting

Tegel *Verlichting* (of functie *NOIR: Verlichting (Lampen)* op een eigen tegel) opent een eigen NOIR-scherm (simulator
`build(3)`): per lamp een brede knop *Naam · Aan/Uit* of, dimbaar, *Naam · 65%*. Tikken = aan/uit (`light.toggle`,
`switch.toggle`), lang drukken op een dimbare lamp = dimvenster met schuifregelaar (`light.turn_on brightness_pct` bij
loslaten, 0 = uit). De echte stand komt elke 3 s uit HA (`/api/esphomeremote/states`), alleen zolang het scherm open is.

Lampen komen uit de configurator (NOIR → *Lampenliste anlegen*: menupagina `noir_licht`, elke kachel met entiteit is een
lamp, `light.*` = dimbaar) – zonder te compileren. Zonder die pagina geldt `noir_lampen` uit de YAML.

## NOIR Airco (Mitsubishi Heavy Industries, infrarood)

Tegel *Airco* (of functie *NOIR: Airco*) opent het airco-scherm: aan/uit, temperatuur (- / + of de pijltjestoetsen,
OK = aan/uit), modus (Auto, Koel, Droog, Fan, Warm), fan (Auto, 1-4) en swing (uit, op/neer, links/rechts, alles).
Elke wijziging stuurt de hele stand als een IR-frame in het protocol van de MHI-afstandsbediening RLA502A704A
(SRK..ZS), via dezelfde IR-route als de andere knoppen. De remote kan de airco niet uitlezen: hij onthoudt de laatst
gestuurde stand (global `noir_airco_stand`). Met `noir_airco_seite` gezet opent de tegel de menupagina zoals voorheen.

## NOIR Scènes

Tegel *Scènes* (functie *NOIR: Szenen-Bildschirm*) opent het scènes-scherm. Vooraan staat altijd **Alles uit**: alle
lampen uit de lampenlijst, de tv, de KEF en de airco (IR) uit - ook als functie *NOIR: Alles aus* voor knoppen of
activiteiten. Eigen scènes maak je zonder te compileren in de configurator (NOIR -> *Szenen anlegen*: menupagina
`noir_scenes`); elke kachel voert zijn stappen uit (HA, IR, wachten, functies van de remote) of start zonder stappen
een `scene.`- of `script.`-entiteit uit HA.

## NOIR Netflix-dashboard

De Netflix-tegel van het TV-scherm start Netflix en opent het dashboard (functie *NOIR: Netflix-Dashboard*):

- **Top 10 in Nederland**: de officiele weeklijst van Netflix (Tudum, elke dinsdag nieuw), als posterrij met grote
  rode rangnummers.
- **Populair op Netflix**: TMDB discover (provider Netflix, regio NL), dagelijks.
- **Laatst gestart**: de laatste 4 titels die je via de remote opende (global `noir_nf_laatst`).
- Tik op een poster: detailscherm met achtergrond, jaar, genre, seizoenen, Kijkwijzer, Nederlandse tekst en
  **Afspelen in Netflix** (actie `esphomeremote.tv_app` met `content` = het Netflix-nummer uit Wikidata (P1874, via
  de Wikidata-id van TMDB) - de LG opent de serie direct; zonder nummer opent alleen Netflix). Staat de tv uit, dan
  eerst de TV-activiteit en na 8 s openen.
- Fysieke knoppen op deze schermen: play/pauze (`media_play_pause`), stop, terug-/vooruitspoelen (webOS `STOP`,
  `REWIND`, `FASTFORWARD`), D-pad en OK. *Terug*: detail → dashboard → TV-scherm.

De integratie (`netflix.py`) haalt alles op en levert het aan de remote via HA (`/api/esphomeremote/netflix`,
posters als RGB565 64×96 en achtergrond 216×110 via `/api/esphomeremote/netflix/img`) - de remote zelf hoeft niet
op internet. HA wel: `www.netflix.com`, `api.themoviedb.org`, `image.tmdb.org`, `query.wikidata.org`. Posters, teksten en Populair
vragen een gratis **TMDB-sleutel** (Instellingen → Apparaten & diensten → ESPHomeRemote → Configureren); zonder
sleutel alleen de Top 10-titels. *This product uses the TMDB API but is not endorsed or certified by TMDB.*

## NOIR UI-instellingen

Het tandwiel op NOIR Thuis opent *UI-instellingen* (simulator `build(13)`): **Skin** (NOIR / Obsidian, SIGNATURE /
Graphite, PURE / Black, Nautisch / Navy, Eigen = kleuren van de actieve Aggro600-skin), **Wallpaper** (Obsidian / satijn,
Graphite / zacht, Effen) en **Afronding** (8-20 px, elke tik +2). Een skin kiezen zet ook afronding en wallpaper van die
skin, zoals in de simulator. Alles wordt bewaard (globals `noir_ui_*`). *Alle instellingen (menu)* opent het gewone
Aggro600-menu. De NOIR-lagen doen niet mee met de Aggro600-skins (vlag `LV_OBJ_FLAG_USER_4`).

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
  noir_lampen: "Woonkamer=light.woonkamer_lamp|Keuken=switch.keuken|Gang=light.lamp_gang"   # of leeg + configurator
```

In HA: Instellingen → Apparaten → ESPHome → de remote → *Dit apparaat toestaan Home Assistant-acties uit te voeren*
moet aan staan, anders komen de KEF-opdrachten niet aan.

## Nog niet overgezet (volgende stappen uit de bouwgids)

Audio- en Scènes-schermen in NOIR-stijl, letterwiel/QWERTY (tekstinvoer), Spotify, encoder, wallpaper
Grand Line. Die gebruiken nu de bestaande Aggro600-pagina's (in skin NOIR).

## Audio (KEF + Spotify) - 2026-11-22

Tegel **Audio** opent NOIR Audio zodra `noir_kef_entity` of `noir_spotify` is ingevuld (anders de media-pagina zoals voorheen).

- **Audio**: KEF aan/uit, "nu speelt" (tik = Spotify), ingangen (wifi, bluetooth, tv, optical, analog, usb), volume en subwoofer.
- **Spotify**: hoes, titel/artiest, voortgang, shuffle/herhalen, jouw playlists, recent gespeeld, Spotify Connect-apparaten.
  Play/pauze, vorige/volgende: fysieke toetsen. Volume: altijd de KEF.

Substitutions:

```yaml
  noir_kef_entity: media_player.woonkamer_2
  noir_kef_sub:    number.woonkamer_sw_gain
  noir_spotify:    media_player.woonkamer_spotify_rons_rosenquist
```

De integratie levert `/api/esphomeremote/spotify` (lijsten uit het bladeren van de Spotify-media_player) en
`/api/esphomeremote/spotify/img` (hoesjes 96/64 px RGB565). De stand komt uit `/states` (elke 2 s zolang het scherm open is).
