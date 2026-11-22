"""NOIR Audio (2026-11-22): Spotify-lijsten en hoesjes voor de remote.

GET /api/esphomeremote/spotify?k=<sleutel>&e=<media_player van Spotify>
    Tekstregels (TSV, geen JSON op de remote):
      p\t<naam>\t<sub>\t<media_content_id>\t<media_content_type>   jouw playlists
      r\t<naam>\t<sub>\t<media_content_id>\t<media_content_type>   recent gespeeld
      d\t<apparaat>                                                 Spotify Connect-apparaten (source_list)
GET /api/esphomeremote/spotify/zoek?k=<sleutel>&e=<entity>&q=<tekst>  zoeken: "z\t<titel>\t<artiest>\t<uri>\ttrack"
GET /api/esphomeremote/spotify/img?k=<sleutel>&e=<entity>&s=c           hoes van wat nu speelt, 96 x 96 RGB565
GET /api/esphomeremote/spotify/img?k=<sleutel>&e=<entity>&s=l&w=p|r|z&i=<n> hoesje uit een lijst, 64 x 64 RGB565

Lijsten en zoeken gaan via de Spotify Web API met het token van de officiele Spotify-integratie (dezelfde
koppeling, geen eigen sleutels). Lukt dat niet, dan vallen playlists/recent terug op async_browse_media van de
entiteit. Afspelen: media_player.play_media met media_content_id/-type uit deze lijsten.
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
NOIR_BG = (14, 15, 18)


def _vierkant(data: bytes, maat: int) -> bytes:
    """Executor: willekeurig beeld -> maat x maat (midden bijgesneden) als RGB565."""
    from PIL import Image
    src = Image.open(io.BytesIO(data)).convert("RGB")
    k = min(src.width, src.height)
    l, t = (src.width - k) // 2, (src.height - k) // 2
    img = src.crop((l, t, l + k, t + k)).resize((maat, maat), Image.LANCZOS)
    return rgb565(img)


def _schoon(t) -> str:
    return str(t or "").replace("\t", " ").replace("\n", " ").strip()[:60]


API = "https://api.spotify.com/v1"


def _beeld_url(images) -> str | None:
    """Kleinste afbeelding van minstens 64 px (Spotify geeft ze van groot naar klein)."""
    goed = None
    for im in images or []:
        if (im.get("width") or 640) >= 64:
            goed = im.get("url")
    return goed or ((images or [{}])[0].get("url") if images else None)


class SpotifyData:
    def __init__(self, hass: HomeAssistant) -> None:
        self.hass = hass
        self.lijsten: dict[str, tuple[float, list, list]] = {}   # entity -> (tijd, playlists, recent)
        self.zoek: dict[str, list] = {}                          # entity -> laatste zoekresultaten
        self.beelden: OrderedDict[str, bytes] = OrderedDict()

    async def _token(self, eid: str) -> str | None:
        """Access token van de Spotify-integratie waar deze entiteit bij hoort (vernieuwt hem zo nodig)."""
        try:
            from homeassistant.helpers import config_entry_oauth2_flow, entity_registry as er
            reg = er.async_get(self.hass).async_get(eid)
            entry = self.hass.config_entries.async_get_entry(reg.config_entry_id) if reg and reg.config_entry_id else None
            if entry is None or entry.domain != "spotify":
                entries = self.hass.config_entries.async_entries("spotify")
                entry = entries[0] if entries else None
            if entry is None:
                return None
            impl = await config_entry_oauth2_flow.async_get_config_entry_implementation(self.hass, entry)
            sess = config_entry_oauth2_flow.OAuth2Session(self.hass, entry, impl)
            await sess.async_ensure_token_valid()
            return sess.token.get("access_token")
        except Exception as err:   # noqa: BLE001
            _LOGGER.info("Spotify-token voor %s niet beschikbaar: %s", eid, err)
            return None

    async def _api(self, eid: str, pad: str, params: dict | None = None):
        token = await self._token(eid)
        if not token:
            return None
        try:
            async with async_get_clientsession(self.hass).get(
                    API + pad, params=params, headers={"Authorization": f"Bearer {token}"}, timeout=10) as r:
                if r.status != 200:
                    _LOGGER.info("Spotify API %s: HTTP %s", pad, r.status)
                    return None
                return await r.json()
        except Exception as err:   # noqa: BLE001
            _LOGGER.info("Spotify API %s mislukt: %s", pad, err)
            return None

    async def _api_lijsten(self, eid: str):
        pl = await self._api(eid, "/me/playlists", {"limit": MAX_PER_RIJ})
        rp = await self._api(eid, "/me/player/recently-played", {"limit": 30})
        if pl is None and rp is None:
            return None
        playlists = []
        for x in (pl or {}).get("items") or []:
            if not x:
                continue
            n = (x.get("tracks") or x.get("items") or {}).get("total")
            playlists.append({"naam": _schoon(x.get("name")), "sub": f"{n} nummers" if n else _schoon((x.get("owner") or {}).get("display_name")),
                              "id": x.get("uri", ""), "type": "playlist", "thumb": _beeld_url(x.get("images"))})
        recent, gezien = [], set()
        for it in (rp or {}).get("items") or []:
            t = (it or {}).get("track") or {}
            if not t.get("uri") or t["uri"] in gezien:
                continue
            gezien.add(t["uri"])
            recent.append({"naam": _schoon(t.get("name")), "sub": _schoon(", ".join(a.get("name", "") for a in t.get("artists") or [])),
                           "id": t["uri"], "type": "track", "thumb": _beeld_url((t.get("album") or {}).get("images"))})
            if len(recent) >= MAX_PER_RIJ:
                break
        return playlists[:MAX_PER_RIJ], recent

    async def zoeken(self, eid: str, q: str) -> str:
        q = q.strip()[:60]
        res = []
        if q:
            data = await self._api(eid, "/search", {"q": q, "type": "track", "limit": 10, "market": "from_token"})
            for t in ((data or {}).get("tracks") or {}).get("items") or []:
                if not t or not t.get("uri"):
                    continue
                res.append({"naam": _schoon(t.get("name")), "sub": _schoon(", ".join(a.get("name", "") for a in t.get("artists") or [])),
                            "id": t["uri"], "type": "track", "thumb": _beeld_url((t.get("album") or {}).get("images"))})
            _LOGGER.info("Spotify zoeken '%s': %d resultaten", q, len(res))
        self.zoek[eid] = res
        return "".join(f"z\t{x['naam']}\t{x['sub']}\t{x['id']}\t{x['type']}\n" for x in res) or "\n"

    def _entiteit(self, eid: str):
        comp = self.hass.data.get("media_player")   # EntityComponent (DATA_COMPONENT = HassKey("media_player"))
        return comp.get_entity(eid) if comp is not None else None

    async def _bladeren(self, ent, soort=None, mid=None):
        try:
            return await ent.async_browse_media(soort, mid)
        except Exception as err:   # noqa: BLE001 - elke integratie gooit iets anders
            _LOGGER.info("Spotify bladeren (%s, %s) mislukt: %s", soort, mid, err)
            return None

    async def _categorie(self, ent, root, sleutel: str):
        """Kind van de bibliotheek waarvan type/id op 'sleutel' eindigt (direct of een niveau dieper)."""
        def zoek(node):
            for c in (node.children or []) if node is not None else []:
                if str(c.media_content_type or "").endswith(sleutel) or str(c.media_content_id or "").endswith(sleutel):
                    return c
            return None
        c = zoek(root)
        if c is None and root is not None:
            for k in root.children or []:
                if k.can_expand:
                    c = zoek(await self._bladeren(ent, k.media_content_type, k.media_content_id))
                    if c is not None:
                        break
        if c is None:
            return []
        node = await self._bladeren(ent, c.media_content_type, c.media_content_id)
        uit, gezien = [], set()
        for k in (node.children or []) if node is not None else []:
            naam = _schoon(k.title)
            if not naam or naam in gezien:
                continue
            gezien.add(naam)
            uit.append({"naam": naam, "sub": "", "id": str(k.media_content_id or ""),
                        "type": str(k.media_content_type or ""), "thumb": k.thumbnail})
            if len(uit) >= MAX_PER_RIJ:
                break
        return uit

    async def lijst(self, eid: str):
        oud = self.lijsten.get(eid)
        if oud is not None and time.monotonic() - oud[0] < LIJST_TTL:
            return oud[1], oud[2]
        api = await self._api_lijsten(eid)
        if api is not None:
            playlists, recent = api
            bron = "Web API"
        else:
            ent = self._entiteit(eid)
            if ent is None:
                _LOGGER.info("Spotify-lijst %s: entiteit niet gevonden", eid)
                return [], []
            root = await self._bladeren(ent)
            if root is not None:
                _LOGGER.info("Spotify bladeren %s, bovenste niveau: %s", eid,
                             [(c.title, c.media_content_type, c.media_content_id) for c in root.children or []])
            playlists = await self._categorie(ent, root, "current_user_playlists")
            recent = await self._categorie(ent, root, "current_user_recently_played")
            bron = "bladeren"
        _LOGGER.info("Spotify-lijst %s (%s): %d playlists, %d recent", eid, bron, len(playlists), len(recent))
        if playlists or recent:
            self.lijsten[eid] = (time.monotonic(), playlists, recent)
        return playlists, recent

    async def tsv(self, eid: str) -> str:
        playlists, recent = await self.lijst(eid)
        regels = [f"p\t{x['naam']}\t{x['sub']}\t{x['id']}\t{x['type']}" for x in playlists]
        regels += [f"r\t{x['naam']}\t{x['sub']}\t{x['id']}\t{x['type']}" for x in recent]
        st = self.hass.states.get(eid)
        for d in (st.attributes.get("source_list") or [])[:6] if st is not None else []:
            regels.append(f"d\t{_schoon(d)}")
        return "\n".join(regels) + "\n"

    def _bewaar(self, sleutel: str, raw: bytes) -> None:
        self.beelden[sleutel] = raw
        while len(self.beelden) > 40:
            self.beelden.popitem(last=False)

    async def hoes(self, eid: str) -> bytes | None:
        """Hoes van wat nu speelt (media_image van de entiteit, dus geen eigen internetverzoek nodig)."""
        st = self.hass.states.get(eid)
        sleutel = f"c|{eid}|{st.attributes.get('entity_picture') if st else ''}"
        if sleutel in self.beelden:
            return self.beelden[sleutel]
        ent = self._entiteit(eid)
        if ent is None:
            return None
        try:
            data, _type = await ent.async_get_media_image()
        except Exception as err:   # noqa: BLE001
            _LOGGER.info("Spotify-hoes %s mislukt: %s", eid, err)
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
            _LOGGER.info("Spotify-tegel %s mislukt: %s", url, err)
            return None
        raw = await self.hass.async_add_executor_job(_vierkant, data, 64)
        self._bewaar(sleutel, raw)
        return raw


class SpotifyView(HomeAssistantView):
    url = "/api/esphomeremote/spotify"
    name = "api:esphomeremote:spotify"
    requires_auth = False

    def __init__(self, sd: SpotifyData, sleutel_ok) -> None:
        self.sd = sd
        self.ok = sleutel_ok

    async def get(self, request: web.Request) -> web.Response:
        if not self.ok(request):
            return web.Response(status=403)
        eid = request.query.get("e", "")
        if not eid.startswith("media_player."):
            return web.Response(status=400)
        return web.Response(body=await self.sd.tsv(eid), content_type="text/plain", charset="utf-8",
                            headers={"Cache-Control": "no-store"})


class SpotifyBeeldView(HomeAssistantView):
    url = "/api/esphomeremote/spotify/img"
    name = "api:esphomeremote:spotify:img"
    requires_auth = False

    def __init__(self, sd: SpotifyData, sleutel_ok) -> None:
        self.sd = sd
        self.ok = sleutel_ok

    async def get(self, request: web.Request) -> web.Response:
        if not self.ok(request):
            return web.Response(status=403)
        eid = request.query.get("e", "")
        if not eid.startswith("media_player."):
            return web.Response(status=400)
        if request.query.get("s") == "l":
            try:
                i = int(request.query.get("i", "-1"))
            except ValueError:
                return web.Response(status=400)
            w = request.query.get("w", "p")
            raw = await self.sd.tegel(eid, w if w in ("p", "r", "z") else "p", i)
        else:
            raw = await self.sd.hoes(eid)
        if raw is None:
            return web.Response(status=404)
        return web.Response(body=raw, content_type="application/octet-stream", headers={"Cache-Control": "no-store"})


class SpotifyZoekView(HomeAssistantView):
    url = "/api/esphomeremote/spotify/zoek"
    name = "api:esphomeremote:spotify:zoek"
    requires_auth = False

    def __init__(self, sd: SpotifyData, sleutel_ok) -> None:
        self.sd = sd
        self.ok = sleutel_ok

    async def get(self, request: web.Request) -> web.Response:
        if not self.ok(request):
            return web.Response(status=403)
        eid = request.query.get("e", "")
        if not eid.startswith("media_player."):
            return web.Response(status=400)
        return web.Response(body=await self.sd.zoeken(eid, request.query.get("q", "")), content_type="text/plain",
                            charset="utf-8", headers={"Cache-Control": "no-store"})
