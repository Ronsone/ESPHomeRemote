"""Aktivitaet als Auswahl (Dropdown): je Remote ein select mit allen Aktivitaeten + "Aus" (2026-10-18, siehe dock.py).

Gleiche Logik wie die Aktivitaets-Schalter (switch.py): Zustand aus dem Sensor "Aktivität" der Remote, Setzen ueber
den ESPHome-Dienst <remote>_aktivitaet_setzen; schlaeft die Remote, wird beim naechsten Verbinden zugestellt.
"""

from __future__ import annotations

from typing import Any

from homeassistant.components.select import SelectEntity

from .dock import DOMAIN, Verwalter
from .switch import AktBasis, _remote_name
from .i18n import t as _t

AUS = "Aus"


async def async_setup_platform(hass, config, add, discovery_info=None) -> None:
    if discovery_info is not None:
        hass.data[DOMAIN + "_dock"].plattform("select", add)


class AktAuswahl(AktBasis, SelectEntity):
    _attr_icon = "mdi:remote-tv"

    def __init__(self, v: Verwalter, uid: str, dev: str, namen: list[str]) -> None:
        super().__init__(v, uid, dev, -1)
        self._namen = namen
        self._attr_name = f"{_remote_name(dev)} {_t(None, 'Aktivität')}"
        self._attr_options = [*namen, AUS]

    @property
    def current_option(self) -> str | None:
        nr = self._wunsch if self._wunsch is not None else self._laufend()
        if nr is None:
            return None
        return self._namen[nr] if 0 <= nr < len(self._namen) else AUS

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        return {"remote": self.dev, "wartet_auf_remote": self._wunsch is not None}

    async def async_select_option(self, option: str) -> None:
        await self._setzen(self._namen.index(option) if option in self._namen else -1)


# ---- Skin (2026-11-19, esphomeremote-skin.h) ----------------------------------------------------------------------
# Eingebaute Skins der Firmware (Nummer = Index) - gleiche Reihenfolge wie skin::FESTE
SKINS_FEST = ["Standard", "Hell", "OLED", "Nord", "Retro", "Synthwave", "NOIR"]
SKIN_EIGENE_AB = 10


def skin_optionen(cfg: dict) -> list[tuple[int, str]]:
    """[(skin_nr, Anzeigename)] - eingebaute, dann eigene aus cfg["skins"] (Namen eindeutig gemacht)."""
    out = list(enumerate(SKINS_FEST))
    namen = set(SKINS_FEST)
    for i, sk in enumerate((cfg.get("skins") or [])[:8]):
        n = str(sk.get("name") or f"Skin {i + 1}")
        while n in namen:
            n += " *"
        namen.add(n)
        out.append((SKIN_EIGENE_AB + i, n))
    return out


class SkinAuswahl(SelectEntity):
    """Skin einer Remote: setzt die Einstellung skin_nr (geht wie jede Einstellung an die Remote, auch spaeter, wenn
    sie gerade schlaeft); an der Remote umgeschaltet (Taste "Skin wechseln") meldet sie es selbst zurueck."""
    _attr_should_poll = False
    _attr_icon = "mdi:palette"

    def __init__(self, v: Verwalter, uid: str, dev: str, optionen: list[tuple[int, str]]) -> None:
        self.v = v
        self.dev = dev
        self._opt = optionen
        self._attr_unique_id = uid
        self._attr_name = f"{_remote_name(dev)} Skin"
        self._attr_options = [n for _, n in optionen]

    async def async_added_to_hass(self) -> None:
        from .dock import DOMAIN as _D

        def _neu(ev) -> None:
            if ev.data.get("device") == self.dev:
                self.async_write_ha_state()
        self.async_on_remove(self.hass.bus.async_listen(_D + "_einst", _neu))

    @property
    def current_option(self) -> str | None:
        werte, _ = self.v.kd.einst_werte(self.dev)
        nr = werte.get("skin_nr")
        if nr is None:
            return None   # Firmware ohne Skins
        return next((n for k, n in self._opt if k == nr), self._opt[0][1])

    @property
    def available(self) -> bool:
        return "skin_nr" in self.v.kd.einst_werte(self.dev)[0]

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        werte, offen = self.v.kd.einst_werte(self.dev)
        return {"remote": self.dev, "skin_nr": werte.get("skin_nr"), "wartet_auf_remote": "skin_nr" in offen}

    async def async_select_option(self, option: str) -> None:
        nr = next((k for k, n in self._opt if n == option), 0)
        await self.v.kd.einst_setzen(self.dev, {"skin_nr": nr})
        self.async_write_ha_state()
