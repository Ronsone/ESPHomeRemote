"""NOIR Muziek (2026-11-25): lijsten, zoeken en hoesjes uit Music Assistant voor de remote.

Alle muziek gaat via Music Assistant (Spotify, radio, eigen muziek ... als bronnen in MA). De remote kiest een
MA-speler (NOIR Builder > Audio > Music) en speelt af met media_player.play_media op die speler.

GET /api/esphomeremote/muziek?k=<sleutel>&e=<MA-speler>
    Tekstregels (TSV, geen JSON op de remote):
      p\t<naam>\t<sub>\t<uri>\t<media_type>   playlists uit de bibliotheek
      r\t<naam>\t<sub>\t<uri>\t<media_type>   laatst gespeelde nummers
GET /api/esphomeremote/muziek/zoek?k=<sleutel>&e=<speler>&q=<tekst>     "z\t<naam>\t<sub>\t<uri>\t<media_type>"
GET /api/esphomeremote/muziek/img?k=<sleutel>&e=<speler>&s=c             hoes van wat nu speelt, 96 x 96 RGB565
GET /api/esphomeremote/muziek/img?k=<sleutel>&e=<speler>&s=l&w=p|r|z&i=<n>  hoesje uit een lijst, 64 x 64 RGB565
"""
from __future__ import annotations

import io
import logging
import time
from collections import OrderedDict

from aiohttp import web

from homeassistant.components.http import HomeAssistantView
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .netflix import rgb565

_LOGGER = logging.getLogger(__name__)

LIJST_TTL = 600            # s
MAX_PER_RIJ = 12
MA_DOMEINEN = ("music_assistant", "mass")   # mass = oudere HACS-versie van de MA-koppeling
SOORT_NAAM = {"playlist": "Playlist", "album": "Album", "radio": "Radio", "artist": "Artiest"}


def _vierkant(data: bytes, maat: int) -> bytes:
    """Executor: willekeurig beeld -> maat x maat (midden bijgesneden) als RGB565."""
    from PIL import Image
    src = Image.open(io.BytesIO(data)).convert("RGB")
    k = min(src.width, src.height)
    l, t = (src.width - k) // 2, (src.height - k) // 2
    return rgb565(src.crop((l, t, l + k, t + k)).resize((maat, maat), Image.LANCZOS))


def _schoon(t) -> str:
    return str(t or "").replace("\t", " ").replace("\n", " ").strip()[:60]


def _item(x: dict) -> dict | None:
    """MA-item -> {naam, sub, id, type, thumb}."""
    if not x or not x.get("uri"):
        return None
    artiesten = ", ".join(a.get("name", "") for a in x.get("artists") or [] if isinstance(a, dict))
    beeld = x.get("image")
    if isinstance(beeld, dict):
        beeld = beeld.get("path") or beeld.get("url")
    return {"naam": _schoon(x.get("name")), "sub": _schoon(artiesten or SOORT_NAAM.get(x.get("media_type", ""), "")),
            "id": x["uri"], "type": x.get("media_type") or "track",
            "thumb": beeld if isinstance(beeld, str) and beeld.startswith(("http", "/")) else None}


def _tsv(soort: str, rij: list) -> list[str]:
    return [f"{soort}\t{x['naam']}\t{x['sub']}\t{x['id']}\t{x['type']}" for x in rij]


class MuziekData:
    def __init__(self, hass: HomeAssistant) -> None:
        self.hass = hass
        self.lijsten: dict[str, tuple[float, list, list]] = {}   # speler -> (tijd, playlists, recent)
        self.zoek: dict[str, list] = {}                          # speler -> laatste zoekresultaten
        self.beelden: OrderedDict[str, bytes] = OrderedDict()
        self.domein = "music_assistant"   # service-domein van de gevonden MA-koppeling

    def _entry(self, eid: str) -> str | None:
        """config_entry_id van Music Assistant voor deze speler (of de eerste MA-koppeling)."""
        try:
            from homeassistant.helpers import entity_registry as er
            reg = er.async_get(self.hass).async_get(eid)
        except Exception:   # noqa: BLE001
            reg = None
        platform = reg.platform if reg is not None else None
        if platform in MA_DOMEINEN and reg.config_entry_id:
            self.domein = platform
            return reg.config_entry_id
        entries = [e for d in MA_DOMEINEN for e in self.hass.config_entries.async_entries(d)]
        if entries:
            _LOGGER.warning("%s is geen Music Assistant-speler (platform %s) - kies een MA-speler in de NOIR Builder (Audio > Music)", eid, platform)
            self.domein = entries[0].domain
            return entries[0].entry_id
        _LOGGER.warning("Geen Music Assistant-koppeling gevonden (speler %s, platform %s; koppelingen: %s)", eid, platform,
                        sorted({e.domain for e in self.hass.config_entries.async_entries()} & {"music_assistant", "mass", "cast", "kef"}))
        return None

    async def _ma(self, dienst: str, data: dict):
        try:
            return await self.hass.services.async_call(self.domein, dienst, data, blocking=True, return_response=True)
        except Exception as err:   # noqa: BLE001
            _LOGGER.info("Music Assistant %s mislukt: %s", dienst, err)
            return None

    async def lijst(self, eid: str):
        oud = self.lijsten.get(eid)
        if oud is not None and time.monotonic() - oud[0] < LIJST_TTL:
            return oud[1], oud[2]
        entry = self._entry(eid)
        if entry is None:
            return [], []
        uit = lambda d: [i for i in (_item(x) for x in ((d or {}).get("items") or [])) if i][:MAX_PER_RIJ]
        playlists = uit(await self._ma("get_library", {"config_entry_id": entry, "media_type": "playlist", "limit": MAX_PER_RIJ}))
        recent = uit(await self._ma("get_library", {"config_entry_id": entry, "media_type": "track", "limit": MAX_PER_RIJ, "order_by": "last_played"}))
        _LOGGER.info("Muziek %s: %d playlists, %d recent", eid, len(playlists), len(recent))
        if playlists or recent:
            self.lijsten[eid] = (time.monotonic(), playlists, recent)
        return playlists, recent

    async def tsv(self, eid: str) -> str:
        playlists, recent = await self.lijst(eid)
        return "\n".join(_tsv("p", playlists) + _tsv("r", recent)) + "\n"

    async def zoeken(self, eid: str, q: str) -> str:
        q, res = q.strip()[:60], []
        entry = self._entry(eid) if q else None
        if entry:
            d = await self._ma("search", {"config_entry_id": entry, "name": q, "limit": 5,
                                          "media_type": ["track", "playlist", "album", "artist", "radio"]})
            for groep in ("tracks", "playlists", "albums", "radio", "artists"):
                res += [i for i in (_item(x) for x in (d or {}).get(groep) or []) if i]
            res = res[:10]
            _LOGGER.info("Muziek zoeken '%s': %d resultaten", q, len(res))
        self.zoek[eid] = res
        return "\n".join(_tsv("z", res)) + "\n"

    def _bewaar(self, sleutel: str, raw: bytes) -> None:
        self.beelden[sleutel] = raw
        while len(self.beelden) > 40:
            self.beelden.popitem(last=False)

    async def hoes(self, eid: str) -> bytes | None:
        """Hoes van wat nu speelt (media_image van de speler)."""
        st = self.hass.states.get(eid)
        sleutel = f"c|{eid}|{st.attributes.get('entity_picture') if st else ''}"
        if sleutel in self.beelden:
            return self.beelden[sleutel]
        comp = self.hass.data.get("media_player")   # EntityComponent
        ent = comp.get_entity(eid) if comp is not None else None
        if ent is None:
            return None
        try:
            data, _type = await ent.async_get_media_image()
        except Exception as err:   # noqa: BLE001
            _LOGGER.info("Hoes %s mislukt: %s", eid, err)
            return None
        if not data:
            return None
        raw = await self.hass.async_add_executor_job(_vierkant, data, 96)
        self._bewaar(sleutel, raw)
        return raw

    async def tegel(self, eid: str, welke: str, i: int) -> bytes | None:
        if welke == "z":
            rij = self.zoek.get(eid, [])
        else:
            playlists, recent = await self.lijst(eid)
            rij = playlists if welke == "p" else recent
        if i < 0 or i >= len(rij) or not rij[i].get("thumb"):
            return None
        url = rij[i]["thumb"]
        sleutel = f"l|{url}"
        if sleutel in self.beelden:
            return self.beelden[sleutel]
        try:
            if url.startswith("/"):   # via HA zelf (media proxy)
                from homeassistant.helpers.network import get_url
                url = get_url(self.hass, prefer_external=False) + url
            async with async_get_clientsession(self.hass).get(url, timeout=10) as r:
                if r.status != 200:
                    return None
                data = await r.read()
        except Exception as err:   # noqa: BLE001
            _LOGGER.info("Hoesje %s mislukt: %s", url, err)
            return None
        raw = await self.hass.async_add_executor_job(_vierkant, data, 64)
        self._bewaar(sleutel, raw)
        return raw


class _Basis(HomeAssistantView):
    requires_auth = False

    def __init__(self, md: MuziekData, sleutel_ok) -> None:
        self.md = md
        self.ok = sleutel_ok

    def _speler(self, request: web.Request) -> str | None:
        if not self.ok(request):
            return None
        eid = request.query.get("e", "")
        return eid if eid.startswith("media_player.") else None

    @staticmethod
    def _tekst(body: str) -> web.Response:
        return web.Response(body=body, content_type="text/plain", charset="utf-8", headers={"Cache-Control": "no-store"})


class MuziekView(_Basis):
    url = "/api/esphomeremote/muziek"
    name = "api:esphomeremote:muziek"

    async def get(self, request: web.Request) -> web.Response:
        eid = self._speler(request)
        return self._tekst(await self.md.tsv(eid)) if eid else web.Response(status=403)


class MuziekZoekView(_Basis):
    url = "/api/esphomeremote/muziek/zoek"
    name = "api:esphomeremote:muziek:zoek"

    async def get(self, request: web.Request) -> web.Response:
        eid = self._speler(request)
        return self._tekst(await self.md.zoeken(eid, request.query.get("q", ""))) if eid else web.Response(status=403)


class MuziekBeeldView(_Basis):
    url = "/api/esphomeremote/muziek/img"
    name = "api:esphomeremote:muziek:img"

    async def get(self, request: web.Request) -> web.Response:
        eid = self._speler(request)
        if not eid:
            return web.Response(status=403)
        if request.query.get("s") == "l":
            try:
                i = int(request.query.get("i", "-1"))
            except ValueError:
                return web.Response(status=400)
            w = request.query.get("w", "p")
            raw = await self.md.tegel(eid, w if w in ("p", "r", "z") else "p", i)
        else:
            raw = await self.md.hoes(eid)
        if raw is None:
            return web.Response(status=404)
        return web.Response(body=raw, content_type="application/octet-stream", headers={"Cache-Control": "no-store"})
