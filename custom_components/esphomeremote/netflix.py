"""NOIR Netflix-dashboard (2026-11-22): Top 10 in Nederland, Populair op Netflix, posters voor de remote.

Bronnen
- Top 10: de officiele weeklijst van Netflix (Tudum, all-weeks-countries.tsv), elke dinsdag nieuw. Alleen titels.
  Het bestand is groot (alle landen sinds 2021): het wordt gestreamd, alleen land + categorie TV blijven over, en
  alleen opnieuw gehaald als de bewaarde week ouder is dan 8 dagen. Bewaard in .storage/esphomeremote_netflix.json.
- Populair op Netflix: TMDB discover (provider 8 = Netflix, regio NL), dagelijks. Populariteit volgens TMDB.
- Posters, achtergronden, Nederlandse titels/teksten, genres, seizoenen, leeftijd: TMDB (sleutel in de opties van
  de integratie). Zonder sleutel: alleen de Top 10-titels, geen posters. "This product uses the TMDB API but is not
  endorsed or certified by TMDB."

Voor de remote (geen HA-login, de gedeelde sleutel ?k=):
  GET /api/esphomeremote/netflix?k=..        {"w": "9-15 nov", "t": [titel...], "p": [titel...]}  (&f=tsv: tekstregels)
      titel = {"i": tmdb-id, "n": naam, "z": zoeknaam, "j": jaar, "g": genre, "s": seizoenen, "l": leeftijd, "o": tekst}
  GET /api/esphomeremote/netflix/img?k=..&id=<tmdb>&s=p|b
      ruwe RGB565 (little endian) - p: poster 64x96, b: achtergrond 216x110 (onderaan uitlopend naar NOIR-zwart)
"""
from __future__ import annotations

import asyncio
import io
import json
import logging
import time
from collections import OrderedDict
from datetime import date, timedelta

from aiohttp import web

from homeassistant.components.http import HomeAssistantView
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.event import async_track_time_interval

_LOGGER = logging.getLogger(__name__)

TUDUM = "https://www.netflix.com/tudum/top10/data/all-weeks-countries.tsv"
TMDB = "https://api.themoviedb.org/3"
TMDB_IMG = "https://image.tmdb.org/t/p/"
NETFLIX_PROVIDER = 8
LAND = "NL"
TAAL = "nl-NL"
UA = {"User-Agent": "Mozilla/5.0 (HomeAssistant esphomeremote)"}
MAANDEN = ["jan", "feb", "mrt", "apr", "mei", "jun", "jul", "aug", "sep", "okt", "nov", "dec"]
POSTER = (64, 96)
ACHTERGROND = (216, 110)
NOIR_BG = (14, 15, 18)


def week_tekst(einde: str) -> str:
    """'2026-11-15' (zondag) -> '9-15 nov' (of '27 okt-2 nov')."""
    try:
        z = date.fromisoformat(einde)
    except ValueError:
        return einde
    m = z - timedelta(days=6)
    if m.month == z.month:
        return f"{m.day}–{z.day} {MAANDEN[z.month - 1]}"
    return f"{m.day} {MAANDEN[m.month - 1]}–{z.day} {MAANDEN[z.month - 1]}"


def rgb565(img) -> bytes:
    """PIL-RGB -> RGB565 little endian (zoals LVGL het verwacht)."""
    out = bytearray(img.width * img.height * 2)
    i = 0
    for r, g, b in img.getdata():
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out[i] = v & 0xFF
        out[i + 1] = v >> 8
        i += 2
    return bytes(out)


def _omzetten(data: bytes, soort: str) -> bytes:
    """Executor: TMDB-JPEG -> vaste maat, randloos bijgesneden, als RGB565."""
    from PIL import Image, ImageDraw
    src = Image.open(io.BytesIO(data)).convert("RGB")
    bw, bh = POSTER if soort == "p" else ACHTERGROND
    r = src.width / src.height
    w, h = (round(bh * r), bh) if r > bw / bh else (bw, round(bw / r))
    l, t = (w - bw) // 2, (h - bh) // 2
    img = src.resize((w, h), Image.LANCZOS).crop((l, t, l + bw, t + bh))
    if soort == "b":   # onderste helft loopt uit naar de NOIR-achtergrond (titel staat erover)
        masker = Image.new("L", (bw, bh), 0)
        d = ImageDraw.Draw(masker)
        for y in range(bh):
            d.line([(0, y), (bw - 1, y)], fill=int(255 * min(1.0, max(0.0, (y - bh * 0.4) / (bh * 0.6)))))
        img = Image.composite(Image.new("RGB", (bw, bh), NOIR_BG), img, masker)
    return rgb565(img)


class NetflixData:
    def __init__(self, hass: HomeAssistant) -> None:
        self.hass = hass
        self.sleutel = ""
        self.week = ""                      # einddatum van de Top 10-week (zondag)
        self.top10: list[dict] = []
        self.populair: list[dict] = []
        self.pop_tijd = 0.0
        self.genres: dict[int, str] = {}
        self.paden: dict[int, tuple[str, str]] = {}   # tmdb-id -> (poster_path, backdrop_path)
        self.beelden: OrderedDict[str, bytes] = OrderedDict()   # "id:p" -> RGB565 (LRU)
        self._sperre = asyncio.Lock()
        self._pad = hass.config.path(".storage", "esphomeremote_netflix.json")
        self._stop = None

    # ---------------------------------------------------------------- opslag
    def _laden(self) -> None:
        try:
            with open(self._pad, encoding="utf-8") as f:
                d = json.load(f)
        except (OSError, ValueError):
            return
        self.week = d.get("week", "")
        self.top10 = d.get("top10", [])
        self.populair = d.get("populair", [])
        self.pop_tijd = d.get("pop_tijd", 0.0)
        self.paden = {int(k): tuple(v) for k, v in d.get("paden", {}).items()}

    def _bewaren(self) -> None:
        d = {"week": self.week, "top10": self.top10, "populair": self.populair, "pop_tijd": self.pop_tijd,
             "paden": {str(k): list(v) for k, v in self.paden.items()}}
        tmp = self._pad + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(d, f, ensure_ascii=False)
        import os
        os.replace(tmp, self._pad)

    async def starten(self, sleutel: str) -> None:
        self.sleutel = (sleutel or "").strip()
        await self.hass.async_add_executor_job(self._laden)
        if self._stop:
            self._stop()
        self._stop = async_track_time_interval(self.hass, lambda _now: self.hass.async_create_task(self.verversen()),
                                               timedelta(hours=6))
        self.hass.async_create_task(self.verversen())

    # ---------------------------------------------------------------- TMDB
    async def _tmdb(self, pad: str, **params) -> dict | None:
        if not self.sleutel:
            return None
        sessie = async_get_clientsession(self.hass)
        kop = {}
        if len(self.sleutel) > 40:   # v4 "API Read Access Token"
            kop["Authorization"] = f"Bearer {self.sleutel}"
        else:                        # v3 "API Key"
            params["api_key"] = self.sleutel
        try:
            async with sessie.get(f"{TMDB}{pad}", params=params, headers=kop, timeout=15) as r:
                if r.status != 200:
                    _LOGGER.warning("TMDB %s: HTTP %s", pad, r.status)
                    return None
                return await r.json()
        except (asyncio.TimeoutError, OSError) as err:
            _LOGGER.warning("TMDB %s: %s", pad, err)
            return None

    async def _genres(self) -> None:
        if self.genres:
            return
        d = await self._tmdb("/genre/tv/list", language=TAAL)
        self.genres = {g["id"]: g["name"] for g in (d or {}).get("genres", [])}

    async def _details(self, tmdb_id: int, zoeknaam: str) -> dict:
        """Titel uit TMDB: Nederlandse naam en tekst, jaar, genre, seizoenen, Kijkwijzer-leeftijd, beeldpaden."""
        d = await self._tmdb(f"/tv/{tmdb_id}", language=TAAL, append_to_response="content_ratings")
        if not d:
            return {"i": tmdb_id, "n": zoeknaam, "z": zoeknaam}
        leeftijd = ""
        for c in (d.get("content_ratings") or {}).get("results", []):
            if c.get("iso_3166_1") == LAND:
                leeftijd = str(c.get("rating") or "")
        if leeftijd and leeftijd.isdigit():
            leeftijd += "+"
        if leeftijd in ("AL", "AL+"):
            leeftijd = "AL"
        self.paden[tmdb_id] = (d.get("poster_path") or "", d.get("backdrop_path") or "")
        return {"i": tmdb_id, "n": (d.get("name") or zoeknaam)[:40], "z": zoeknaam[:60],
                "j": (d.get("first_air_date") or "")[:4],
                "g": ", ".join(g["name"] for g in (d.get("genres") or [])[:2])[:30],
                "s": int(d.get("number_of_seasons") or 0), "l": leeftijd[:4],
                "o": (d.get("overview") or "")[:300]}

    async def _zoeken(self, naam: str) -> int:
        d = await self._tmdb("/search/tv", query=naam, language=TAAL)
        res = (d or {}).get("results", [])
        # liefst een exacte (originele) naam, anders de populairste treffer
        for r in res:
            if naam.lower() in ((r.get("name") or "").lower(), (r.get("original_name") or "").lower()):
                return int(r["id"])
        return int(res[0]["id"]) if res else 0

    # ---------------------------------------------------------------- Tudum
    async def _tudum(self) -> tuple[str, list[str]] | None:
        """Laatste week + titels (plaats 1-10) van de Nederlandse TV-lijst, regel voor regel gelezen."""
        sessie = async_get_clientsession(self.hass)
        week, titels = "", {}
        try:
            async with sessie.get(TUDUM, headers=UA, timeout=300) as r:
                if r.status != 200:
                    _LOGGER.warning("Netflix Top 10: HTTP %s", r.status)
                    return None
                kop = None
                rest = b""
                async for blok in r.content.iter_chunked(65536):
                    regels = (rest + blok).split(b"\n")
                    rest = regels.pop()
                    for regel in regels:
                        velden = regel.decode("utf-8", "replace").rstrip("\r").split("\t")
                        if kop is None:
                            kop = {n: i for i, n in enumerate(velden)}
                            continue
                        try:
                            if velden[kop["country_iso2"]] != LAND or velden[kop["category"]] != "TV":
                                continue
                            w, plek, titel = velden[kop["week"]], int(velden[kop["weekly_rank"]]), velden[kop["show_title"]]
                        except (KeyError, IndexError, ValueError):
                            continue
                        if w > week:
                            week, titels = w, {}
                        if w == week:
                            titels[plek] = titel
        except (asyncio.TimeoutError, OSError) as err:
            _LOGGER.warning("Netflix Top 10: %s", err)
            return None
        if not week:
            return None
        return week, [titels[k] for k in sorted(titels)][:10]

    # ---------------------------------------------------------------- verversen
    async def verversen(self) -> None:
        async with self._sperre:
            veranderd = False
            oud = True
            try:
                oud = (date.today() - date.fromisoformat(self.week)).days > 8
            except ValueError:
                pass
            if oud or not self.top10:
                res = await self._tudum()
                if res:
                    week, titels = res
                    lijst = []
                    for t in titels:
                        i = await self._zoeken(t) if self.sleutel else 0
                        lijst.append(await self._details(i, t) if i else {"i": 0, "n": t[:40], "z": t[:60]})
                    self.week, self.top10, veranderd = week, lijst, True
                    _LOGGER.info("Netflix Top 10 NL, week tot %s: %s", week, ", ".join(titels))
            if self.sleutel and time.time() - self.pop_tijd > 20 * 3600:
                await self._genres()
                d = await self._tmdb("/discover/tv", with_watch_providers=NETFLIX_PROVIDER, watch_region=LAND,
                                     sort_by="popularity.desc", language=TAAL, page=1)
                if d:
                    lijst = []
                    for r in d.get("results", [])[:10]:
                        lijst.append(await self._details(int(r["id"]), r.get("original_name") or r.get("name") or ""))
                    self.populair, self.pop_tijd, veranderd = lijst, time.time(), True
            if veranderd:
                await self.hass.async_add_executor_job(self._bewaren)

    # ---------------------------------------------------------------- beelden
    async def beeld(self, tmdb_id: int, soort: str) -> bytes | None:
        sleutel = f"{tmdb_id}:{soort}"
        if sleutel in self.beelden:
            self.beelden.move_to_end(sleutel)
            return self.beelden[sleutel]
        paden = self.paden.get(tmdb_id)
        pad = (paden[0] if soort == "p" else paden[1]) if paden else ""
        if not pad:
            return None
        sessie = async_get_clientsession(self.hass)
        try:
            async with sessie.get(f"{TMDB_IMG}{'w154' if soort == 'p' else 'w300'}{pad}", timeout=15) as r:
                if r.status != 200:
                    return None
                data = await r.read()
        except (asyncio.TimeoutError, OSError):
            return None
        raw = await self.hass.async_add_executor_job(_omzetten, data, soort)
        self.beelden[sleutel] = raw
        while len(self.beelden) > 80:
            self.beelden.popitem(last=False)
        return raw

    def tsv(self) -> bytes:
        """Voor de remote: regels zonder JSON (zie esphomeremote-noir.h nf::klaar_)."""
        schoon = lambda v: str(v or "").replace("\t", " ").replace("\n", " ").replace("\r", " ")  # noqa: E731
        regels = [f"w\t{schoon(week_tekst(self.week) if self.week else '')}"]
        for soort, lijst in (("t", self.top10), ("p", self.populair)):
            for t in lijst:
                regels.append("\t".join([soort, str(t.get("i", 0))] + [schoon(t.get(k)) for k in ("n", "z", "j", "g", "s", "l", "o")]))
        return ("\n".join(regels) + "\n").encode()

    def json(self) -> bytes:
        return json.dumps({"w": week_tekst(self.week) if self.week else "", "t": self.top10, "p": self.populair},
                          ensure_ascii=False, separators=(",", ":")).encode()


class NetflixView(HomeAssistantView):
    url = "/api/esphomeremote/netflix"
    name = "api:esphomeremote:netflix"
    requires_auth = False

    def __init__(self, nd: NetflixData, sleutel_ok) -> None:
        self.nd = nd
        self.ok = sleutel_ok

    async def get(self, request: web.Request) -> web.Response:
        if not self.ok(request):
            return web.Response(status=403)
        if request.query.get("f") == "tsv":
            return web.Response(body=self.nd.tsv(), content_type="text/plain", charset="utf-8", headers={"Cache-Control": "no-store"})
        return web.Response(body=self.nd.json(), content_type="application/json", headers={"Cache-Control": "no-store"})


class NetflixBeeldView(HomeAssistantView):
    url = "/api/esphomeremote/netflix/img"
    name = "api:esphomeremote:netflix:img"
    requires_auth = False

    def __init__(self, nd: NetflixData, sleutel_ok) -> None:
        self.nd = nd
        self.ok = sleutel_ok

    async def get(self, request: web.Request) -> web.Response:
        if not self.ok(request):
            return web.Response(status=403)
        try:
            i = int(request.query.get("id", "0"))
        except ValueError:
            return web.Response(status=400)
        soort = "b" if request.query.get("s") == "b" else "p"
        raw = await self.nd.beeld(i, soort) if i else None
        if raw is None:
            return web.Response(status=404)
        return web.Response(body=raw, content_type="application/octet-stream", headers={"Cache-Control": "max-age=86400"})
