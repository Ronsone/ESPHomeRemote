"""NOIR Netflix (2026-11-22): app op een LG webOS-tv starten, optioneel direct een titel (deep link).

Dienst esphomeremote.tv_app: app (bv. netflix, youtube.leanback.v4), content (optioneel), entity_id (optioneel).
De ESPHome-actie van de remote kan alleen teksten sturen; webostv.command wil een payload-dict - die bouwt deze
dienst. Zonder entity_id: de eerste media_player van de integratie webostv.

Netflix: content = het nummer van de titel (uit netflix.com/title/80057281 -> 80057281) of de hele URL; wordt
contentId "m=https://www.netflix.com/title/<nummer>&source_type=4" (opent de titelpagina in de app).
Andere apps: content gaat ongewijzigd als contentId mee.
search (alleen Netflix, zonder content): opent de zoekpagina met die naam - het Netflix-dashboard kent geen
Netflix-nummers (Tudum en TMDB leveren ze niet).
"""
from __future__ import annotations

import logging
import re

import voluptuous as vol

from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers import entity_registry as er

_LOGGER = logging.getLogger(__name__)
DOMAIN = "esphomeremote"

SCHEMA = vol.Schema({vol.Required("app"): str, vol.Optional("content", default=""): str, vol.Optional("search", default=""): str,
                     vol.Optional("entity_id", default=""): str}, extra=vol.ALLOW_EXTRA)


def netflix_content(c: str) -> str:
    c = c.strip()
    if not c:
        return ""
    if c.startswith("m="):
        return c
    m = re.search(r"(?:title|watch)/(\d+)", c) or re.fullmatch(r"(\d+)", c)
    if m:
        return f"m=https://www.netflix.com/title/{m.group(1)}&source_type=4"
    return f"m={c}&source_type=4" if c.startswith("http") else c


def registrieren(hass: HomeAssistant) -> None:
    async def dienst(call: ServiceCall) -> None:
        ent = (call.data.get("entity_id") or "").strip()
        if not ent:
            reg = er.async_get(hass)
            ent = next((e.entity_id for e in reg.entities.values()
                        if e.platform == "webostv" and e.domain == "media_player"), "")
        if not ent:
            _LOGGER.warning("tv_app: geen webOS-tv gevonden (entity_id meegeven)")
            return
        app = call.data["app"].strip()
        content = call.data.get("content", "")
        zoek = (call.data.get("search") or "").strip()
        if app == "netflix" and zoek and not content.strip():   # NOIR Netflix-dashboard: titel op naam (zoekpagina)
            from urllib.parse import quote
            content = f"m=https://www.netflix.com/search?q={quote(zoek)}&source_type=4"
        elif app == "netflix":
            content = netflix_content(content)
        payload = {"id": app}
        if content.strip():
            payload["contentId"] = content.strip()
        _LOGGER.debug("tv_app: %s %s", ent, payload)
        await hass.services.async_call("webostv", "command",
                                       {"entity_id": ent, "command": "system.launcher/launch", "payload": payload},
                                       blocking=False)

    hass.services.async_register(DOMAIN, "tv_app", dienst, schema=SCHEMA)
