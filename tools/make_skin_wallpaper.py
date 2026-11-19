#!/usr/bin/env python3
"""Hintergrundbild fuer einen Skin der Remote erzeugen (2026-11-19, Firmware: esphomeremote-skin.h).

Beliebiges Bild (PNG/JPG/...) -> 240 x 320, RGB565 little-endian, als
  sd-card/themes/<skin>/background.rle      lauflaengenkodiert "RL16" (wie die Cover), wenn das kleiner ist
  sd-card/themes/<skin>/background.rgb565   roh (153600 Byte, wie OpenRemote), sonst
plus background_preview.png zum Ansehen. Die Firmware sucht zuerst .rle, dann .rgb565 - liegt eine alte
Datei der anderen Art noch auf der Karte, loeschen (sonst gewinnt die .rle).

<skin> = Name des Skins in Kleinbuchstaben, Leer-/Bindestriche als "_", sonst nur a-z0-9
("Mijn Skin" -> mijn_skin). Der Konfigurator zeigt den genauen Pfad beim Skin an.

    python3 tools/make_skin_wallpaper.py foto.jpg "Mijn Skin"
    python3 tools/make_skin_wallpaper.py foto.jpg "Mijn Skin" --farben 32      # weniger Farben: kleiner, laedt schneller
    python3 tools/make_skin_wallpaper.py logo.png "Retro" --einpassen --rand "#0B0A07"
    python3 tools/make_skin_wallpaper.py foto.jpg "Mijn Skin" --hochladen 192.168.1.50

Hochladen braucht den Konfig-Modus der Remote (tools/sd_upload.py schaltet ihn selbst). Danach den Skin an der Remote
einmal wechseln oder neu starten - ein fehlendes Bild sucht die Remote erst nach einem Skin-Wechsel erneut.
Braucht Pillow.
"""
import argparse
import re
import struct
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageOps

HERE = Path(__file__).resolve().parent.parent
W, H = 240, 320


def slug(name: str) -> str:
    """Gleiche Regel wie skin::slug() in der Firmware und skinSlug() im Konfigurator."""
    s = re.sub(r"[^a-z0-9 _-]", "", name.lower())
    s = re.sub(r"[ _-]+", "_", s).strip("_")
    return s or "skin"


def hex_rgb(h: str):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def rgb565(im: Image.Image) -> list[int]:
    im = im.convert("RGB")
    px = [im.getpixel((x, y)) for y in range(H) for x in range(W)]
    return [((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3) for r, g, b in px]


def rle16(px: list[int]) -> bytes:
    out = bytearray(b"RL16" + struct.pack("<HH", W, H))
    i, n = 0, len(px)
    while i < n:
        v, j = px[i], i + 1
        while j < n and px[j] == v and j - i < 65535:
            j += 1
        out += struct.pack("<HH", j - i, v)
        i = j
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bild")
    ap.add_argument("skin", help="Name des Skins wie im Konfigurator")
    ap.add_argument("--einpassen", action="store_true", help="ganzes Bild zeigen (mit Rand) statt fuellen und beschneiden")
    ap.add_argument("--rand", default="#000000", help="Randfarbe beim Einpassen (#RRGGBB)")
    ap.add_argument("--farben", type=int, default=0, help="auf so viele Farben reduzieren (z. B. 16-64; kleinere Datei)")
    ap.add_argument("--hochladen", metavar="IP", help="danach per tools/sd_upload.py auf die Remote laden")
    a = ap.parse_args()

    im = ImageOps.exif_transpose(Image.open(a.bild)).convert("RGB")
    if a.einpassen:
        im = ImageOps.pad(im, (W, H), color=hex_rgb(a.rand), method=Image.LANCZOS)
    else:
        im = ImageOps.fit(im, (W, H), method=Image.LANCZOS)
    if a.farben:
        im = im.quantize(colors=max(2, min(256, a.farben)), dither=Image.Dither.NONE).convert("RGB")

    px = rgb565(im)
    rle = rle16(px)
    roh = struct.pack("<%dH" % len(px), *px)
    ziel = HERE / "sd-card" / "themes" / slug(a.skin)
    ziel.mkdir(parents=True, exist_ok=True)
    for alt in ("background.rle", "background.rgb565"):
        (ziel / alt).unlink(missing_ok=True)
    if len(rle) < len(roh):
        datei, daten = ziel / "background.rle", rle
    else:
        datei, daten = ziel / "background.rgb565", roh
    datei.write_bytes(daten)
    im.save(ziel / "background_preview.png")
    sd_pfad = f"/themes/{slug(a.skin)}/{datei.name}"
    print(f"{datei.relative_to(HERE)}: {len(daten) / 1024:.0f} KB  ->  SD-Karte {sd_pfad}")
    if datei.suffix == ".rgb565":
        print("  (Foto: roh ist kleiner als RLE. Mit --farben 32 wird es meist deutlich kleiner und laedt schneller.)")
    if a.hochladen:
        return subprocess.call([sys.executable, str(HERE / "tools" / "sd_upload.py"), a.hochladen, str(datei), sd_pfad])
    return 0


if __name__ == "__main__":
    sys.exit(main())
