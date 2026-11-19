# Skins

A skin recolours the **whole** interface of the remote – background, cards, buttons, lines, text, grey text, accent,
green/amber/red – and can add a **background image** from the SD card. Skins switch at runtime: no new firmware, no
restart.

> Builds on the accent colour (`esphomeremote-accent.h`); the accent setting keeps working and applies to every skin
> that has no accent of its own.

## Built-in skins

| Nr | Name | Look |
|---|---|---|
| 0 | Standard | the familiar dark blue-grey |
| 1 | Hell | light: grey background, white cards, dark text |
| 2 | OLED | pure black, high contrast |
| 3 | Nord | Nord palette (polar night + frost) |
| 4 | Retro | amber terminal |
| 5 | Synthwave | dark purple, neon pink accent |

## Your own skins

Configurator › remote › **Skins** (also under Settings › Anzeige): *Neuer Skin* on the basis of a built-in one, then
change any of the 11 colours. Colours you leave out are mixed from the others (e.g. the grey steps from *Text grau*
and *Regler-Spur*). Up to 8 skins per remote, numbered 10, 11, … in order. **Speichern** transfers them.

Config format (`cfg.skins`, validated by `validate_skins`):

```json
"skins": [
  {"name": "Mijn Skin", "hg": "#101820", "karte": "#1A2430", "text": "#FEE715", "akzent": "#FEE715",
   "bild": true, "dimmen": 40, "ueberall": false}
]
```

Keys: `hg` background · `karte` cards · `karte2` buttons · `linie` lines · `spur` slider track/inactive ·
`text` · `matt` grey text · `akzent` · `ok` · `warn` · `fehler`; `bild` background image, `dimmen` 0–90 %,
`ueberall` image on all pages instead of only the home screen.

## Choosing a skin

- **Remote:** assign *Skin wechseln (nächster)* or *Skin wählen …* to a key, a menu tile or a gesture. The name is shown
  briefly.
- **Configurator:** Settings › Anzeige › *Skin*.
- **Home Assistant:** `select.<remote>_skin` – usable in automations (e.g. *OLED* at night).

The choice is the setting `skin_nr` (0–5 built-in, 10+ own). Like the display language it is per remote and is
**not** synced to the group by default.

## Background image

1. `python3 tools/make_skin_wallpaper.py photo.jpg "Mijn Skin"` → `sd-card/themes/mijn_skin/background.rle`
   (240 × 320, RGB565 RLE; raw `background.rgb565` if that is smaller). `--farben 32` makes photos much smaller,
   `--einpassen` keeps the whole image, `--hochladen <ip>` uploads it right away.
2. Put it on the card at `/themes/<skin>/background.rle` (or `.rgb565` – the OpenRemote format works too).
   `<skin>` = name in lower case, spaces/dashes → `_`, only a–z 0–9. The configurator shows the exact path.
3. Turn on *Hintergrundbild* for the skin. The remote loads the image when the card is free (mic, cover logos and
   voice share its pins) and keeps it in PSRAM (150 KB).

If the file is missing the remote does not search again until the skin is changed or the remote restarts.

## How it works (firmware)

`esphomeremote-skin.h`. The interface uses one fixed set of ~18 colours plus the 5 accent steps. Every colour is a
*role*; a skin maps each role to a new colour. `skin::anwenden()` walks all pages, the top layer and the shared styles
and swaps every local colour that belongs to a role – the same mechanism the accent colour used, generalised.

To make switching A → B and late-built objects (menu, popups, widgets – they are still created with the standard
colours) unambiguous, `skin::bauen()` keeps every palette **injective**: no skin colour may equal the standard colour
of another role or another role of the same skin; pure black/white are reserved. Colliding colours are nudged by one
LSB (invisible). After every screen load the new screen and the top layer are refreshed (`skin::auffrischen`), only
when a non-standard skin is active.

New code: use `skin::f(skin::KARTE)` etc. instead of literal colours – or just keep using the standard colours, they
are mapped automatically on the next refresh.

**Limits:** colours that come only from the LVGL theme (not set locally or in `style_definitions`) are not
recoloured; images/covers are unchanged; two skins with the same name share one image folder.
