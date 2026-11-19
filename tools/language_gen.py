#!/usr/bin/env python3
"""Uebersetzungstabelle fuer das Display der Remote (2026-11-01, Nutzer: "alles lokalisieren").

Quelle: esphome/language/<code>.json (deutscher Text -> Uebersetzung), aus dem Woerterbuch des Konfigurators plus den
Texten der Firmware. Ausgabe: components/menu/language_table.h - sortiert nach dem deutschen Text (Bytes), die Remote
sucht binaer (sprache.h). Aufruf: python3 tools/language_gen.py   (nach Aenderungen an language/*.json)
"""
import json
from pathlib import Path

E = Path(__file__).resolve().parent.parent


def c(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def tabelle(code: str, wb: dict) -> list[str]:
    """Paare (deutsch, Uebersetzung) als C-Array TAB_<CODE> plus Teilstueck-Indizes TEIL_<CODE>."""
    paare = sorted(((k, v) for k, v in wb.items() if v and v != k), key=lambda kv: kv[0].encode())
    C = code.upper()
    z = [f"inline const Paar TAB_{C}[] = {{"]
    z += [f"  {{{c(k)}, {c(v)}}}," for k, v in paare]
    # Teilstuecke fuer zusammengesetzte Texte (Zahlen dazwischen): Indizes langer Eintraege, laengste zuerst
    lang = sorted((i for i, (k, v) in enumerate(paare) if len(k.encode()) >= 8), key=lambda i: -len(paare[i][0].encode()))
    z += ["};", f"inline const unsigned TAB_{C}_N = {len(paare)};",
          f"inline const unsigned short TEIL_{C}[] = {{" + ", ".join(map(str, lang)) + "};",
          f"inline const unsigned TEIL_{C}_N = {len(lang)};", ""]
    print(f"{code}: {len(paare)} Eintraege")
    return z


en = json.loads((E / "language" / "en.json").read_text(encoding="utf-8"))
# Niederlaendisch (2026-11-20): language/nl.json; was dort fehlt, kommt englisch (besser als deutsch)
nl_datei = E / "language" / "nl.json"
nl = json.loads(nl_datei.read_text(encoding="utf-8")) if nl_datei.exists() else {}
nl = {k: (nl.get(k) or v) for k, v in en.items()} | {k: v for k, v in nl.items() if k not in en and v}
z = ["// GENERIERT von tools/language_gen.py aus language/en.json und language/nl.json - nicht von Hand aendern.",
     "#pragma once", "", "namespace sprache {", "struct Paar { const char *de; const char *en; };   // en = Uebersetzung", ""]
z += tabelle("en", en) + tabelle("nl", nl)
z += ["}  // namespace sprache", ""]
(E / "components" / "menu" / "language_table.h").write_text("\n".join(z), encoding="utf-8")
