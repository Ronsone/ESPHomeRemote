#pragma once
// Skins der Remote (2026-11-19): ganzes Farbpalett + optionales Hintergrundbild, umschaltbar ohne neue Firmware.
//
// Baut auf der Akzentfarbe auf (esphomeremote-accent.h) und verallgemeinert sie: die Oberflaeche nutzt ueberall
// dieselben ~18 Grundfarben (Hintergrund 0x0E1116, Karte 0x1A1F27, Text 0xF1F3F5, Grau 0x8B94A3 ...) plus die 5
// Abstufungen des Akzents. Ein Skin ordnet jeder dieser "Rollen" eine neue Farbe zu; skin::anwenden() geht alle
// Objekte und gemeinsamen Stile durch und tauscht jede Farbe, die einer Rolle gehoert, gegen die neue.
//
// Eindeutigkeit: damit ein spaeteres Umschalten (Skin A -> B) und neu gebaute Teile (die noch die Standardfarben
// tragen) sicher erkannt werden, darf keine Farbe eines Skins gleich einer Standardfarbe einer ANDEREN Rolle oder
// einer anderen Rolle desselben Skins sein. eindeutig() verschiebt solche Farben um 1 im untersten Bit - mit blossem
// Auge nicht zu sehen. Reines Schwarz/Weiss sind reserviert (Cover-Abdunklung, Bildfaerbung) und werden genauso
// verschoben.
//
// Auswahl: global skin_nr (Einstellung "skin_nr", mit dem Konfigurator abgeglichen, je Remote):
//   0..ANZ_FEST-1  eingebaute Skins (Standard, Hell, OLED, Nord, Retro, Synthwave)
//   10 + i         eigener Skin i aus dem Konfigurator (cfg "skins": [{name, hg, karte, ...}], hoechstens 8)
// Hintergrundbild (nur eigene Skins, "bild": true): SD-Karte /themes/<slug>/background.rle (RL16, 240x320,
// tools/make_skin_wallpaper.py) oder /themes/<slug>/background.rgb565 (roh, 153600 Byte, wie OpenRemote).
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/sd_card/sd_card.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

namespace skin {

enum Rolle : uint8_t {
  HG, HG_TIEF, KARTE, KARTE2, LINIE, RAND, SPUR, GRAU1, GRAU2, GRAU3, MATT, KNOPF, TEXT,
  OK, WARN, GELB, FEHLER, FEHLER_HELL,
  A0, A1, A2, A3, A4,   // Akzent und Abstufungen (akzent::stufen: Grund, gedrueckt, Rand, Flaeche, hell)
  N
};
static const int ERSTE_AKZENT = A0;

// Die Farben der Oberflaeche, wie sie in YAML und C++ stehen
static const uint32_t STANDARD[N] = {
  0x0E1116, 0x111418, 0x1A1F27, 0x232A35, 0x2A3340, 0x2E3643, 0x3A424E, 0x4B5563, 0x5B6472, 0x6B7280, 0x8B94A3,
  0xC9CED6, 0xF1F3F5, 0x22C55E, 0xF59E0B, 0xFACC15, 0xEF4444, 0xFCA5A5,
  0x3B82F6, 0x2563EB, 0x60A5FA, 0x1E3A8A, 0x93C5FD};

static const uint32_t FEHLT = 0xFF000000u;

struct Palette { uint32_t c[N]; };

// Was ein Skin festlegt; alles andere wird daraus gemischt
struct Basis {
  uint32_t hg{FEHLT}, karte{FEHLT}, karte2{FEHLT}, linie{FEHLT}, spur{FEHLT}, text{FEHLT}, matt{FEHLT},
      akzent{FEHLT}, ok{FEHLT}, warn{FEHLT}, fehler{FEHLT};
};

struct Fest { const char *name; Basis b; };
//                                  hg        karte     karte2    linie     spur      text      matt      akzent    ok        warn      fehler
static const Fest FESTE[] = {
  {"Standard",  {}},
  {"Hell",      {0xF3F4F6, 0xFFFFFF, 0xE5E7EB, 0xD1D5DB, 0xCBD5E1, 0x111827, 0x4B5563, 0x2563EB, 0x16A34A, 0xD97706, 0xDC2626}},
  {"OLED",      {0x000000, 0x0D0D0D, 0x1A1A1A, 0x222222, 0x2E2E2E, 0xFFFFFF, 0x9CA3AF, FEHLT,    FEHLT,    FEHLT,    FEHLT}},
  {"Nord",      {0x2E3440, 0x3B4252, 0x434C5E, 0x4C566A, 0x56607A, 0xECEFF4, 0x9AA5B8, 0x88C0D0, 0xA3BE8C, 0xEBCB8B, 0xBF616A}},
  {"Retro",     {0x0B0A07, 0x17140D, 0x211C12, 0x2E2618, 0x3A3020, 0xFFD9A0, 0xB08D57, 0xFFB000, 0x7FD15A, 0xFF8C00, 0xFF5544}},
  {"Synthwave", {0x120B1E, 0x1E1430, 0x2A1C42, 0x3A2858, 0x47306B, 0xF5E9FF, 0xA08CC0, 0xFF3EA5, 0x3EF7C4, 0xFFB347, 0xFF4D6D}},
};
static const int ANZ_FEST = sizeof(FESTE) / sizeof(FESTE[0]);
static const int EIGENE_AB = 10;
static const int MAX_EIGENE = 8;

inline bool gesetzt(uint32_t c) { return c != FEHLT; }
inline uint32_t oder(uint32_t c, uint32_t std) { return gesetzt(c) ? c : std; }

// "#RRGGBB" -> Farbe, sonst FEHLT
inline uint32_t farbe_aus(const char *t) {
  if (t == nullptr || t[0] != '#' || strlen(t) != 7) return FEHLT;
  char *ende = nullptr;
  const unsigned long v = strtoul(t + 1, &ende, 16);
  return (ende != nullptr && *ende == 0) ? (uint32_t) v : FEHLT;
}

// ---- Palette bauen ---------------------------------------------------------------------------------------------
inline bool ist_leer(const Basis &b) {
  return !gesetzt(b.hg) && !gesetzt(b.karte) && !gesetzt(b.karte2) && !gesetzt(b.linie) && !gesetzt(b.spur) &&
         !gesetzt(b.text) && !gesetzt(b.matt) && !gesetzt(b.ok) && !gesetzt(b.warn) && !gesetzt(b.fehler);
}

inline bool vorhanden_obj(const std::vector<lv_obj_t *> &v, lv_obj_t *o) {
  for (auto *x : v) if (x == o) return true;
  return false;
}
inline bool vorhanden(const std::vector<uint32_t> &v, uint32_t c) {
  for (uint32_t x : v) if (x == c) return true;
  return false;
}

// Farbe minimal verschieben, bis sie keine der verbotenen ist
inline uint32_t ausweichen(uint32_t c, const std::vector<uint32_t> &verboten) {
  static const uint32_t D[] = {0, 0x000001, 0x000100, 0x010000, 0x000101, 0x010001, 0x010100, 0x010101,
                               0x000002, 0x000200, 0x020000, 0x000202, 0x020002, 0x020200, 0x020202, 0x000003};
  for (uint32_t d : D) {
    const uint32_t k = c ^ d;
    if (!vorhanden(verboten, k)) return k;
  }
  return c;
}

// Akzent-Abstufungen einer Palette eintragen (gleiche Rechnung wie akzent::stufen, damit Code, der zur Laufzeit
// akzent::stufen(akzent::farbe()) aufruft, genau dieselben Werte bekommt)
inline void akzent_eintragen(Palette &p, uint32_t a) {
  const akzent::Stufen s = akzent::stufen(a);
  for (int i = 0; i < 5; i++) p.c[A0 + i] = s.f[i];
}

// cfg_akzent: Akzentfarbe aus dem Konfigurator ("akzent") - gilt, wenn der Skin keinen eigenen Akzent hat
inline Palette bauen(const Basis &b, uint32_t cfg_akzent) {
  Palette p;
  for (int i = 0; i < N; i++) p.c[i] = STANDARD[i];
  const bool standard = ist_leer(b);
  akzent::flaeche() = standard ? 0x000000 : oder(b.hg, STANDARD[HG]);   // vor allen akzent::stufen-Aufrufen
  if (!standard) {
    const uint32_t hg = oder(b.hg, STANDARD[HG]), karte = oder(b.karte, STANDARD[KARTE]),
                   karte2 = oder(b.karte2, STANDARD[KARTE2]), linie = oder(b.linie, STANDARD[LINIE]),
                   spur = oder(b.spur, STANDARD[SPUR]), text = oder(b.text, STANDARD[TEXT]),
                   matt = oder(b.matt, STANDARD[MATT]), ok = oder(b.ok, STANDARD[OK]),
                   warn = oder(b.warn, STANDARD[WARN]), fehler = oder(b.fehler, STANDARD[FEHLER]);
    using akzent::mischen;
    p.c[HG] = hg;
    p.c[HG_TIEF] = mischen(hg, karte, 50);
    p.c[KARTE] = karte;
    p.c[KARTE2] = karte2;
    p.c[LINIE] = linie;
    p.c[RAND] = mischen(linie, spur, 80);
    p.c[SPUR] = spur;
    p.c[GRAU1] = mischen(spur, matt, 64);
    p.c[GRAU2] = mischen(spur, matt, 110);
    p.c[GRAU3] = mischen(spur, matt, 150);
    p.c[MATT] = matt;
    p.c[KNOPF] = mischen(matt, text, 160);
    p.c[TEXT] = text;
    p.c[OK] = ok;
    p.c[WARN] = warn;
    p.c[GELB] = gesetzt(b.warn) ? mischen(warn, text, 70) : STANDARD[GELB];
    p.c[FEHLER] = fehler;
    p.c[FEHLER_HELL] = gesetzt(b.fehler) ? mischen(fehler, text, 140) : STANDARD[FEHLER_HELL];
  }
  // Verboten fuer eine Rolle r: Standardfarbe jeder anderen Rolle, schon vergebene Farben dieses Skins,
  // Schwarz/Weiss und die Abstufungen des Konfigurator-Akzents (die Menue-Komponente baut damit)
  std::vector<uint32_t> cfg_stufen;
  if (gesetzt(cfg_akzent)) {
    const akzent::Stufen s = akzent::stufen(cfg_akzent);
    cfg_stufen.assign(s.f, s.f + 5);
  }
  for (int r = 0; r < ERSTE_AKZENT; r++) {
    if (p.c[r] == STANDARD[r] && standard) continue;
    std::vector<uint32_t> v = {0x000000, 0xFFFFFF};
    for (int s = 0; s < N; s++) if (s != r) v.push_back(STANDARD[s]);
    for (int s = 0; s < r; s++) v.push_back(p.c[s]);
    for (uint32_t c : cfg_stufen) v.push_back(c);
    p.c[r] = ausweichen(p.c[r], v);
  }
  // Akzent: Grundfarbe verschieben (nicht die Stufen einzeln), bis keine Stufe mit etwas anderem zusammenfaellt
  uint32_t a = gesetzt(b.akzent) ? b.akzent : gesetzt(cfg_akzent) ? cfg_akzent : akzent::BLAU;
  if (!(standard && a == akzent::BLAU)) {
    std::vector<uint32_t> v = {0x000000, 0xFFFFFF};
    for (int s = 0; s < ERSTE_AKZENT; s++) { v.push_back(STANDARD[s]); v.push_back(p.c[s]); }
    static const uint32_t D[] = {0, 0x000001, 0x000100, 0x010000, 0x000101, 0x010001, 0x010100, 0x010101, 0x000002, 0x000200};
    for (uint32_t d : D) {
      const akzent::Stufen st = akzent::stufen(a ^ d);
      bool frei = true;
      for (int i = 0; i < 5 && frei; i++) {
        if (vorhanden(v, st.f[i])) frei = false;
        for (int j = 0; j < 5 && frei; j++) if (j != i && st.f[j] == st.f[i]) frei = false;
        for (int j = 0; j < 5 && frei; j++) if (j != i && STANDARD[A0 + j] == st.f[i]) frei = false;
      }
      if (frei) { a ^= d; break; }
    }
  }
  akzent_eintragen(p, a);
  return p;
}

// ---- Auswahl aus der Konfiguration ------------------------------------------------------------------------------
struct Wahl {
  Palette pal;
  std::string name;
  std::string slug;      // Ordner unter /themes auf der SD-Karte
  bool bild{false};      // Hintergrundbild laden
  int dimmen{40};        // Bild abdunkeln (0..90 %)
  bool ueberall{false};  // Bild auf allen Seiten statt nur auf der Startseite
};

inline std::string slug(const std::string &n) {
  std::string s;
  for (char ch : n) {
    if (std::isalnum((unsigned char) ch)) s += (char) std::tolower((unsigned char) ch);
    else if (ch == ' ' || ch == '-' || ch == '_') { if (!s.empty() && s.back() != '_') s += '_'; }
  }
  while (!s.empty() && s.back() == '_') s.pop_back();
  return s.empty() ? std::string("skin") : s;
}

inline int anzahl_eigene(JsonObjectConst w) {
  JsonArrayConst a = w["skins"].as<JsonArrayConst>();
  if (a.isNull()) return 0;
  const int n = (int) a.size();
  return n > MAX_EIGENE ? MAX_EIGENE : n;
}

inline bool gueltig(int nr, JsonObjectConst w) {
  return (nr >= 0 && nr < ANZ_FEST) || (nr >= EIGENE_AB && nr < EIGENE_AB + anzahl_eigene(w));
}

inline Wahl aufloesen(int nr, JsonObjectConst w) {
  const uint32_t cfg_akz = farbe_aus(w["akzent"] | "");
  Wahl e;
  if (!gueltig(nr, w)) nr = 0;
  if (nr < ANZ_FEST) {
    e.name = FESTE[nr].name;
    e.slug = slug(e.name);
    e.pal = bauen(FESTE[nr].b, cfg_akz);
    return e;
  }
  JsonObjectConst s = w["skins"][nr - EIGENE_AB].as<JsonObjectConst>();
  Basis b;
  b.hg = farbe_aus(s["hg"] | "");
  b.karte = farbe_aus(s["karte"] | "");
  b.karte2 = farbe_aus(s["karte2"] | "");
  b.linie = farbe_aus(s["linie"] | "");
  b.spur = farbe_aus(s["spur"] | "");
  b.text = farbe_aus(s["text"] | "");
  b.matt = farbe_aus(s["matt"] | "");
  b.akzent = farbe_aus(s["akzent"] | "");
  b.ok = farbe_aus(s["ok"] | "");
  b.warn = farbe_aus(s["warn"] | "");
  b.fehler = farbe_aus(s["fehler"] | "");
  e.name = s["name"] | "Eigener Skin";
  e.slug = slug(e.name);
  e.bild = s["bild"] | false;
  e.dimmen = s["dimmen"] | 40;
  if (e.dimmen < 0) e.dimmen = 0;
  if (e.dimmen > 90) e.dimmen = 90;
  e.ueberall = s["ueberall"] | false;
  e.pal = bauen(b, cfg_akz);
  return e;
}

// Naechster Skin in der Reihe (Taste/Menue "Skin wechseln"): eingebaute, dann eigene, dann wieder von vorn
inline int naechster(int nr, JsonObjectConst w) {
  const int n_eig = anzahl_eigene(w);
  if (nr < 0) return 0;
  if (nr < ANZ_FEST - 1) return nr + 1;
  if (nr < ANZ_FEST) return n_eig > 0 ? EIGENE_AB : 0;
  if (nr - EIGENE_AB + 1 < n_eig) return nr + 1;
  return 0;
}

// ---- Umfaerben --------------------------------------------------------------------------------------------------
inline Palette &aktuell() {
  static Palette p = [] { Palette x; for (int i = 0; i < N; i++) x.c[i] = STANDARD[i]; return x; }();
  return p;
}
inline uint32_t &cfg_akzent_merk() { static uint32_t c = FEHLT; return c; }
inline bool ist_standard() {
  for (int i = 0; i < N; i++) if (aktuell().c[i] != STANDARD[i]) return false;
  return true;
}
// Farbe einer Rolle im aktiven Skin (fuer neuen Code: skin::f(skin::KARTE) statt 0x1A1F27)
inline uint32_t f(Rolle r) { return aktuell().c[r]; }

struct Eintrag { uint32_t c; uint8_t r; };
// Woran eine Farbe erkannt wird: aktiver Skin zuerst, dann Standard, dann Abstufungen des Konfigurator-Akzents
inline std::vector<Eintrag> quellen(const Palette &von) {
  std::vector<Eintrag> q;
  q.reserve(N * 2 + 5);
  for (int i = 0; i < N; i++) q.push_back({von.c[i], (uint8_t) i});
  for (int i = 0; i < N; i++) if (STANDARD[i] != von.c[i]) q.push_back({STANDARD[i], (uint8_t) i});
  if (gesetzt(cfg_akzent_merk())) {
    const akzent::Stufen s = akzent::stufen(cfg_akzent_merk());
    for (int i = 0; i < 5; i++) q.push_back({s.f[i], (uint8_t) (A0 + i)});
  }
  return q;
}
inline int rolle_von(const std::vector<Eintrag> &q, uint32_t c) {
  for (const auto &e : q) if (e.c == c) return e.r;
  return -1;
}

static const lv_style_prop_t PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, LV_STYLE_TEXT_COLOR, LV_STYLE_OUTLINE_COLOR,
                                        LV_STYLE_LINE_COLOR, LV_STYLE_ARC_COLOR, LV_STYLE_SHADOW_COLOR, LV_STYLE_BG_GRAD_COLOR,
                                        LV_STYLE_IMAGE_RECOLOR};

inline void faerben_(lv_obj_t *o, const std::vector<Eintrag> &q, const Palette &neu, bool knopf_theme) {
  if (o == nullptr) return;
  const bool aus = akzent::ausgenommen(o);   // Bluetooth-Blau: Akzent-Rollen bleiben, Grundfarben folgen dem Skin
  static const lv_part_t TEILE[] = {LV_PART_MAIN, LV_PART_SCROLLBAR, LV_PART_INDICATOR, LV_PART_KNOB, LV_PART_SELECTED, LV_PART_ITEMS, LV_PART_CURSOR};
  static const lv_state_t ZUST[] = {LV_STATE_DEFAULT, LV_STATE_CHECKED, LV_STATE_FOCUSED, LV_STATE_FOCUS_KEY, LV_STATE_PRESSED,
                                    LV_STATE_EDITED, (lv_state_t) (LV_STATE_CHECKED | LV_STATE_PRESSED)};
  for (lv_part_t t : TEILE)
    for (lv_state_t z : ZUST) {
      const lv_style_selector_t sel = (lv_style_selector_t) t | z;
      for (lv_style_prop_t p : PROPS) {
        lv_style_value_t v;
        if (lv_obj_get_local_style_prop(o, p, &v, sel) != LV_RESULT_OK) continue;
        const uint32_t c = akzent::als_hex(v.color);
        const int r = rolle_von(q, c);
        if (r < 0 || neu.c[r] == c || (aus && r >= ERSTE_AKZENT)) continue;
        v.color = akzent::hex(neu.c[r]);
        lv_obj_set_local_style_prop(o, p, v, sel);
      }
    }
  // Theme-Knoepfe (Fokus-Rahmen, gedrueckt) wie bei der Akzentfarbe: eigene Werte, die das Theme ueberdecken
  if (knopf_theme && !aus && lv_obj_check_type(o, &lv_button_class)) {
    lv_style_value_t v;
    if (lv_obj_get_local_style_prop(o, LV_STYLE_BORDER_COLOR, &v, LV_STATE_FOCUSED) != LV_RESULT_OK)
      lv_obj_set_style_border_color(o, akzent::hex(neu.c[A0]), LV_STATE_FOCUSED);
    if (lv_obj_get_local_style_prop(o, LV_STYLE_BG_COLOR, &v, LV_STATE_PRESSED) != LV_RESULT_OK)
      lv_obj_set_style_bg_color(o, akzent::hex(neu.c[A1]), LV_STATE_PRESSED);
    if (lv_obj_get_local_style_prop(o, LV_STYLE_BORDER_COLOR, &v, LV_STATE_PRESSED) != LV_RESULT_OK)
      lv_obj_set_style_border_color(o, akzent::hex(neu.c[A2]), LV_STATE_PRESSED);
  }
  const uint32_t n = lv_obj_get_child_count(o);
  for (uint32_t i = 0; i < n; i++) faerben_(lv_obj_get_child(o, i), q, neu, knopf_theme);
}

inline void stil_faerben_(lv_style_t *s, const std::vector<Eintrag> &q, const Palette &neu) {
  bool geaendert = false;
  for (lv_style_prop_t p : PROPS) {
    lv_style_value_t v;
    if (lv_style_get_prop(s, p, &v) != LV_STYLE_RES_FOUND) continue;
    const uint32_t c = akzent::als_hex(v.color);
    const int r = rolle_von(q, c);
    if (r < 0 || neu.c[r] == c) continue;
    v.color = akzent::hex(neu.c[r]);
    lv_style_set_prop(s, p, v);
    geaendert = true;
  }
  if (geaendert) lv_obj_report_style_change(s);
}

// Ganzen Skin anwenden. seiten: alle Seiten-Objekte, stile: gemeinsame Stile (style_definitions)
inline void anwenden(const Palette &neu, uint32_t cfg_akzent, const std::vector<lv_obj_t *> &seiten,
                     const std::vector<lv_style_t *> &stile) {
  bool gleich = cfg_akzent == cfg_akzent_merk();
  for (int i = 0; i < N && gleich; i++) gleich = neu.c[i] == aktuell().c[i];
  if (gleich) return;   // nichts zu tun (neu gebaute Teile: auffrischen)
  const uint32_t t0 = millis();
  const auto q = quellen(aktuell());
  const bool knopf = neu.c[A0] != akzent::BLAU;
  for (auto *s : seiten) faerben_(s, q, neu, knopf);
  for (auto *s : stile) stil_faerben_(s, q, neu);
  faerben_(lv_layer_top(), q, neu, knopf);
  aktuell() = neu;
  cfg_akzent_merk() = cfg_akzent;
  akzent::wert() = neu.c[A0];   // akzent::farbe() fuer alles, was danach gebaut wird
  lv_obj_invalidate(lv_screen_active());
  ESP_LOGI("skin", "Skin angewendet: HG #%06X Text #%06X Akzent #%06X in %u ms", (unsigned) neu.c[HG],
           (unsigned) neu.c[TEXT], (unsigned) neu.c[A0], (unsigned) (millis() - t0));
}

// Nachtraeglich gebaute Teile (Menue, Popups, Widgets tragen noch Standardfarben) auf den aktiven Skin bringen
inline void auffrischen(lv_obj_t *o) {
  if (o == nullptr || ist_standard()) return;
  const auto q = quellen(aktuell());
  faerben_(o, q, aktuell(), aktuell().c[A0] != akzent::BLAU);
}

// Bei jedem Seitenwechsel die neue Seite und die oberste Ebene auffrischen (nur wenn ein Skin aktiv ist).
// LVGL schickt LV_EVENT_SCREEN_LOADED an die Seite selbst (nicht ans Display): an jede Seite einmal haengen.
inline void beim_seitenwechsel_einrichten(const std::vector<lv_obj_t *> &seiten) {
  static std::vector<lv_obj_t *> fertig;
  for (auto *o : seiten) {
    if (o == nullptr || vorhanden_obj(fertig, o)) continue;
    fertig.push_back(o);
    lv_obj_add_event_cb(o, [](lv_event_t *e) {
      if (ist_standard()) return;
      auffrischen((lv_obj_t *) lv_event_get_target(e));
      auffrischen(lv_layer_top());
    }, LV_EVENT_SCREEN_LOADED, nullptr);
  }
}

// ---- Hintergrundbild (SD-Karte) ---------------------------------------------------------------------------------
struct Bild {
  std::vector<uint8_t> daten;
  lv_image_dsc_t dsc{};
  bool ok{false};
};
inline Bild &bild() { static Bild b; return b; }        // angezeigt
inline Bild &geladen() { static Bild b; return b; }     // vom Task gefuellt
inline volatile bool &busy() { static volatile bool b = false; return b; }
inline std::string &bild_slug() { static std::string s; return s; }   // was gerade angezeigt wird
inline std::string &bild_slug_geladen() { static std::string s; return s; }   // was im Speicher liegt

inline bool rle_lesen(const std::vector<uint8_t> &roh, Bild &b) {
  if (roh.size() < 8 || roh[0] != 'R' || roh[1] != 'L' || roh[2] != '1' || roh[3] != '6') return false;
  const int w = roh[4] | (roh[5] << 8), h = roh[6] | (roh[7] << 8);
  if (w != 240 || h != 320) return false;
  const size_t gesamt = (size_t) w * h;
  b.daten.assign(gesamt * 2, 0);
  uint16_t *aus = reinterpret_cast<uint16_t *>(b.daten.data());
  size_t pos = 8, px = 0;
  while (pos + 4 <= roh.size() && px < gesamt) {
    size_t n = roh[pos] | (roh[pos + 1] << 8);
    const uint16_t v = roh[pos + 2] | (roh[pos + 3] << 8);
    pos += 4;
    if (px + n > gesamt) n = gesamt - px;
    for (size_t i = 0; i < n; i++) aus[px++] = v;
  }
  return px == gesamt;
}

inline void fuellen(Bild &b) {
  b.dsc = {};
  b.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  b.dsc.header.cf = LV_COLOR_FORMAT_RGB565;
  b.dsc.header.w = 240;
  b.dsc.header.h = 320;
  b.dsc.header.stride = 240 * 2;
  b.dsc.data_size = 240 * 320 * 2;
  b.dsc.data = b.daten.data();
  b.ok = true;
}

struct Auftrag { esphome::sd_card::SdCard *sd; std::string slug; };
inline void task_fn(void *arg) {
  Auftrag *a = static_cast<Auftrag *>(arg);
  Bild &b = geladen();
  b = Bild();
  if (a->sd->mount()) {
    std::vector<uint8_t> roh;
    const std::string ordner = "/themes/" + a->slug + "/background";
    if (a->sd->read_binary(ordner + ".rle", roh, 400000) && rle_lesen(roh, b)) {
      fuellen(b);
    } else {
      roh.clear();
      if (a->sd->read_binary(ordner + ".rgb565", roh, 240 * 320 * 2 + 16) && roh.size() == 240 * 320 * 2) {
        b.daten = std::move(roh);
        fuellen(b);
      }
    }
    a->sd->unmount();
  }
  if (!b.ok) { b.daten.clear(); b.daten.shrink_to_fit(); }
  delete a;
  busy() = false;
  vTaskDelete(nullptr);
}
inline bool bild_starten(esphome::sd_card::SdCard *sd, const std::string &slug) {
  if (busy()) return false;
  busy() = true;
  if (xTaskCreate(task_fn, "skinbild", 8192, new Auftrag{sd, slug}, 1, nullptr) != pdPASS) {
    busy() = false;
    return false;
  }
  return true;
}

inline std::string &bild_fehlt() { static std::string s; return s; }   // Slug ohne Datei: nicht erneut suchen

// Bild von den Seiten nehmen (vor dem Freigeben des Speichers!)
inline void bild_weg(lv_obj_t *home, lv_style_t *st_screen) {
  lv_obj_remove_local_style_prop(home, LV_STYLE_BG_IMAGE_SRC, 0);
  lv_obj_remove_local_style_prop(home, LV_STYLE_BG_IMAGE_OPA, 0);
  lv_style_remove_prop(st_screen, LV_STYLE_BG_IMAGE_SRC);
  lv_style_remove_prop(st_screen, LV_STYLE_BG_IMAGE_OPA);
  lv_obj_report_style_change(st_screen);
  bild_slug().clear();
}

// Angezeigtes Bild nach den Einstellungen des Skins platzieren (Startseite oder alle Seiten, Abdunklung)
inline void bild_zeigen(const Wahl &w, lv_obj_t *home, lv_style_t *st_screen) {
  bild_weg(home, st_screen);
  if (!bild().ok) return;
  const lv_opa_t opa = (lv_opa_t) (255 * (100 - w.dimmen) / 100);   // der Rest: Skin-Hintergrund scheint durch
  if (w.ueberall) {
    lv_style_set_bg_image_src(st_screen, &bild().dsc);
    lv_style_set_bg_image_opa(st_screen, opa);
    lv_obj_report_style_change(st_screen);
  } else {
    lv_obj_set_style_bg_image_src(home, &bild().dsc, 0);
    lv_obj_set_style_bg_image_opa(home, opa, 0);
  }
  bild_slug() = w.slug;
  ESP_LOGI("skin", "Hintergrundbild %s (%s, abgedunkelt %d %%)", w.slug.c_str(), w.ueberall ? "alle Seiten" : "Startseite", w.dimmen);
}

// Bild aus, Speicher frei
inline void bild_aus(lv_obj_t *home, lv_style_t *st_screen) {
  bild_weg(home, st_screen);
  bild() = Bild();
  bild_slug_geladen().clear();
}

// Nach dem Laden (busy() == false, im Hauptprogramm): geladenes Bild uebernehmen und zeigen
inline bool bild_uebernehmen(const Wahl &w, lv_obj_t *home, lv_style_t *st_screen) {
  if (!geladen().ok) {
    bild_fehlt() = w.slug;
    return false;
  }
  bild_weg(home, st_screen);   // erst von den Seiten nehmen, dann den alten Speicher ersetzen
  bild() = std::move(geladen());
  bild().dsc.data = bild().daten.data();
  geladen() = Bild();
  bild_slug_geladen() = w.slug;
  bild_zeigen(w, home, st_screen);
  return true;
}

// Was ist zu tun? 0 = nichts, 1 = Bild von der Karte laden. Bild aus / Platzierung aendern erledigt es selbst.
inline int bild_pruefen(const Wahl &w, lv_obj_t *home, lv_style_t *st_screen) {
  if (!w.bild) {
    if (bild().ok) bild_aus(home, st_screen);
    return 0;
  }
  if (bild().ok && bild_slug_geladen() == w.slug) {   // gleiches Bild: nur Platzierung/Abdunklung
    bild_zeigen(w, home, st_screen);
    return 0;
  }
  if (bild_fehlt() == w.slug) return 0;
  return 1;
}

}  // namespace skin
