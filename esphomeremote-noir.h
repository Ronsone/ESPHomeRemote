#pragma once
// NOIR / SIGNATURE (2026-11-21): startscherm uit het NOIR-bouwpakket (simulator/main.c, LVGL 8.3) overgezet naar
// LVGL 9 op de bestaande Aggro600-firmware. Display-, touch-, knop-, IR- en HA-drivers blijven ongewijzigd.
//
// Opbouw: een laag over de volle 240 x 320 van page_home (kind van de pagina). Daardoor blijft alles van de firmware
// gelijk: cur_page = 0, wekken, slapen, dock, toetsindeling. De laag verschijnt bij elk laden van de startpagina
// (noir::bij_home) en verdwijnt als je op "TV" tikt - dan zie je de gewone Aggro600-startpagina (activiteit,
// nu-afspelen-kaart). Menutoets -> menu -> Terug brengt je weer op NOIR Thuis.
//
// Posities, maten, kleuren en labels komen uit build(0) van de simulator: body op (12,30) 216 breed, titel op y=5,
// tandwiel 32 x 32 rechtsboven, tegels 104 x 66 op (0|112, 58|132), Terug/Thuis 104 x 42 op y=270.
//
// KEF (noir::kef): de fysieke volumetoetsen (33 Lauter, 43 Leiser, 44 Stumm) gaan op elk scherm naar de KEF LSX II
// (substitution noir_kef_entity) - in on_key vóór alle scherm- en invoerlogica en vóór de toetsindeling. Leeg = uit
// (dan geldt de toetsindeling zoals voorheen). Ingedrukt houden herhaalt elke 300 ms (hoogstens 30 x).
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#include "esphome/components/menu/mdi_icons.h"

namespace noir {

// Kleuren van de simulator (skin NOIR / Obsidian)
static const uint32_t BG = 0x07080A, SURF = 0x1A1C20, SURF2 = 0x101114, SURF_DIM = 0x101216, TEXT = 0xF2F3F5,
                      TEXT2 = 0xE4E7EC, MUTED = 0x969AA3, STAT = 0xB6BAC3, RAND = 0x383C43, RAND_P = 0xAEB4BF,
                      DRUK = 0x363A43, DRUK2 = 0x252931, LIJN = 0xC9D0DC, BATT = 0xA4AAB5, BATT_F = 0xD3D8E0,
                      DOT = 0xD7DCE5;

// Acties van de tegels (zelfde betekenis als action() in de simulator: 101-104 tegels, 113 instellingen, 1 Terug,
// 100 Thuis)
// ---- NOIR-instellingen (2026-11-22, simulator build(13) "UI-instellingen") --------------------------------------
// Skin = vier grondkleuren + afronding; alle vaste kleuren hierboven worden bij het tekenen omgezet zoals c() in de
// simulator (k()). Skin 0 Obsidian geeft precies de kleuren van de simulator.
struct Thema {
  int skin{0};        // 0 Obsidian, 1 Graphite, 2 Pure Black, 3 Nautisch, 4 Eigen (kleuren van de actieve Aggro600-skin)
  int wall{0};        // wallpaper: 0 Obsidian / satijn, 1 Graphite / zacht, 2 effen
  int radius{16};     // afronding van knoppen en tegels (8-20 px)
  uint32_t bg{BG}, surf{SURF}, text{TEXT}, accent{0xBFC2C9};
};
inline Thema &thema() { static Thema t; return t; }
static const int SKINS = 5, WALLS = 3;
static const char *SKIN_NAMEN[5] = {"NOIR / Obsidian", "SIGNATURE / Graphite", "PURE / Black", "Nautisch / Navy", "Eigen (Aggro600-skin)"};
static const char *WALL_NAMEN[3] = {"Obsidian / satijn", "Graphite / zacht", "Effen"};
// bg, surf, text, accent - radius - wallpaper (simulator preset_colors / preset_radii)
static const uint32_t SKIN_KLEUR[4][4] = {{0x07080A, 0x1A1C20, 0xF2F3F5, 0xBFC2C9}, {0x0C0E12, 0x20242B, 0xF2F3F5, 0xD4D9E2},
                                          {0x000000, 0x111214, 0xF5F5F5, 0xCCCCCC}, {0x071322, 0x14283A, 0xF5EEDC, 0xD6AB61}};
static const int SKIN_RADIUS[4] = {16, 14, 12, 16}, SKIN_WALL[4] = {0, 1, 2, 2};

inline uint32_t mix(uint32_t a, uint32_t b, int pct) {
  uint32_t v = 0;
  for (int s = 0; s < 24; s += 8) v |= ((((a >> s) & 255) * (100 - pct) + ((b >> s) & 255) * pct) / 100) << s;
  return v;
}
// Vaste simulatorkleur -> kleur van de skin (simulator: c())
inline uint32_t kleur(uint32_t x) {
  const Thema &t = thema();
  switch (x) {
    case 0x07080A: case 0x08090B: return t.bg;
    case 0x101216: return mix(t.bg, t.surf, 35);
    case 0x1A1C20: case 0x14161A: case 0x22252A: return t.surf;
    case 0x101114: return mix(t.surf, t.bg, 55);
    case 0xF2F3F5: case 0xE4E7EC: case 0xEFF1F5: return t.text;
    case 0x969AA3: case 0xB6BAC3: return mix(t.text, t.bg, 37);
    case 0xBFC2C9: case 0xAEB4BF: case 0xD3D8E0: return t.accent;
    case 0x383C43: case 0x36393F: return mix(t.surf, t.text, 14);
    default: return x;
  }
}
inline lv_color_t k(uint32_t c) { return lv_color_hex(kleur(c)); }
// simulator box(): afrondingen vanaf 10 px volgen de skin
inline int rnd(int r) { return r >= 10 && r < LV_RADIUS_CIRCLE ? thema().radius : r; }

enum Actie : int { TERUG = 1, THUIS = 100, TV = 101, AUDIO = 102, LICHT = 103, SCENES = 104, AIRCO = 105, INSTELLINGEN = 113,
                   INST_AFRONDING = 129, INST_REMOTE = 130, DIM_GEREED = 128,
                   AIRCO_AANUIT = 140, AIRCO_MIN = 141, AIRCO_PLUS = 142, AIRCO_FAN = 143, AIRCO_SWING = 144, AIRCO_HI = 145, AIRCO_ECO = 146, KNOPTEST = 160, TEST_RESET = 161, SCENE_ALLES_UIT = 170,
                   NETFLIX = 106, NF_AFSPELEN = 186,
                   NF_TITEL = 3200 /* 3200 + lijst*100 + i: lijst 0 Top 10, 1 Populair, 2 Laatst gestart */,
                   SCENE = 3000 /* 3000+i: scène i van de configuratorpagina "noir_scenes" */,
                   AIRCO_MODUS = 150 /* 150-154 */,
                   LAMP = 400 /* 400+i: lamp i op NOIR Verlichting */,
                   TV_AANUIT = 120, TV_APP = 200 /* 200-205: app-tegels */,
                   ITEM = 1000 /* 1000+i: tegel i van de configuratorpagina "noir" */ };

struct Fonts { const lv_font_t *f12{nullptr}, *f14{nullptr}, *f16{nullptr}, *ico{nullptr}, *f20{nullptr}, *groot{nullptr}; };

// App-tegels van het TV-scherm (simulator: apps[] in main.c). bron = tv_source voor het HA-script.
static const char *APP_NAAM[6] = {"PS5", "Switch", "Netflix", "YouTube", "Cookie IPTV", "Plex"};
inline std::string *app_bron() {
  static std::string b[6] = {"PS5 Gameconsole", "HDMI 2", "Netflix", "YouTube", "Cookie IPTV", "Plex"};
  return b;
}
// "a|b|c|d|e|f" (substitution noir_tv_bronnen) -> app_bron(); lege delen houden de standaard
inline void bronnen_zetten(const char *lijst) {
  std::string l = lijst != nullptr ? lijst : "";
  for (int i = 0; i < 6 && !l.empty(); i++) {
    const size_t p = l.find('|');
    const std::string d = l.substr(0, p);
    if (!d.empty()) app_bron()[i] = d;
    if (p == std::string::npos) break;
    l = l.substr(p + 1);
  }
}
struct Beelden {
  const void *wall{nullptr}, *tv{nullptr}, *audio{nullptr}, *licht{nullptr}, *scenes{nullptr};
  const void *app[6]{};   // PS5, Switch, Netflix, YouTube, Cookie IPTV (nullptr = tekst), Plex
};

enum Seite : int { S_THUIS = 0, S_TV = 1, S_LICHT = 2, S_INST = 3, S_AIRCO = 4, S_TEST = 5, S_SCENES = 6, S_NETFLIX = 7, S_NF_DETAIL = 8, S_ANZ = 9 };

struct Stand {
  lv_obj_t *laag{nullptr};                  // NOIR Thuis
  lv_obj_t *tv{nullptr};                    // NOIR TV (2026-11-21)
  lv_obj_t *kop{nullptr};                   // statusbalk, hangt aan de zichtbare laag
  lv_obj_t *klok{nullptr}, *batt_txt{nullptr}, *batt_fill{nullptr}, *ha_dot{nullptr};
  lv_obj_t *tv_status{nullptr};
  lv_obj_t *thuis_body{nullptr};            // tegels van NOIR Thuis (opnieuw opgebouwd bij nieuwe configuratie)
  Fonts fonts;                              // voor het herbouwen
  Beelden beelden;
  int seite{S_THUIS};                       // laatst getoonde NOIR-pagina
  std::string tv_state;                     // "on", "off", "unavailable" ... (uit HA)
  bool aan{false};      // NOIR als startscherm gebruiken
  bool zichtbaar{false};
  std::function<void(int)> actie;   // wordt door de firmware ingevuld (tasten::noir_actie)
  const lv_font_t *toast_font{nullptr};
  // 2026-11-22: Verlichting en UI-instellingen, herbouwen na skinwissel
  lv_obj_t *licht{nullptr}, *inst{nullptr}, *licht_body{nullptr}, *airco{nullptr}, *test{nullptr}, *scenes{nullptr}, *scenes_body{nullptr};
  std::function<JsonArrayConst()> scenes_items;   // configuratorpagina "noir_scenes"
  lv_obj_t *netflix{nullptr}, *netflix_body{nullptr}, *nfd_laag{nullptr}, *nfd_body{nullptr};   // NOIR Netflix (2026-11-22)
  lv_obj_t *home{nullptr};
  std::function<JsonArrayConst()> items;    // tegels van NOIR Thuis (configurator), voor het herbouwen
};
inline Stand &st() { static Stand s; return s; }


inline lv_obj_t *label(lv_obj_t *p, const char *t, int x, int y, int w, const lv_font_t *f, uint32_t kleur) {
  lv_obj_t *l = lv_label_create(p);
  lv_label_set_text(l, t);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_width(l, w);
  if (f != nullptr) lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, k(kleur), 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  return l;
}

inline lv_obj_t *detail(lv_obj_t *p, int x, int y, int w, int h, uint32_t kleur, int opa, int radius) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, k(kleur), 0);
  lv_obj_set_style_bg_opa(o, (lv_opa_t) opa, 0);
  lv_obj_set_style_radius(o, rnd(radius), 0);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

inline void klik_(lv_event_t *e) {
  const int a = (int) (intptr_t) lv_event_get_user_data(e);
  if (st().actie) st().actie(a);
}

// Knop zoals button() in de simulator (grafietvlak, verloop, fijne bovenrand, aparte ingedrukte toestand)
inline lv_obj_t *knop(lv_obj_t *p, int x, int y, int w, int h, int actie, lv_event_code_t code = LV_EVENT_CLICKED) {
  lv_obj_t *b = lv_button_create(p);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, k(SURF), 0);
  lv_obj_set_style_bg_opa(b, 232, 0);
  lv_obj_set_style_radius(b, rnd(h >= 60 ? 16 : h <= 34 ? 11 : 13), 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, k(RAND), 0);
  lv_obj_set_style_border_opa(b, 115, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_outline_width(b, 0, LV_STATE_FOCUSED);
  lv_obj_set_style_bg_grad_color(b, k(SURF2), 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_color(b, k(DRUK), LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_color(b, k(DRUK2), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(b, k(RAND_P), LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(b, 210, LV_STATE_PRESSED);
  lv_obj_set_style_border_color(b, k(RAND), LV_STATE_FOCUSED);   // theme-focusrand niet blauw
  lv_obj_add_event_cb(b, klik_, code, (void *) (intptr_t) actie);
  if (h >= 40) detail(b, 13, 0, w - 26, 1, LIJN, 42, 0);
  return b;
}

// Tegel op NOIR Thuis: icoon linksboven (afbeelding of MDI-teken), naam linksonder. h = hoogte, w = breedte.
inline lv_obj_t *tegel(lv_obj_t *body, const char *t, int x, int y, int actie, const void *icoon, const Fonts &f,
                       int w = 104, int h = 66, const char *mdi = nullptr) {
  lv_obj_t *b = knop(body, x, y, w, h, actie);
  const int iy = h >= 60 ? 10 : 8;
  if (icoon != nullptr) {
    lv_obj_t *im = lv_image_create(b);
    lv_image_set_src(im, icoon);
    lv_obj_set_pos(im, 13, iy);
    lv_obj_set_style_image_opa(im, 210, 0);
    lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
  } else if (mdi != nullptr && f.ico != nullptr) {
    lv_obj_t *i = lv_label_create(b);
    lv_label_set_text(i, mdi);
    lv_obj_set_style_text_font(i, f.ico, 0);
    lv_obj_set_style_text_color(i, k(TEXT2), 0);
    lv_obj_set_style_text_opa(i, 210, 0);
    lv_obj_set_pos(i, 12, iy - 1);
    lv_obj_remove_flag(i, LV_OBJ_FLAG_CLICKABLE);
  }
  label(b, t, 12, h - 25, w - 24, f.f14, TEXT2);
  return b;
}
static const char *ICO_SNEEUW = "\U000F0717";   // mdi:snowflake (airco)

// Terug / Thuis onderaan: donkerder vlak zonder verloop (simulator: p == scr && a == 1 || a == 100)
inline void onderknop(lv_obj_t *p, const char *ico, const char *t, int x, int actie, const Fonts &f) {
  lv_obj_t *b = knop(p, x, 270, 104, 42, actie);
  lv_obj_set_style_bg_opa(b, 180, 0);
  lv_obj_set_style_bg_color(b, k(SURF_DIM), 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_radius(b, rnd(14), 0);
  lv_obj_set_style_border_opa(b, 65, 0);
  lv_obj_t *rij = lv_obj_create(b);
  lv_obj_remove_style_all(rij);
  lv_obj_set_size(rij, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(rij, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rij, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(rij, 7, 0);
  lv_obj_remove_flag(rij, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_center(rij);
  if (f.ico != nullptr && ico != nullptr) {
    lv_obj_t *i = lv_label_create(rij);
    lv_label_set_text(i, ico);
    lv_obj_set_style_text_font(i, f.ico, 0);
    lv_obj_set_style_text_color(i, k(TEXT2), 0);
  }
  lv_obj_t *l = lv_label_create(rij);
  lv_label_set_text(l, t);
  if (f.f12 != nullptr) lv_obj_set_style_text_font(l, f.f12, 0);
  lv_obj_set_style_text_color(l, k(TEXT2), 0);
}

// MDI-codes in fonts/mdi-subset.ttf (font mdi_menu bevat ze)
static const char *ICO_COG = "\U000F0493";
static const char *ICO_LINKS = "\U000F004D";
static const char *ICO_HOME = "\U000F02DC";

// Lege NOIR-laag over page_home (volle 240 x 320, achtergrond + Obsidian-wallpaper)
inline lv_obj_t *laag_maken(lv_obj_t *home, const Beelden &bl) {
  lv_obj_t *laag = lv_obj_create(home);
  lv_obj_remove_style_all(laag);
  lv_obj_add_flag(laag, LV_OBJ_FLAG_IGNORE_LAYOUT);   // page_home heeft padding (st_screen): de laag dekt alles
  lv_obj_add_flag(laag, LV_OBJ_FLAG_FLOATING);
  // Posities van kinderen tellen vanaf de binnenrand (padding) van page_home: terugschuiven naar de schermhoek
  lv_obj_set_pos(laag, -lv_obj_get_style_pad_left(home, LV_PART_MAIN), -lv_obj_get_style_pad_top(home, LV_PART_MAIN));
  lv_obj_set_size(laag, 240, 320);
  lv_obj_add_flag(laag, LV_OBJ_FLAG_USER_4);   // Aggro600-skins (esphomeremote-skin.h) laten NOIR met rust
  const int wall = thema().wall;
  lv_obj_set_style_bg_color(laag, k(wall == 1 ? 0x101216 : BG), 0);   // simulator: wallpaper_mode 1 iets lichter
  lv_obj_set_style_bg_opa(laag, LV_OPA_COVER, 0);
  lv_obj_remove_flag(laag, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(laag, LV_OBJ_FLAG_CLICKABLE);   // tikken op de achtergrond gaat niet door naar de Aggro600-startpagina
  if (bl.wall != nullptr && wall != 2) {   // Obsidian-wallpaper: satijn 210, zacht 100, effen = geen
    lv_obj_t *bg = lv_image_create(laag);
    lv_image_set_src(bg, bl.wall);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_set_style_image_opa(bg, wall == 0 ? 210 : 100, 0);
    lv_obj_remove_flag(bg, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_add_flag(laag, LV_OBJ_FLAG_HIDDEN);
  return laag;
}

// Statusbalk (klok links, HA-punt, accu-% en accusymbool rechts) - een keer, hangt aan de zichtbare laag
inline void kop_maken(lv_obj_t *ouder, const Fonts &f) {
  Stand &s = st();
  lv_obj_t *kop = s.kop = lv_obj_create(ouder);
  lv_obj_remove_style_all(kop);
  lv_obj_set_pos(kop, 0, 0);
  lv_obj_set_size(kop, 240, 26);
  lv_obj_remove_flag(kop, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  s.klok = label(kop, "--:--", 12, 7, 60, f.f12, STAT);
  s.batt_txt = label(kop, "", 151, 7, 31, f.f12, STAT);
  lv_obj_set_style_text_align(s.batt_txt, LV_TEXT_ALIGN_RIGHT, 0);
  s.ha_dot = detail(kop, 143, 11, 3, 3, DOT, 45, LV_RADIUS_CIRCLE);
  lv_obj_t *bat = detail(kop, 192, 9, 25, 11, 0x08090B, 100, 3);
  lv_obj_set_style_border_width(bat, 1, 0);
  lv_obj_set_style_border_color(bat, k(BATT), 0);
  s.batt_fill = detail(bat, 3, 3, 15, 5, BATT_F, 220, 1);
  detail(kop, 218, 12, 2, 5, BATT, 180, 1);
}

inline lv_obj_t *body_maken(lv_obj_t *laag, bool scroll) {
  lv_obj_t *body = lv_obj_create(laag);
  lv_obj_remove_style_all(body);
  lv_obj_set_pos(body, 12, 30);
  lv_obj_set_size(body, 216, 230);
  lv_obj_set_style_pad_bottom(body, 6, 0);
  if (scroll) {
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLL_ELASTIC);
  } else {
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  }
  return body;
}

// ---- NOIR-builder (2026-11-22): tegels van NOIR Thuis uit de configurator --------------------------------------
// Pagina met id "noir" in het menu van de configurator (cfg.menu.pages). Elke tegel = een menu-item met x/y/w/h op
// het raster van de configurator (6 kolommen). NOIR: kolom = (216 + 8) / 6 px, rij = 31 px (h 2 = 56 px hoog).
// icon: "mdi:naam" (fonts/mdi-subset.ttf, zie components/menu/mdi_icons.h) of "noir:tv|audio|licht|scenes" (beelden).
// Wat een tik doet, regelt de firmware (tasten::noir_item_actie): page, special, action (steps), toggle/light/cover.
static const int RIJ = 31, RIJ_GAT = 6, KOL_GAT = 8, TEGELS_Y = 52;

inline std::string utf8_(uint32_t cp) {
  std::string o;
  if (cp < 0x80) o += (char) cp;
  else if (cp < 0x800) { o += (char) (0xC0 | (cp >> 6)); o += (char) (0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) { o += (char) (0xE0 | (cp >> 12)); o += (char) (0x80 | ((cp >> 6) & 0x3F)); o += (char) (0x80 | (cp & 0x3F)); }
  else { o += (char) (0xF0 | (cp >> 18)); o += (char) (0x80 | ((cp >> 12) & 0x3F)); o += (char) (0x80 | ((cp >> 6) & 0x3F)); o += (char) (0x80 | (cp & 0x3F)); }
  return o;
}
// "mdi:air-conditioner" -> UTF-8 van het teken in mdi_menu, "" = onbekend
inline std::string mdi_teken(const char *icon) {
  if (icon == nullptr || strncmp(icon, "mdi:", 4) != 0) return "";
  for (const auto &e : esphome::menue::MDI_ICONS)
    if (strcmp(e.name, icon + 4) == 0) return utf8_(e.cp);
  return "";
}
inline const void *noir_beeld(const char *icon, const Beelden &bl) {
  if (icon == nullptr || strncmp(icon, "noir:", 5) != 0) return nullptr;
  const char *n = icon + 5;
  if (!strcmp(n, "tv")) return bl.tv;
  if (!strcmp(n, "audio")) return bl.audio;
  if (!strcmp(n, "licht")) return bl.licht;
  if (!strcmp(n, "scenes")) return bl.scenes;
  return nullptr;
}

// Een tegel uit een item (JSON uit de configurator) op de NOIR-maat
inline void item_tegel(lv_obj_t *body, JsonObjectConst it, int index, const Fonts &f, const Beelden &bl) {
  const int x = std::max(0, std::min(5, (int) (it["x"] | 0))), w = std::max(1, std::min(6 - x, (int) (it["w"] | 6)));
  const int y = std::max(0, (int) (it["y"] | 0)), h = std::max(1, std::min(6, (int) (it["h"] | 2)));
  const int px = x * (216 + KOL_GAT) / 6, pw = (x + w) * (216 + KOL_GAT) / 6 - px - KOL_GAT;
  const int py = y * RIJ, ph = h * RIJ - RIJ_GAT;
  const char *label_t = it["label"] | "";
  const char *icon = it["icon"] | "";
  const std::string t = it["t"] | "";
  if (t == "text") {   // alleen tekst (kop/uitleg)
    lv_obj_t *l = label(body, label_t, px, py + 4, pw, h >= 2 ? f.f16 : f.f12, h >= 2 ? TEXT : MUTED);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    return;
  }
  const void *beeld = noir_beeld(icon, bl);
  const std::string teken = beeld == nullptr ? mdi_teken(icon) : std::string();
  if (ph >= 46) {   // groot: icoon linksboven, naam linksonder (zoals de simulator)
    tegel(body, label_t, px, py, ITEM + index, beeld, f, pw, ph, teken.empty() ? nullptr : teken.c_str());
    return;
  }
  // klein (1 rij): icoon en naam naast elkaar
  lv_obj_t *b = knop(body, px, py, pw, ph, ITEM + index);
  int lx = 10;
  if (beeld != nullptr || !teken.empty()) {
    lv_obj_t *i;
    if (beeld != nullptr) { i = lv_image_create(b); lv_image_set_src(i, beeld); lv_image_set_scale(i, 180); }
    else { i = lv_label_create(b); lv_label_set_text(i, teken.c_str()); if (f.ico != nullptr) lv_obj_set_style_text_font(i, f.ico, 0);
           lv_obj_set_style_text_color(i, k(TEXT2), 0); }
    lv_obj_align(i, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_remove_flag(i, LV_OBJ_FLAG_CLICKABLE);
    lx = 34;
  }
  lv_obj_t *l = label(b, label_t, lx, 0, pw - lx - 6, f.f12, TEXT2);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, lx, 0);
}

// NOIR Thuis (simulator build(0))
// items = tegels van de configuratorpagina "noir" (leeg = de ingebouwde tegels)
inline void thuis_bouwen(lv_obj_t *laag, const Fonts &f, const Beelden &bl, JsonArrayConst items = JsonArrayConst()) {
  Stand &s = st();
  lv_obj_t *body = s.thuis_body = body_maken(laag, true);
  lv_obj_set_height(body, 236);
  label(body, "NOIR / SIGNATURE", 0, 5, 180, f.f16, TEXT);
  label(body, "Home Control", 0, 30, 216, f.f12, MUTED);
  lv_obj_t *tw = knop(body, 184, 0, 32, 32, INSTELLINGEN);
  if (f.ico != nullptr) {
    lv_obj_t *i = lv_label_create(tw);
    lv_label_set_text(i, ICO_COG);
    lv_obj_set_style_text_font(i, f.ico, 0);
    lv_obj_set_style_text_color(i, k(TEXT2), 0);
    lv_obj_center(i);
  }
  if (!items.isNull() && items.size() > 0) {   // NOIR-builder: tegels uit de configurator
    lv_obj_t *raster = lv_obj_create(body);
    lv_obj_remove_style_all(raster);
    lv_obj_set_pos(raster, 0, TEGELS_Y);
    lv_obj_set_size(raster, 216, LV_SIZE_CONTENT);
    lv_obj_remove_flag(raster, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    int i = 0;
    for (JsonObjectConst it : items) {
      if (i >= 40) break;
      item_tegel(raster, it, i, f, bl);
      i++;
    }
    return;
  }
  // 2026-11-22: Airco erbij - drie rijen van 56 hoog (simulator: twee van 66), Airco over de volle breedte
  tegel(body, "TV", 0, 52, TV, bl.tv, f, 104, 56);
  tegel(body, "Audio", 112, 52, AUDIO, bl.audio, f, 104, 56);
  tegel(body, "Verlichting", 0, 114, LICHT, bl.licht, f, 104, 56);
  tegel(body, "Sc\xC3\xA8nes", 112, 114, SCENES, bl.scenes, f, 104, 56);
  tegel(body, "Airco", 0, 176, AIRCO, nullptr, f, 216, 50, ICO_SNEEUW);
}

static const char *ICO_POWER = "\U000F0425";

// NOIR TV (simulator build(1)): titel, status uit HA, aan/uit, 6 app-tegels (scrollt), Terug/Thuis
inline void tv_bouwen(lv_obj_t *laag, const Fonts &f, const Beelden &bl) {
  Stand &s = st();
  lv_obj_t *body = body_maken(laag, true);
  label(body, "TV", 0, 0, 150, f.f20 != nullptr ? f.f20 : f.f16, TEXT);
  s.tv_status = label(body, "Apparaten en apps", 0, 31, 160, f.f12, MUTED);
  lv_obj_t *pw = knop(body, 164, 20, 52, 34, TV_AANUIT);
  if (f.ico != nullptr) {
    lv_obj_t *i = lv_label_create(pw);
    lv_label_set_text(i, ICO_POWER);
    lv_obj_set_style_text_font(i, f.ico, 0);
    lv_obj_set_style_text_color(i, k(TEXT2), 0);
    lv_obj_center(i);
  }
  for (int i = 0; i < 6; i++) {
    const int x = (i % 2) * 112, y = 60 + (i / 2) * 80;
    lv_obj_t *b = knop(body, x, y, 104, 76, TV_APP + i);
    if (bl.app[i] != nullptr) {
      lv_obj_t *im = lv_image_create(b);
      lv_image_set_src(im, bl.app[i]);
      lv_obj_align(im, LV_ALIGN_TOP_MID, 0, 8);
      lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_t *c = label(b, "Cookie", 8, 17, 88, f.f16, TEXT);
      lv_obj_set_style_text_align(c, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_t *l = label(b, APP_NAAM[i], 3, 55, 98, f.f12, 0xBFC2C9);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  }
  onderknop(laag, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(laag, ICO_HOME, "Thuis", 124, THUIS, f);
}

// ---- NOIR Verlichting (2026-11-22, simulator build(3)) ---------------------------------------------------------
// Per lamp een brede knop "Naam  ·  Aan/Uit" of (dimbaar) "Naam  ·  65%". Tikken = aan/uit, lang drukken (dimbaar)
// = dimvenster met schuifregler (light.turn_on brightness_pct bij loslaten). De echte stand komt elke 3 s uit HA
// (dezelfde /api/esphomeremote/states als de menutegels), alleen zolang het scherm open is.
using Data = std::map<std::string, std::string>;
inline std::function<void(const std::string &, Data)> &ha();
inline void tv_status(const std::string &state, const std::string &bron);
struct Lamp {
  std::string naam, entiteit;
  bool dim{false};
  std::string state;   // "" = nog onbekend
  int bri{-1};         // 0-255, -1 = onbekend/uit
  lv_obj_t *lbl{nullptr};
};
inline std::vector<Lamp> &lampen() { static std::vector<Lamp> l; return l; }
inline bool lamp_aan(const Lamp &l) { return l.state == "on"; }
inline int lamp_pct(const Lamp &l) { return l.bri < 0 ? -1 : (l.bri * 100 + 127) / 255; }

inline void lamp_tekst(int i) {
  if (i < 0 || i >= (int) lampen().size()) return;
  Lamp &l = lampen()[i];
  if (l.lbl == nullptr) return;
  std::string w;
  if (l.state.empty()) w = "\xE2\x80\x94";                                   // —
  else if (l.state == "unavailable" || l.state == "unknown") w = "Niet bereikbaar";
  else if (!lamp_aan(l)) w = "Uit";
  else if (l.dim && lamp_pct(l) >= 0) w = std::to_string(std::max(1, lamp_pct(l))) + "%";
  else w = "Aan";
  const std::string t = l.naam + "  \xC2\xB7  " + w;
  if (t != lv_label_get_text(l.lbl)) lv_label_set_text(l.lbl, t.c_str());
  lv_obj_set_style_text_color(l.lbl, k(lamp_aan(l) ? 0xF2F3F5 : 0xE4E7EC), 0);
  lv_obj_set_style_text_opa(l.lbl, lamp_aan(l) || l.state.empty() ? 255 : 170, 0);
}
inline void lampen_tekst() { for (int i = 0; i < (int) lampen().size(); i++) lamp_tekst(i); }

// "Naam=entity|Naam=entity" (substitution noir_lampen) -> lampen()
inline void lampen_uit_tekst(const char *lijst) {
  lampen().clear();
  std::string l = lijst != nullptr ? lijst : "";
  while (!l.empty()) {
    const size_t p = l.find('|');
    const std::string d = l.substr(0, p);
    const size_t is = d.find('=');
    if (is != std::string::npos && is + 1 < d.size()) {
      Lamp a;
      a.naam = d.substr(0, is);
      a.entiteit = d.substr(is + 1);
      a.dim = a.entiteit.rfind("light.", 0) == 0;
      lampen().push_back(a);
    }
    if (p == std::string::npos) break;
    l = l.substr(p + 1);
  }
}
// Items van de configuratorpagina "noir_licht" (label + entity; dim: false = niet dimbaar) -> lampen()
inline bool lampen_uit_items(JsonArrayConst items) {
  if (items.isNull() || items.size() == 0) return false;
  std::vector<Lamp> neu;
  for (JsonObjectConst it : items) {
    const char *e = it["entity"] | "";
    if (!*e) continue;
    Lamp a;
    a.entiteit = e;
    a.naam = it["label"] | e;
    a.dim = a.entiteit.rfind("light.", 0) == 0 && (it["dim"] | true);
    for (const Lamp &o : lampen()) if (o.entiteit == a.entiteit) { a.state = o.state; a.bri = o.bri; }
    neu.push_back(a);
    if (neu.size() >= 12) break;
  }
  if (neu.empty()) return false;
  lampen() = neu;
  return true;
}

// HTTP-opvraag in een eigen taak (zoals Menue::abfrage_*): url_basis() = "http://host/api/esphomeremote/states?k=..."
namespace licht_http {
inline std::function<std::string()> &url_basis() { static std::function<std::string()> f; return f; }
struct Abfrage { volatile bool laeuft{false}, fertig{false}; std::string url; char *buf{nullptr}; size_t len{0}; int status{0};
                 void *taak{nullptr}; uint32_t volgende{0}; };
inline Abfrage &a() { static Abfrage x; return x; }
#ifdef ESP_PLATFORM
inline void einmal_() {
  Abfrage &q = a();
  esp_http_client_config_t cfg = {};
  cfg.url = q.url.c_str();
  cfg.timeout_ms = 3000;
  cfg.buffer_size = 512;
  cfg.buffer_size_tx = std::max<int>(512, (int) q.url.size() + 256);
  cfg.keep_alive_enable = false;
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  int status = -1;
  size_t len = 0;
  char *buf = nullptr;
  if (c != nullptr && esp_http_client_open(c, 0) == ESP_OK) {
    esp_http_client_fetch_headers(c);
    status = esp_http_client_get_status_code(c);
    if (status == 200) {
      const size_t MAX = 4096;
      buf = (char *) heap_caps_malloc(MAX, MALLOC_CAP_SPIRAM);
      while (buf != nullptr && len < MAX) {
        int r = esp_http_client_read(c, buf + len, MAX - len);
        if (r <= 0) break;
        len += r;
      }
    }
  }
  if (c != nullptr) { esp_http_client_close(c); esp_http_client_cleanup(c); }
  q.buf = buf; q.len = len; q.status = status; q.fertig = true;
}
inline void taak_(void *) { for (;;) { ulTaskNotifyTake(pdTRUE, portMAX_DELAY); einmal_(); } }
inline void starten_() {
  Abfrage &q = a();
  if (q.taak == nullptr) {
    static StaticTask_t tcb;
    const size_t groesse = 4096;
    auto *stapel = (StackType_t *) heap_caps_malloc(groesse, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (stapel != nullptr) q.taak = xTaskCreateStaticPinnedToCore(taak_, "noir_licht", groesse, nullptr, 1, stapel, &tcb, tskNO_AFFINITY);
  }
  if (q.taak == nullptr) return;
  q.laeuft = true;
  q.fertig = false;
  xTaskNotifyGive((TaskHandle_t) q.taak);
}
#else
inline void starten_() {}
#endif
// Antwort {"light.x": ["on", 128, -1, ""], ...} -> lampen()
inline void auswerten_() {
  Abfrage &q = a();
  if (q.status == 200 && q.buf != nullptr && q.len > 0) {
    JsonDocument doc;
    if (!deserializeJson(doc, q.buf, q.len)) {
      for (Lamp &l : lampen()) {
        JsonArrayConst v = doc[l.entiteit];
        if (v.isNull()) continue;
        l.state = v[0] | "";
        l.bri = v[1] | -1;
      }
      lampen_tekst();
    }
  }
#ifdef ESP_PLATFORM
  if (q.buf != nullptr) heap_caps_free(q.buf);
#endif
  q.buf = nullptr;
  q.len = 0;
}
// Elke 100 ms vanuit de firmware; open = NOIR Verlichting is te zien (scherm aan, startpagina)
inline void tik(uint32_t nu, bool open) {
  Abfrage &q = a();
  if (q.fertig) { q.fertig = false; q.laeuft = false; auswerten_(); }
  if (!open || q.laeuft || lampen().empty() || !url_basis() || (int32_t) (nu - q.volgende) < 0) return;
  q.volgende = nu + 3000;
  const std::string basis = url_basis()();
  if (basis.empty()) return;
  std::string lijst;
  for (const Lamp &l : lampen()) { lijst += l.entiteit; lijst += ","; }
  q.url = basis + "&e=" + lijst;
  starten_();
}
inline void snel(uint32_t nu, uint32_t ms) { a().volgende = nu + ms; }   // na een tik: stand kort daarna opnieuw
}  // namespace licht_http

inline void lamp_schakelen(int i, uint32_t nu) {
  if (i < 0 || i >= (int) lampen().size() || !ha()) return;
  Lamp &l = lampen()[i];
  const std::string &e = l.entiteit;
  const char *svc = e.rfind("light.", 0) == 0 ? "light.toggle" : e.rfind("switch.", 0) == 0 ? "switch.toggle" : "homeassistant.toggle";
  ha()(svc, {{"entity_id", e}});
  if (!l.state.empty()) { l.state = lamp_aan(l) ? "off" : "on"; lamp_tekst(i); }   // meteen tonen, HA bevestigt
  licht_http::snel(nu, 900);
}

// Dimvenster (simulator open_dimmer): over alles op lv_layer_top
struct Dimmer { lv_obj_t *over{nullptr}, *waarde{nullptr}; int lamp{-1}; };
inline Dimmer &dimmer() { static Dimmer d; return d; }
inline void dimmer_dicht() {
  Dimmer &d = dimmer();
  if (d.over != nullptr) { lv_obj_delete_async(d.over); d.over = nullptr; d.waarde = nullptr; }
  d.lamp = -1;
}
inline void dimmer_waarde_(lv_event_t *e) {
  lv_obj_t *sl = (lv_obj_t *) lv_event_get_target(e);
  const int v = lv_slider_get_value(sl);
  Dimmer &d = dimmer();
  if (d.waarde != nullptr) lv_label_set_text_fmt(d.waarde, "%d%%", v);
  if (lv_event_get_code(e) != LV_EVENT_RELEASED || d.lamp < 0 || d.lamp >= (int) lampen().size() || !ha()) return;
  Lamp &l = lampen()[d.lamp];
  if (v <= 0) { ha()("light.turn_off", {{"entity_id", l.entiteit}}); l.state = "off"; }
  else { ha()("light.turn_on", {{"entity_id", l.entiteit}, {"brightness_pct", std::to_string(v)}}); l.state = "on"; l.bri = (v * 255 + 50) / 100; }
  lamp_tekst(d.lamp);
  licht_http::snel(lv_tick_get(), 1500);
}
inline void dimmer_open(int i) {
  Stand &s = st();
  if (i < 0 || i >= (int) lampen().size() || !lampen()[i].dim || dimmer().over != nullptr) return;
  if (lv_indev_active() != nullptr) lv_indev_wait_release(lv_indev_active());   // loslaten = geen tik op de knop
  Lamp &l = lampen()[i];
  Dimmer &d = dimmer();
  d.lamp = i;
  d.over = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(d.over);
  lv_obj_add_flag(d.over, LV_OBJ_FLAG_USER_4);
  lv_obj_set_size(d.over, 240, 320);
  lv_obj_set_style_bg_color(d.over, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(d.over, 175, 0);
  lv_obj_add_flag(d.over, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(d.over, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *m = lv_obj_create(d.over);
  lv_obj_set_pos(m, 16, 80);
  lv_obj_set_size(m, 208, 162);
  lv_obj_set_style_bg_color(m, k(SURF), 0);
  lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(m, rnd(16), 0);
  lv_obj_set_style_border_width(m, 1, 0);
  lv_obj_set_style_border_color(m, k(RAND), 0);
  lv_obj_set_style_border_opa(m, LV_OPA_60, 0);
  lv_obj_set_style_pad_all(m, 0, 0);
  lv_obj_set_style_shadow_width(m, 0, 0);
  lv_obj_remove_flag(m, LV_OBJ_FLAG_SCROLLABLE);
  label(m, l.naam.c_str(), 16, 15, 176, s.fonts.f16, TEXT);
  const int pct = lamp_aan(l) ? std::max(1, lamp_pct(l) < 0 ? 100 : lamp_pct(l)) : 0;
  d.waarde = label(m, "", 16, 45, 176, s.fonts.f20 != nullptr ? s.fonts.f20 : s.fonts.f16, TEXT);
  lv_label_set_text_fmt(d.waarde, "%d%%", pct);
  lv_obj_t *sl = lv_slider_create(m);
  lv_obj_set_pos(sl, 18, 88);
  lv_obj_set_size(sl, 172, 5);
  lv_slider_set_range(sl, 0, 100);
  lv_slider_set_value(sl, pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(sl, k(0x36393F), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(sl, k(0xBFC2C9), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sl, k(0xF2F3F5), LV_PART_KNOB);
  lv_obj_set_style_pad_all(sl, 6, LV_PART_KNOB);
  lv_obj_set_style_outline_width(sl, 0, LV_STATE_FOCUSED);
  lv_obj_set_ext_click_area(sl, 14);
  lv_obj_add_event_cb(sl, dimmer_waarde_, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sl, dimmer_waarde_, LV_EVENT_RELEASED, nullptr);
  lv_obj_t *g = knop(m, 12, 116, 184, 32, DIM_GEREED);
  lv_obj_t *gl = label(g, "Gereed", 0, 0, 174, s.fonts.f12, 0xE4E7EC);
  lv_obj_set_style_text_align(gl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(gl);
}
inline void lang_(lv_event_t *e) { dimmer_open((int) (intptr_t) lv_event_get_user_data(e) - LAMP); }

inline void licht_bouwen(lv_obj_t *laag, const Fonts &f) {
  Stand &s = st();
  lv_obj_t *body = s.licht_body = body_maken(laag, true);
  label(body, "Verlichting", 0, 0, 216, f.f20 != nullptr ? f.f20 : f.f16, TEXT);
  if (lampen().empty()) {
    lv_obj_t *l = label(body, "Nog geen lampen. Configurator \xE2\x86\x92 NOIR \xE2\x86\x92 Verlichting.", 0, 42, 216, f.f12, MUTED);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  }
  for (int i = 0; i < (int) lampen().size(); i++) {
    Lamp &l = lampen()[i];
    lv_obj_t *b = knop(body, 0, 42 + i * 55, 216, 47, LAMP + i, LV_EVENT_SHORT_CLICKED);
    l.lbl = label(b, "", 0, 0, 200, f.f12, 0xE4E7EC);
    lv_obj_set_style_text_align(l.lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l.lbl);
    if (l.dim) {   // dimbaar: lichte rand, lang drukken = dimmer
      lv_obj_add_event_cb(b, lang_, LV_EVENT_LONG_PRESSED, (void *) (intptr_t) (LAMP + i));
      lv_obj_set_style_border_color(b, k(0xBFC2C9), 0);
      lv_obj_set_style_border_opa(b, 85, 0);
    }
  }
  if (!lampen().empty()) {
    lv_obj_t *h = label(body, "Lang drukken = dimmen", 0, 42 + (int) lampen().size() * 55, 216, f.f12, MUTED);
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
  }
  lampen_tekst();
}

// ---- NOIR Airco (2026-11-22): Mitsubishi Heavy Industries via infrarood ----------------------------------------
// Afstandsbediening RLA502A704A (SRK..ZS). De airco krijgt bij elke wijziging de HELE stand in een frame (152 bits):
// kop (5 bytes) | modus+aan | temperatuur | fan | 3D-auto | lamellen | staart, elke databyte als (omgekeerd, echt).
// Bits staan hieronder in zendvolgorde (zoals de decodering van deze afstandsbediening door de community:
// github.com/peterhinch/micropython_ir issue 46). Timing: kop 3200/1600 us, bit = 400 us aan + 1200 (1) of 430 (0) uit.
// De remote kan de stand van de airco niet uitlezen - hij onthoudt zelf wat hij het laatst stuurde (global).
namespace airco {

// fan: 0 auto, 1-4 stand, 5 HI POWER, 6 ECONO (zoals de knoppen HI en ECO op de afstandsbediening)
struct Stand { bool aan{false}; int modus{1}; int temp{22}; int fan{0}; int swing{0}; int fan_normaal{0}; };
inline Stand &st_() { static Stand s; return s; }
static const char *MODUS_NAAM[5] = {"Auto", "Koel", "Droog", "Fan", "Warm"};
static const char *MODUS_LANG[5] = {"Automatisch", "Koelen", "Ontvochtigen", "Ventileren", "Verwarmen"};
static const char *FAN_NAAM[7] = {"Auto", "1", "2", "3", "4", "HI", "Eco"};
static const char *SWING_NAAM[4] = {"Uit", "Op/neer", "Links/rechts", "Alles"};
static const int TEMP_MIN = 16, TEMP_MAX = 31;   // grenzen van het protocol; het bereik zelf staat in inst()

// Instellingen uit de configurator (cfg.noir_airco, 2026-11-22) - aanpassen zonder te compileren.
// Standaard = getest op de SRK..ZS met RLA502A704A.
struct Inst {
  int hi{9}, eco{6};          // fan-code van HI POWER en ECONO
  int hor{3}, ver{6};         // lamelstand zonder swing (horizontaal 0-8, verticaal 0-12)
  int hor_sw{0}, ver_sw{0};   // lamelstand bij swing
  int tmin{18}, tmax{30};     // temperatuurbereik
  int start{3200};            // startpuls in us
};
inline Inst &inst() { static Inst i; return i; }
inline void inst_laden(JsonObjectConst o) {
  Inst n;
  if (!o.isNull()) {
    auto lees = [&](const char *k, int &v, int lo, int hi) { if (o[k].is<int>()) v = std::max(lo, std::min(hi, (int) o[k])); };
    lees("hi", n.hi, 0, 15); lees("eco", n.eco, 0, 15);
    lees("hor", n.hor, 0, 8); lees("ver", n.ver, 0, 12);
    lees("hor_sw", n.hor_sw, 0, 8); lees("ver_sw", n.ver_sw, 0, 12);
    lees("tmin", n.tmin, TEMP_MIN, TEMP_MAX); lees("tmax", n.tmax, TEMP_MIN, TEMP_MAX);
    lees("start", n.start, 1000, 6000);
    if (n.tmax < n.tmin) std::swap(n.tmin, n.tmax);
  }
  inst() = n;
}

inline std::function<void(const std::string &)> &zender() { static std::function<void(const std::string &)> z; return z; }   // IR-stap (JSON)
inline std::function<void(int)> &opslaan() { static std::function<void(int)> z; return z; }

inline int inpakken() {
  const Stand &s = st_();
  return (s.aan ? 1 : 0) | (s.modus & 7) << 1 | ((s.temp - 16) & 31) << 4 | (s.fan & 7) << 9 | (s.swing & 3) << 12 | 1 << 15;
}
inline void uitpakken(int v) {
  if (!(v & (1 << 15))) return;   // nog nooit bewaard: standaard
  Stand &s = st_();
  s.aan = v & 1;
  s.modus = std::min(4, (v >> 1) & 7);
  s.temp = std::max(inst().tmin, std::min(inst().tmax, ((v >> 4) & 31) + 16));
  s.fan = std::min(6, (v >> 9) & 7);
  s.fan_normaal = std::min(4, s.fan);
  s.swing = (v >> 12) & 3;
}

// n bits van v, minst significante eerst (zoals de afstandsbediening)
inline std::string lsb_(int v, int n) { std::string o; for (int i = 0; i < n; i++) o += (v >> i) & 1 ? '1' : '0'; return o; }
inline std::string paar_(const std::string &b) { std::string o; for (char c : b) o += c == '1' ? '0' : '1'; return o + b; }

// Frame als bitreeks (zendvolgorde)
inline std::string bits() {
  const Stand &s = st_();
  std::string b = "0100101001110101110000110101100010100111";   // kop
  b += paar_(lsb_(s.modus, 3) + (s.aan ? "1" : "0") + "0000");
  b += paar_(lsb_(std::max(TEMP_MIN, std::min(TEMP_MAX, s.temp)) - 17, 4) + "0000");   // codeert 17 + 0..15
  b += paar_(lsb_(s.fan <= 4 ? s.fan : s.fan == 5 ? inst().hi : inst().eco, 4) + "0000");   // standaard HI = 9, ECO = 6 (getest)
  // lamellen: horizontaal 0 = zwenken, 3 = midden; verticaal 0 = zwenken, 6 = midden
  const int hor = (s.swing & 2) ? inst().hor_sw : inst().hor, ver = (s.swing & 1) ? inst().ver_sw : inst().ver;
  const std::string paneel = lsb_(hor, 4) + lsb_(ver, 4);
  b += paar_("00000" + paneel.substr(5));   // 3D-auto uit
  b += paar_(paneel);
  b += "11111111000000001111111000000001";   // staart
  return b;
}
// IR-stap voor tasten_konfig (zelfde sendweg als de andere IR-knoppen: zelf, dock of HA-blaster)
inline std::string ir_json() {
  std::string j = "{\"t\":\"ir\",\"f\":38000,\"raw\":[" + std::to_string(inst().start) + ",-1600";
  for (char c : bits()) { j += ",400,"; j += c == '1' ? "-1200" : "-430"; }
  j += ",400]}";
  return j;
}

inline lv_timer_t *&timer_() { static lv_timer_t *t = nullptr; return t; }
inline void nu_sturen() {
  if (zender()) zender()(ir_json());
  if (opslaan()) opslaan()(inpakken());
}
// Na een wijziging kort wachten (vasthouden van + stuurt niet elke stap)
inline void sturen(uint32_t ms = 600) {
  if (timer_() != nullptr) { lv_timer_reset(timer_()); return; }
  timer_() = lv_timer_create([](lv_timer_t *) { timer_() = nullptr; nu_sturen(); }, ms, nullptr);
  lv_timer_set_repeat_count(timer_(), 1);
}

// Scherm-objecten
struct Ui { lv_obj_t *temp{nullptr}, *status{nullptr}, *aan{nullptr}, *fan{nullptr}, *swing{nullptr}, *hi{nullptr}, *eco{nullptr}; lv_obj_t *modus[5]{}; };
inline Ui &ui() { static Ui u; return u; }

inline void tonen() {
  Ui &u = ui();
  const Stand &s = st_();
  if (u.temp == nullptr) return;
  char t[48];
  snprintf(t, sizeof(t), "%d", s.temp);
  lv_label_set_text(u.temp, t);
  lv_obj_set_style_text_opa(u.temp, s.aan ? 255 : 110, 0);
  if (!s.aan) snprintf(t, sizeof(t), "Uit");
  else if (s.fan == 5) snprintf(t, sizeof(t), "%s \xC2\xB7 HI POWER", MODUS_LANG[s.modus]);
  else if (s.fan == 6) snprintf(t, sizeof(t), "%s \xC2\xB7 Eco", MODUS_LANG[s.modus]);
  else snprintf(t, sizeof(t), "%s \xC2\xB7 fan %s", MODUS_LANG[s.modus], FAN_NAAM[s.fan]);
  lv_label_set_text(u.status, t);
  lv_obj_set_style_border_color(u.aan, k(s.aan ? 0xBFC2C9 : RAND), 0);
  lv_obj_set_style_border_opa(u.aan, s.aan ? 220 : 115, 0);
  for (int i = 0; i < 5; i++) {
    if (u.modus[i] == nullptr) continue;
    const bool sel = i == s.modus;
    lv_obj_set_style_border_color(u.modus[i], k(sel ? 0xBFC2C9 : RAND), 0);
    lv_obj_set_style_border_opa(u.modus[i], sel ? 220 : 115, 0);
    lv_obj_t *l = lv_obj_get_child(u.modus[i], -1);
    if (l != nullptr) lv_obj_set_style_text_opa(l, sel ? 255 : 160, 0);
  }
  for (int i = 0; i < 2; i++) {   // HI / ECO: lichte rand als actief
    lv_obj_t *b = i == 0 ? u.hi : u.eco;
    if (b == nullptr) continue;
    const bool sel = s.fan == 5 + i;
    lv_obj_set_style_border_color(b, k(sel ? 0xBFC2C9 : RAND), 0);
    lv_obj_set_style_border_opa(b, sel ? 220 : 115, 0);
  }
  snprintf(t, sizeof(t), "Fan  %s", FAN_NAAM[s.fan <= 4 ? s.fan : s.fan_normaal]);
  lv_label_set_text(lv_obj_get_child(u.fan, -1), t);
  snprintf(t, sizeof(t), "Swing  %s", SWING_NAAM[s.swing]);
  lv_label_set_text(lv_obj_get_child(u.swing, -1), t);
}

// Acties (AIRCO_* in Actie)
inline void temp_stap(int d) {
  Stand &s = st_();
  s.temp = std::max(inst().tmin, std::min(inst().tmax, s.temp + d));
  if (!s.aan) s.aan = true;
  tonen();
  sturen();
}
inline void aanuit() { st_().aan = !st_().aan; tonen(); sturen(150); }
// Altijd uit sturen, ook als de remote denkt dat hij al uit staat (bv. aangezet met de originele afstandsbediening).
// Het MHI-frame bevat de volledige stand, dus nogmaals 'uit' sturen is onschuldig.
inline void uit() { st_().aan = false; tonen(); sturen(150); }
inline void modus(int m) { st_().modus = m; st_().aan = true; tonen(); sturen(150); }
inline void fan_volgende() {
  Stand &s = st_();
  s.fan = s.fan <= 4 ? (s.fan + 1) % 5 : s.fan_normaal;   // uit HI/ECO terug naar de gewone stand
  s.fan_normaal = s.fan;
  tonen();
  sturen();
}
// HI POWER / ECONO aan of uit (n = 5 of 6); uit = terug naar de laatste gewone fanstand
inline void fan_speciaal(int n) {
  Stand &s = st_();
  if (s.fan <= 4) s.fan_normaal = s.fan;
  s.fan = s.fan == n ? s.fan_normaal : n;
  if (!s.aan) s.aan = true;
  tonen();
  sturen(150);
}
inline void swing_volgende() { st_().swing = (st_().swing + 1) % 4; tonen(); sturen(); }

}  // namespace airco

static const char *ICO_MIN = "\U000F0374", *ICO_PLUS = "\U000F0415";   // mdi:minus, mdi:plus

inline lv_obj_t *icoon_knop_(lv_obj_t *p, int x, int y, int w, int h, int actie, const char *ico, const Fonts &f) {
  lv_obj_t *b = knop(p, x, y, w, h, actie);
  if (f.ico != nullptr) {
    lv_obj_t *i = lv_label_create(b);
    lv_label_set_text(i, ico);
    lv_obj_set_style_text_font(i, f.ico, 0);
    lv_obj_set_style_text_color(i, k(TEXT2), 0);
    lv_obj_center(i);
  }
  return b;
}
inline lv_obj_t *tekst_knop_(lv_obj_t *p, int x, int y, int w, int h, int actie, const char *t, const Fonts &f) {
  lv_obj_t *b = knop(p, x, y, w, h, actie);
  lv_obj_t *l = label(b, t, 0, 0, w - 2, f.f12, 0xE4E7EC);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(l);
  return b;
}

// NOIR Airco: titel + aan/uit, status, - temperatuur +, modus (5), fan en swing (wisselknoppen)
inline void airco_bouwen(lv_obj_t *laag, const Fonts &f) {
  airco::Ui &u = airco::ui();
  lv_obj_t *body = body_maken(laag, false);
  label(body, "Airco", 0, 0, 150, f.f20 != nullptr ? f.f20 : f.f16, TEXT);
  u.status = label(body, "", 0, 26, 160, f.f12, MUTED);
  u.aan = icoon_knop_(body, 164, 0, 52, 34, AIRCO_AANUIT, ICO_POWER, f);
  icoon_knop_(body, 0, 46, 58, 58, AIRCO_MIN, ICO_MIN, f);
  icoon_knop_(body, 158, 46, 58, 58, AIRCO_PLUS, ICO_PLUS, f);
  const lv_font_t *groot = f.groot != nullptr ? f.groot : (f.f20 != nullptr ? f.f20 : f.f16);
  u.temp = label(body, "", 62, 46 + (58 - lv_font_get_line_height(groot)) / 2, 82, groot, TEXT);
  lv_obj_set_style_text_align(u.temp, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_t *graad = label(body, "\xC2\xB0" "C", 140, 52, 20, f.f12, MUTED);
  (void) graad;
  const int bw = (216 - 4 * 4) / 5;
  for (int i = 0; i < 5; i++)
    u.modus[i] = tekst_knop_(body, i * (bw + 4), 114, bw, 34, AIRCO_MODUS + i, airco::MODUS_NAAM[i], f);
  u.fan = tekst_knop_(body, 0, 158, 104, 34, AIRCO_FAN, "", f);
  u.swing = tekst_knop_(body, 112, 158, 104, 34, AIRCO_SWING, "", f);
  u.hi = tekst_knop_(body, 0, 198, 104, 30, AIRCO_HI, "HI POWER", f);
  u.eco = tekst_knop_(body, 112, 198, 104, 30, AIRCO_ECO, "ECO", f);
  airco::tonen();
}

// ---- NOIR Knoppentest (2026-11-22): D-pad en OK testen zonder tv/HA ---------------------------------------------
// Zelfde weg als bij normaal gebruik (on_key -> driver met entprellen -> hier), maar er gaat niets naar buiten.
// Raster 3 x 3: pijlen verplaatsen de cursor, OK telt op de tegel. "Verdacht" = dezelfde knop opnieuw ingedrukt
// binnen 120 ms na loslaten (zo snel drukt niemand - dat is dender of een dubbel gelezen druk).
namespace knoptest {
struct Stand {
  int cur{4};
  int tel[6]{};                 // ^ v < > OK anders
  int ok_op[9]{};
  int verdacht{0};
  uint32_t los_ms[128]{}, druk_ms[128]{};
  std::string laatste{"Druk op een pijl of OK"};
  lv_obj_t *tegel[9]{}, *info{nullptr}, *teller{nullptr}, *verd{nullptr}, *groot{nullptr};
  lv_timer_t *flits{nullptr};
  int flits_tegel{-1};
};
inline Stand &st_() { static Stand s; return s; }
inline const char *naam_(int t) {
  switch (t) { case 14: return "Op"; case 35: return "Neer"; case 15: return "Links"; case 32: return "Rechts";
               case 34: return "OK"; default: return nullptr; }
}
inline void tonen() {
  Stand &s = st_();
  if (s.info == nullptr) return;
  for (int i = 0; i < 9; i++) {
    if (s.tegel[i] == nullptr) continue;
    const bool sel = i == s.cur;
    lv_obj_set_style_border_color(s.tegel[i], k(sel ? 0xBFC2C9 : RAND), 0);
    lv_obj_set_style_border_opa(s.tegel[i], sel ? 230 : 90, 0);
    lv_obj_set_style_border_width(s.tegel[i], sel ? 2 : 1, 0);
    lv_label_set_text_fmt(lv_obj_get_child(s.tegel[i], -1), "%d", s.ok_op[i]);
  }
  lv_label_set_text(s.info, s.laatste.c_str());
  lv_label_set_text_fmt(s.teller, "Op %d   Neer %d   Links %d   Rechts %d\nOK %d   overige %d",
                        s.tel[0], s.tel[1], s.tel[2], s.tel[3], s.tel[4], s.tel[5]);
  lv_label_set_text_fmt(s.verd, "Verdacht (dubbel < 120 ms): %d", s.verdacht);
  lv_obj_set_style_text_color(s.verd, k(s.verdacht ? 0xF2F3F5 : 0x969AA3), 0);
}
// OK: tegel licht 180 ms op
inline void flits(int i) {
  Stand &s = st_();
  if (i < 0 || i > 8 || s.tegel[i] == nullptr) return;
  if (s.flits != nullptr) { lv_timer_delete(s.flits); s.flits = nullptr; if (s.flits_tegel >= 0 && s.tegel[s.flits_tegel]) lv_obj_remove_state(s.tegel[s.flits_tegel], LV_STATE_PRESSED); }
  s.flits_tegel = i;
  lv_obj_add_state(s.tegel[i], LV_STATE_PRESSED);   // zelfde look als aantikken
  s.flits = lv_timer_create([](lv_timer_t *) {
    Stand &z = st_();
    if (z.flits_tegel >= 0 && z.tegel[z.flits_tegel] != nullptr) lv_obj_remove_state(z.tegel[z.flits_tegel], LV_STATE_PRESSED);
    z.flits = nullptr;
    z.flits_tegel = -1;
  }, 180, nullptr);
  lv_timer_set_repeat_count(s.flits, 1);
}
inline void reset() {
  Stand &s = st_();
  for (int &v : s.tel) v = 0;
  for (int &v : s.ok_op) v = 0;
  s.verdacht = 0;
  s.cur = 4;
  s.laatste = "Teller op nul";
  tonen();
}
// Uit on_key (via tv::taste). true = afgehandeld; de menutoets (31) blijft vrij om eruit te komen.
inline bool taste(int t, bool ingedrukt) {
  if (t == 31 || t < 1 || t > 127) return false;
  Stand &s = st_();
  const uint32_t nu = lv_tick_get();
  if (!ingedrukt) {
    s.los_ms[t] = nu;
    const char *n = naam_(t);
    char b[64];
    snprintf(b, sizeof(b), "%s%s losgelaten na %u ms", n ? n : "Toets ", n ? "" : std::to_string(t).c_str(), (unsigned) (nu - s.druk_ms[t]));
    s.laatste = b;
    tonen();
    return true;
  }
  const bool dubbel = s.los_ms[t] != 0 && nu - s.los_ms[t] < 120;
  if (dubbel) s.verdacht++;
  s.druk_ms[t] = nu;
  int r = s.cur / 3, c = s.cur % 3;
  switch (t) {
    case 14: s.tel[0]++; r = (r + 2) % 3; break;
    case 35: s.tel[1]++; r = (r + 1) % 3; break;
    case 15: s.tel[2]++; c = (c + 2) % 3; break;
    case 32: s.tel[3]++; c = (c + 1) % 3; break;
    case 34: s.tel[4]++; s.ok_op[s.cur]++; flits(s.cur); break;
    default: s.tel[5]++; break;
  }
  s.cur = r * 3 + c;
  char b[64];
  const char *n = naam_(t);
  if (n) snprintf(b, sizeof(b), "%s ingedrukt%s", n, dubbel ? "  \xE2\x80\x94 VERDACHT" : "");
  else snprintf(b, sizeof(b), "Toets %d ingedrukt%s", t, dubbel ? "  \xE2\x80\x94 VERDACHT" : "");
  s.laatste = b;
  if (s.groot != nullptr) {   // laatste knop groot in beeld (rood-wit bij verdacht)
    lv_label_set_text(s.groot, n ? n : std::to_string(t).c_str());
    lv_obj_set_style_text_opa(s.groot, 255, 0);
  }
  tonen();
  return true;
}
}  // namespace knoptest

inline void test_bouwen(lv_obj_t *laag, const Fonts &f) {
  knoptest::Stand &s = knoptest::st_();
  lv_obj_t *body = body_maken(laag, false);
  label(body, "Knoppentest", 0, 0, 150, f.f20 != nullptr ? f.f20 : f.f16, TEXT);
  tekst_knop_(body, 150, 0, 66, 30, TEST_RESET, "Reset", f);
  // raster links (3 x 3, 44 px), laatste knop groot rechts
  for (int i = 0; i < 9; i++) {
    lv_obj_t *b = knop(body, (i % 3) * 46, 36 + (i / 3) * 40, 42, 34, -1);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);   // alleen met de knoppen
    lv_obj_t *l = label(b, "0", 0, 0, 38, f.f14, 0xE4E7EC);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    s.tegel[i] = b;
  }
  s.groot = label(body, "", 140, 70, 76, f.f20 != nullptr ? f.f20 : f.f16, TEXT);
  lv_obj_set_style_text_align(s.groot, LV_TEXT_ALIGN_CENTER, 0);
  label(body, "laatste", 140, 100, 76, f.f12, MUTED);
  lv_obj_set_style_text_align(lv_obj_get_child(body, -1), LV_TEXT_ALIGN_CENTER, 0);
  s.info = label(body, "", 0, 156, 216, f.f12, TEXT);
  s.teller = label(body, "", 0, 174, 216, f.f12, MUTED);
  lv_label_set_long_mode(s.teller, LV_LABEL_LONG_MODE_WRAP);
  s.verd = label(body, "", 0, 212, 216, f.f12, MUTED);
  knoptest::tonen();
}

// ---- NOIR Scènes (2026-11-22) ---------------------------------------------------------------------------------
// Tegels = items van de configuratorpagina "noir_scenes" (naam, icoon, stappen: HA, IR, wachten ...) - zonder
// compileren aan te passen. Vooraan altijd de ingebouwde scène "Alles uit" (lampen, tv, KEF en airco uit).
static const char *ICO_UIT = "\U000F0425";   // mdi:power
inline void scenes_bouwen(lv_obj_t *laag, const Fonts &f, const Beelden &bl, JsonArrayConst items) {
  Stand &s = st();
  lv_obj_t *body = s.scenes_body = body_maken(laag, true);
  label(body, "Sc\xC3\xA8nes", 0, 0, 216, f.f20 != nullptr ? f.f20 : f.f16, TEXT);
  int n = 0;
  auto plek = [&](int i, int &x, int &y) { x = (i % 2) * 112; y = 36 + (i / 2) * 64; };
  int x, y;
  plek(n++, x, y);
  tegel(body, "Alles uit", x, y, SCENE_ALLES_UIT, nullptr, f, 104, 56, ICO_UIT);
  int i = 0;
  for (JsonObjectConst it : items) {
    if (i >= 30) break;
    const char *naam = it["label"] | "";
    if (!*naam && (it["t"] | std::string()) == "text") { i++; continue; }
    const char *icon = it["icon"] | "";
    const void *beeld = noir_beeld(icon, bl);
    const std::string teken = beeld == nullptr ? mdi_teken(icon) : std::string();
    plek(n++, x, y);
    tegel(body, naam, x, y, SCENE + i, beeld, f, 104, 56, teken.empty() ? nullptr : teken.c_str());
    i++;
  }
  if (items.isNull() || items.size() == 0) {
    plek(n, x, y);
    lv_obj_t *h = label(body, "Eigen sc\xC3\xA8nes: configurator \xE2\x86\x92 NOIR \xE2\x86\x92 Sc\xC3\xA8nes.", 0, y + 8, 216, f.f12, MUTED);
    lv_label_set_long_mode(h, LV_LABEL_LONG_MODE_WRAP);
  }
}

// ---- NOIR Netflix-dashboard (2026-11-22) -----------------------------------------------------------------------
// Achter de Netflix-tegel van het TV-scherm. Posterrijen (horizontaal vegen): Top 10 in Nederland (officiele
// weeklijst van Netflix), Populair op Netflix (TMDB, dagelijks) en Laatst gestart (de laatste 4 via de remote).
// Tik = detailscherm met achtergrond, tekst en "Afspelen in Netflix". Alle gegevens en beelden komen van de
// integratie (netflix.py) via HA (/api/esphomeremote/netflix...): de remote komt niet zelf op internet.
// Bediening van de tv met de fysieke knoppen (play/pauze, stop, spoelen, D-pad, OK) - zie tv::taste.
static const uint32_t NETFLIX_ROOD = 0xE50914;
namespace nf {
struct Titel { int id{0}; std::string naam, zoek, jaar, genre, leeftijd, tekst, netflix; int seizoenen{0}; };   // netflix = titelnummer (Wikidata)
struct Beeld { uint8_t *px{nullptr}; lv_image_dsc_t dsc{}; };
struct Wacht { lv_obj_t *img; int id; char soort; };
struct Taak { std::string url; int id; char soort; size_t max; };   // soort 'j' json, 'p' poster, 'b' achtergrond
struct Data {
  std::string week;
  std::vector<Titel> top10, populair, laatst;
  bool geladen{false}, bezig{false};
  uint32_t volgende{0};             // wanneer de lijsten opnieuw mogen
  std::map<int, Beeld> posters;
  Beeld achter; int achter_id{0};
  std::vector<Wacht> wacht_dash, wacht_detail;   // beeld-objecten die op een download wachten
  std::vector<Taak> rij;            // downloads
  std::set<int> gevraagd;           // posters die al in de rij staan/stonden
  Titel detail; int detail_plek{0}; // plek in de Top 10 (1-10), 0 = niet
  std::function<void(const std::string &)> bewaar;   // laatst gestart -> global
};
inline Data &d() { static Data x; return x; }
inline std::function<std::string()> &url_basis() { static std::function<std::string()> f; return f; }   // ".../states?k=.."

// ---- downloads (eigen taak, een tegelijk) ----
struct Abfrage { volatile bool laeuft{false}, fertig{false}; Taak taak; uint8_t *buf{nullptr}; size_t len{0}; int status{0};
                 void *handle{nullptr}; };
inline Abfrage &q() { static Abfrage x; return x; }
#ifdef ESP_PLATFORM
inline void einmal_() {
  Abfrage &a = q();
  esp_http_client_config_t cfg = {};
  cfg.url = a.taak.url.c_str();
  cfg.timeout_ms = 6000;
  cfg.buffer_size = 1024;
  cfg.buffer_size_tx = std::max<int>(512, (int) a.taak.url.size() + 256);
  cfg.keep_alive_enable = false;
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  int status = -1;
  size_t len = 0;
  uint8_t *buf = nullptr;
  if (c != nullptr && esp_http_client_open(c, 0) == ESP_OK) {
    esp_http_client_fetch_headers(c);
    status = esp_http_client_get_status_code(c);
    if (status == 200) {
      const size_t MAX = a.taak.max;
      buf = (uint8_t *) heap_caps_malloc(MAX + 1, MALLOC_CAP_SPIRAM);
      while (buf != nullptr && len < MAX) {
        int r = esp_http_client_read(c, (char *) buf + len, MAX - len);
        if (r <= 0) break;
        len += r;
      }
    }
  }
  if (c != nullptr) { esp_http_client_close(c); esp_http_client_cleanup(c); }
  a.buf = buf; a.len = len; a.status = status; a.fertig = true;
}
inline void taak_(void *) { for (;;) { ulTaskNotifyTake(pdTRUE, portMAX_DELAY); einmal_(); } }
inline void starten_(const Taak &t) {
  Abfrage &a = q();
  if (a.handle == nullptr) {
    static StaticTask_t tcb;
    const size_t groesse = 4096;
    auto *stapel = (StackType_t *) heap_caps_malloc(groesse, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (stapel != nullptr) a.handle = xTaskCreateStaticPinnedToCore(taak_, "noir_nf", groesse, nullptr, 1, stapel, &tcb, tskNO_AFFINITY);
  }
  if (a.handle == nullptr) return;
  a.taak = t;
  a.laeuft = true;
  a.fertig = false;
  xTaskNotifyGive((TaskHandle_t) a.handle);
}
inline void vrij_(void *p) { if (p != nullptr) heap_caps_free(p); }
#else
inline void starten_(const Taak &) {}
inline void vrij_(void *p) { free(p); }
#endif
inline std::string url_(const char *pad, const std::string &extra) {
  if (!url_basis()) return "";
  std::string u = url_basis()();
  const size_t p = u.find("/states?");
  if (p == std::string::npos) return "";
  return u.substr(0, p) + pad + u.substr(p + 8) + extra;   // ".../netflix?k=.." + extra
}
inline void beeld_vullen_(Beeld &b, uint8_t *px, int w, int h) {
  b.px = px;
  b.dsc = {};
  b.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  b.dsc.header.cf = LV_COLOR_FORMAT_RGB565;
  b.dsc.header.w = w;
  b.dsc.header.h = h;
  b.dsc.header.stride = w * 2;
  b.dsc.data_size = w * h * 2;
  b.dsc.data = px;
}
inline void tonen_(std::vector<Wacht> &lijst, int id, char soort, const lv_image_dsc_t *dsc) {
  for (auto &w : lijst)
    if (w.id == id && w.soort == soort && w.img != nullptr) { lv_image_set_src(w.img, dsc); lv_obj_remove_flag(w.img, LV_OBJ_FLAG_HIDDEN); }
}
inline void poster_vragen(int id) {
  Data &x = d();
  if (id <= 0 || x.posters.count(id) || x.gevraagd.count(id)) return;
  x.gevraagd.insert(id);
  x.rij.push_back({url_("/netflix/img?", "&s=p&id=" + std::to_string(id)), id, 'p', 64 * 96 * 2});
}
inline void achter_vragen(int id) {
  Data &x = d();
  if (id <= 0 || x.achter_id == id) return;
  x.rij.push_back({url_("/netflix/img?", "&s=b&id=" + std::to_string(id)), id, 'b', 216 * 110 * 2});
}
inline Titel titel_uit_(JsonObjectConst o) {
  Titel t;
  t.id = o["i"] | 0; t.naam = o["n"] | ""; t.zoek = o["z"] | t.naam.c_str(); t.jaar = o["j"] | "";
  t.genre = o["g"] | ""; t.leeftijd = o["l"] | ""; t.tekst = o["o"] | ""; t.seizoenen = o["s"] | 0;
  return t;
}
inline void herbouwen_();   // verderop (netflix_herbouwen)
inline void klaar_() {
  Abfrage &a = q();
  Data &x = d();
  const Taak t = a.taak;
  if (t.soort == 'j') {
    // Tekstregels van HA (geen ArduinoJson: een tweede JSON-variant gaf "l32r: literal target out of range" bij het
    // linken): "w\t<week>" en "t|p\t<id>\t<naam>\t<zoek>\t<jaar>\t<genre>\t<seizoenen>\t<leeftijd>\t<tekst>"
    x.bezig = false;
    if (a.status == 200 && a.buf != nullptr && a.len > 0) {
      x.week.clear(); x.top10.clear(); x.populair.clear();
      x.gevraagd.clear();   // posters die eerder mislukten (404, time-out) bij nieuwe lijsten opnieuw vragen
      const char *p = (const char *) a.buf, *eind = p + a.len;
      while (p < eind) {
        const char *e = (const char *) memchr(p, '\n', eind - p);
        if (e == nullptr) e = eind;
        std::vector<std::string> v;
        const char *q2 = p;
        for (const char *c = p; c <= e; c++)
          if (c == e || *c == '\t') { v.emplace_back(q2, c - q2); q2 = c + 1; }
        if (!v.empty() && v[0] == "w" && v.size() > 1) x.week = v[1];
        else if (!v.empty() && (v[0] == "t" || v[0] == "p") && v.size() >= 9) {
          Titel ti;
          ti.id = atoi(v[1].c_str()); ti.naam = v[2]; ti.zoek = v[3].empty() ? v[2] : v[3]; ti.jaar = v[4];
          ti.genre = v[5]; ti.seizoenen = atoi(v[6].c_str()); ti.leeftijd = v[7]; ti.tekst = v[8];
          if (v.size() > 9) ti.netflix = v[9];
          (v[0] == "t" ? x.top10 : x.populair).push_back(ti);
        }
        p = e + 1;
      }
      x.geladen = true;
      herbouwen_();
    }
    vrij_(a.buf);
  } else if (t.soort == 'p') {
    if (a.status == 200 && a.buf != nullptr && a.len == t.max) {
      Beeld &b = x.posters[t.id];
      beeld_vullen_(b, a.buf, 64, 96);
      tonen_(x.wacht_dash, t.id, 'p', &b.dsc);
      tonen_(x.wacht_detail, t.id, 'p', &b.dsc);
    } else vrij_(a.buf);
  } else if (t.soort == 'b') {
    if (a.status == 200 && a.buf != nullptr && a.len == t.max) {
      vrij_(x.achter.px);
      beeld_vullen_(x.achter, a.buf, 216, 110);
      x.achter_id = t.id;
      tonen_(x.wacht_detail, t.id, 'b', &x.achter.dsc);
    } else vrij_(a.buf);
  }
  a.buf = nullptr; a.len = 0;
}
// Elke 100 ms (via licht_http::tik). zichtbaar = NOIR Netflix of het detailscherm staat open.
inline void tik(uint32_t nu, bool zichtbaar) {
  Abfrage &a = q();
  Data &x = d();
  if (a.fertig) { a.fertig = false; a.laeuft = false; klaar_(); }
  if (zichtbaar && !x.bezig && (int32_t) (nu - x.volgende) >= 0) {   // lijsten: bij openen, daarna elk uur
    const std::string u = url_("/netflix?", "&f=tsv");
    if (!u.empty()) {
      x.bezig = true;
      bool zonder = !x.geladen || x.top10.empty();   // nog niets of titels zonder TMDB: vaker vragen
      for (auto &t : x.top10) if (t.id <= 0) zonder = true;
      x.volgende = nu + (zonder ? 60000u : 3600000u);
      x.rij.insert(x.rij.begin(), {u, 0, 'j', 24 * 1024});
    }
  }
  if (a.laeuft || x.rij.empty() || !zichtbaar) return;
  Taak t = x.rij.front();
  x.rij.erase(x.rij.begin());
  if (t.url.empty()) { if (t.soort == 'j') x.bezig = false; return; }
  starten_(t);
}
// Laatst gestart: "id\tzoeknaam\tnetflixnr\n" x4 (global van max. 254 tekens, overleeft herstarten)
inline void laatst_laden(const std::string &s) {
  Data &x = d();
  x.laatst.clear();
  size_t p = 0;
  while (p < s.size() && x.laatst.size() < 4) {
    size_t e = s.find('\n', p);
    if (e == std::string::npos) e = s.size();
    const std::string r = s.substr(p, e - p);
    const size_t t1 = r.find('\t');
    if (t1 != std::string::npos && t1 + 1 < r.size()) {
      Titel t;
      t.id = atoi(r.substr(0, t1).c_str()); t.zoek = r.substr(t1 + 1);
      const size_t t2 = t.zoek.find('\t');
      if (t2 != std::string::npos) { t.netflix = t.zoek.substr(t2 + 1); t.zoek = t.zoek.substr(0, t2); }
      t.naam = t.zoek;
      x.laatst.push_back(t);
    }
    p = e + 1;
  }
}
inline void laatst_toevoegen(const Titel &t) {
  Data &x = d();
  for (size_t i = 0; i < x.laatst.size(); i++)
    if ((t.id > 0 && x.laatst[i].id == t.id) || x.laatst[i].zoek == t.zoek) { x.laatst.erase(x.laatst.begin() + i); break; }
  x.laatst.insert(x.laatst.begin(), t);
  if (x.laatst.size() > 4) x.laatst.resize(4);
  std::string s;
  for (auto &l : x.laatst) s += std::to_string(l.id) + "\t" + l.zoek.substr(0, 34) + "\t" + l.netflix.substr(0, 10) + "\n";
  if (x.bewaar) x.bewaar(s);
}
// Titel bij een actie (lijst*100 + i); volledige gegevens uit Top 10/Populair als de laatst-gestart-regel kort is
inline bool titel_van(int code, Titel &t, int &plek) {
  Data &x = d();
  const int lijst = code / 100, i = code % 100;
  const std::vector<Titel> *v = lijst == 0 ? &x.top10 : lijst == 1 ? &x.populair : &x.laatst;
  if (i < 0 || i >= (int) v->size()) return false;
  t = (*v)[i];
  plek = lijst == 0 ? i + 1 : 0;
  if (lijst == 2) {
    for (int k = 0; k < (int) x.top10.size(); k++) if (x.top10[k].id == t.id && t.id > 0) { t = x.top10[k]; plek = k + 1; }
    if (plek == 0) for (auto &p : x.populair) if (p.id == t.id && t.id > 0) t = p;
  }
  return true;
}
}  // namespace nf

inline lv_obj_t *rood_label_(lv_obj_t *p, const char *t, int x, int y, int w, const lv_font_t *f) {
  lv_obj_t *l = label(p, t, x, y, w, f, NETFLIX_ROOD);
  lv_obj_set_style_text_color(l, lv_color_hex(NETFLIX_ROOD), 0);   // merkkleur: niet door het thema laten omzetten
  lv_obj_add_flag(l, LV_OBJ_FLAG_USER_4);
  return l;
}
// Poster (64 x 96 of kleiner): titel als tekst tot het beeld binnen is (of zonder TMDB-sleutel blijft)
inline lv_obj_t *nf_poster_(lv_obj_t *p, const nf::Titel &t, int x, int y, int w, int h, int actie, const Fonts &f,
                            std::vector<nf::Wacht> &wacht) {
  lv_obj_t *b = actie >= 0 ? knop(p, x, y, w, h, actie) : lv_obj_create(p);
  if (actie < 0) { lv_obj_remove_style_all(b); lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, h);
                   lv_obj_set_style_bg_color(b, k(SURF), 0); lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
                   lv_obj_remove_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE)); }
  lv_obj_set_style_radius(b, 8, 0);
  lv_obj_set_style_clip_corner(b, true, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);   // vegen schuift de rij, niet de knop
  lv_obj_t *l = label(b, t.naam.c_str(), 0, 0, w - 6, f.f12, TEXT2);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_max_height(l, h - 8, 0);
  lv_obj_center(l);
  lv_obj_t *im = lv_image_create(b);
  lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_center(im);
  if (w != 64) lv_image_set_scale(im, 256 * w / 64);
  auto it = nf::d().posters.find(t.id);
  if (it != nf::d().posters.end()) lv_image_set_src(im, &it->second.dsc);
  else { lv_obj_add_flag(im, LV_OBJ_FLAG_HIDDEN); nf::poster_vragen(t.id); }
  wacht.push_back({im, t.id, 'p'});
  return b;
}
inline lv_obj_t *nf_rij_(lv_obj_t *body, int y, int h) {
  lv_obj_t *r = lv_obj_create(body);
  lv_obj_remove_style_all(r);
  lv_obj_set_pos(r, 0, y);
  lv_obj_set_size(r, 216, h);
  lv_obj_set_scroll_dir(r, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(r, LV_SCROLLBAR_MODE_OFF);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLL_ELASTIC);
  return r;
}
inline void nf_kop_(lv_obj_t *body, int y, const char *l, const char *r, const Fonts &f) {
  label(body, l, 0, y, 150, f.f14 != nullptr ? f.f14 : f.f12, TEXT);
  if (r != nullptr && *r) {
    lv_obj_t *m = label(body, r, 96, y + 2, 120, f.f12, MUTED);
    lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_RIGHT, 0);
  }
}
inline void netflix_bouwen(lv_obj_t *laag, const Fonts &f) {
  Stand &s = st();
  nf::Data &x = nf::d();
  x.wacht_dash.clear();
  lv_obj_t *body = s.netflix_body = body_maken(laag, true);
  rood_label_(body, "NETFLIX", 0, 0, 110, f.f20 != nullptr ? f.f20 : f.f16);
  int y = 36;
  // Top 10: groot rangnummer half achter de poster, zoals de Top 10-rij in de app
  nf_kop_(body, y, "Top 10 in Nederland", x.week.c_str(), f);
  y += 20;
  if (x.top10.empty()) {
    label(body, x.geladen ? "Nog geen lijst - kijk in de log van HA" : "Wordt geladen\xE2\x80\xA6", 0, y + 4, 216, f.f12, MUTED);
    y += 30;
  } else {
    lv_obj_t *r = nf_rij_(body, y, 100);
    const lv_font_t *groot = f.groot != nullptr ? f.groot : (f.f20 != nullptr ? f.f20 : f.f16);
    const int gh = lv_font_get_line_height(groot);
    for (int i = 0; i < (int) x.top10.size() && i < 10; i++) {
      const int px = i * 92;
      char nr[4];
      snprintf(nr, sizeof(nr), "%d", i + 1);
      lv_obj_t *n = rood_label_(r, nr, i == 9 ? px - 24 : px - 4, 98 - gh, i == 9 ? 58 : 38, groot);
      lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_RIGHT, 0);
      nf_poster_(r, x.top10[i], px + 28, 2, 64, 96, NF_TITEL + i, f, x.wacht_dash);
    }
    y += 110;
  }
  if (!x.populair.empty()) {
    nf_kop_(body, y, "Populair op Netflix", "vandaag", f);
    lv_obj_t *r = nf_rij_(body, y + 20, 100);
    for (int i = 0; i < (int) x.populair.size() && i < 10; i++)
      nf_poster_(r, x.populair[i], i * 72, 2, 64, 96, NF_TITEL + 100 + i, f, x.wacht_dash);
    y += 130;
  }
  nf_kop_(body, y, "Laatst gestart", nullptr, f);
  if (x.laatst.empty()) {
    lv_obj_t *h = label(body, "Tik op een titel en kies Afspelen, dan staat hij hier.", 0, y + 22, 216, f.f12, MUTED);
    lv_label_set_long_mode(h, LV_LABEL_LONG_MODE_WRAP);
  } else {
    lv_obj_t *r = nf_rij_(body, y + 20, 100);
    for (int i = 0; i < (int) x.laatst.size(); i++)
      nf_poster_(r, x.laatst[i], i * 72, 2, 64, 96, NF_TITEL + 200 + i, f, x.wacht_dash);
  }
}
// Detailscherm van de gekozen titel (nf::d().detail)
inline void nf_detail_bouwen(lv_obj_t *laag, const Fonts &f) {
  Stand &s = st();
  nf::Data &x = nf::d();
  x.wacht_detail.clear();
  const nf::Titel &t = x.detail;
  lv_obj_t *body = s.nfd_body = body_maken(laag, true);
  lv_obj_t *bd = lv_obj_create(body);
  lv_obj_remove_style_all(bd);
  lv_obj_set_pos(bd, 0, 0);
  lv_obj_set_size(bd, 216, 110);
  lv_obj_set_style_radius(bd, 12, 0);
  lv_obj_set_style_clip_corner(bd, true, 0);
  lv_obj_set_style_bg_color(bd, k(SURF), 0);
  lv_obj_set_style_bg_opa(bd, LV_OPA_COVER, 0);
  lv_obj_remove_flag(bd, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_t *im = lv_image_create(bd);
  lv_obj_set_pos(im, 0, 0);
  if (x.achter_id == t.id && x.achter.px != nullptr) lv_image_set_src(im, &x.achter.dsc);
  else { lv_obj_add_flag(im, LV_OBJ_FLAG_HIDDEN); nf::achter_vragen(t.id); }
  x.wacht_detail.push_back({im, t.id, 'b'});
  nf_poster_(body, t, 8, 40, 48, 72, -1, f, x.wacht_detail);
  if (x.detail_plek > 0) {
    char p[32];
    snprintf(p, sizeof(p), "#%d IN NEDERLAND", x.detail_plek);
    rood_label_(body, p, 64, 58, 150, f.f12);
  }
  lv_obj_t *n = label(body, t.naam.c_str(), 64, 74, 150, f.f20 != nullptr ? f.f20 : f.f16, TEXT);
  lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
  std::string meta = t.jaar;
  auto plus = [&](const std::string &v) { if (!v.empty()) meta += (meta.empty() ? "" : " \xC2\xB7 ") + v; };
  plus(t.genre);
  if (t.seizoenen > 0) plus(std::to_string(t.seizoenen) + (t.seizoenen == 1 ? " seizoen" : " seizoenen"));
  plus(t.leeftijd);
  lv_obj_t *m = label(body, meta.c_str(), 0, 118, 216, f.f12, MUTED);
  lv_label_set_long_mode(m, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_height(m, lv_font_get_line_height(f.f12));   // een regel; te lang = "..." (anders loopt hij over de tekst)
  int y = 138;
  if (!t.tekst.empty()) {
    // drie regels, dan "..." - de knop blijft zo zonder scrollen in beeld
    lv_obj_t *o = label(body, t.tekst.c_str(), 0, y, 216, f.f12, 0xC9CCD3);
    lv_label_set_long_mode(o, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(o, 3 * lv_font_get_line_height(f.f12));
    y += 3 * lv_font_get_line_height(f.f12) + 8;
  }
  lv_obj_t *b = knop(body, 0, y, 216, 38, NF_AFSPELEN);
  lv_obj_set_style_bg_color(b, lv_color_hex(NETFLIX_ROOD), 0);
  lv_obj_set_style_bg_grad_color(b, lv_color_hex(0xB20710), 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_USER_4);
  if (f.ico != nullptr) {
    lv_obj_t *pi = lv_label_create(b);
    lv_label_set_text(pi, "\U000F040A");   // mdi:play
    lv_obj_set_style_text_font(pi, f.ico, 0);
    lv_obj_set_style_text_color(pi, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(pi, LV_ALIGN_CENTER, -78, 0);
  }
  lv_obj_t *bl = label(b, "Afspelen in Netflix", 0, 0, 170, f.f14 != nullptr ? f.f14 : f.f12, 0xFFFFFF);
  lv_obj_set_style_text_color(bl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_add_flag(bl, LV_OBJ_FLAG_USER_4);
  lv_obj_align(bl, LV_ALIGN_CENTER, 18, 0);
}

// ---- NOIR UI-instellingen (simulator build(13)) ----------------------------------------------------------------
inline std::function<void(int, int, int)> &opslaan() { static std::function<void(int, int, int)> f; return f; }   // skin, wall, radius
inline std::function<void(uint32_t *)> &eigen_kleuren() { static std::function<void(uint32_t *)> f; return f; }   // bg,surf,text,accent
inline void alles_herbouwen();

// Kleuren van skin nr in thema() zetten (wall/radius blijven)
inline void thema_kleuren_() {
  Thema &t = thema();
  uint32_t c[4] = {SKIN_KLEUR[0][0], SKIN_KLEUR[0][1], SKIN_KLEUR[0][2], SKIN_KLEUR[0][3]};
  if (t.skin >= 0 && t.skin < 4) for (int i = 0; i < 4; i++) c[i] = SKIN_KLEUR[t.skin][i];
  else if (t.skin == 4 && eigen_kleuren()) eigen_kleuren()(c);
  t.bg = c[0]; t.surf = c[1]; t.text = c[2]; t.accent = c[3];
}
// Bij het opstarten (opgeslagen waarden) - vóór bouwen()
inline void thema_laden(int skin, int wall, int radius) {
  Thema &t = thema();
  t.skin = std::max(0, std::min(SKINS - 1, skin));
  t.wall = std::max(0, std::min(WALLS - 1, wall));
  t.radius = std::max(8, std::min(20, radius));
  thema_kleuren_();
}
inline void thema_bewaren_() { if (opslaan()) opslaan()(thema().skin, thema().wall, thema().radius); }
// Skin kiezen zoals select_preset() in de simulator: kleuren + afronding + wallpaper van de preset
inline void skin_kiezen(int nr) {
  Thema &t = thema();
  t.skin = std::max(0, std::min(SKINS - 1, nr));
  if (t.skin < 4) { t.radius = SKIN_RADIUS[t.skin]; t.wall = SKIN_WALL[t.skin]; }
  thema_kleuren_();
  thema_bewaren_();
  alles_herbouwen();
}
// Aggro600-skin gewisseld: bij "Eigen" volgt NOIR
inline void eigen_gewisseld() { if (thema().skin == 4) alles_herbouwen(); }

// Keuzelijst (2026-11-22): de firmware bouwt LVGL zonder lv_dropdown - daarom een eigen lijst van knoppen over
// het scherm (lv_layer_top), in NOIR-stijl. soort 0 = skin, 1 = wallpaper.
struct Keuze { lv_obj_t *over{nullptr}; int soort{0}; };
inline Keuze &keuze() { static Keuze k; return k; }
inline void keuze_dicht() {
  Keuze &kz = keuze();
  if (kz.over != nullptr) { lv_obj_delete_async(kz.over); kz.over = nullptr; }
}
inline void keuze_gekozen_(lv_event_t *e) {
  const int v = (int) (intptr_t) lv_event_get_user_data(e);
  const int soort = keuze().soort;
  keuze_dicht();
  if (v < 0) return;   // achtergrond: annuleren
  if (soort == 0) { if (v != thema().skin) skin_kiezen(v); return; }
  if (v == thema().wall) return;
  thema().wall = std::max(0, std::min(WALLS - 1, v));
  thema_bewaren_();
  alles_herbouwen();
}
inline void keuze_open_(lv_event_t *e) {
  const int soort = (int) (intptr_t) lv_event_get_user_data(e);
  Keuze &kz = keuze();
  if (kz.over != nullptr) return;
  const Fonts &f = st().fonts;
  const char *const *namen = soort == 0 ? SKIN_NAMEN : WALL_NAMEN;
  const int n = soort == 0 ? SKINS : WALLS, gekozen = soort == 0 ? thema().skin : thema().wall;
  kz.soort = soort;
  kz.over = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(kz.over);
  lv_obj_add_flag(kz.over, LV_OBJ_FLAG_USER_4);
  lv_obj_set_size(kz.over, 240, 320);
  lv_obj_set_style_bg_color(kz.over, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(kz.over, 175, 0);
  lv_obj_add_flag(kz.over, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(kz.over, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(kz.over, keuze_gekozen_, LV_EVENT_CLICKED, (void *) (intptr_t) -1);
  const int h = 16 + 22 + n * 42 + 8;
  lv_obj_t *m = lv_obj_create(kz.over);
  lv_obj_set_size(m, 208, h);
  lv_obj_align(m, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(m, k(SURF), 0);
  lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(m, rnd(16), 0);
  lv_obj_set_style_border_width(m, 1, 0);
  lv_obj_set_style_border_color(m, k(RAND), 0);
  lv_obj_set_style_border_opa(m, LV_OPA_60, 0);
  lv_obj_set_style_pad_all(m, 0, 0);
  lv_obj_set_style_shadow_width(m, 0, 0);
  lv_obj_remove_flag(m, LV_OBJ_FLAG_SCROLLABLE);
  label(m, soort == 0 ? "Skin" : "Wallpaper", 14, 12, 180, f.f12, MUTED);
  for (int i = 0; i < n; i++) {
    lv_obj_t *b = knop(m, 8, 34 + i * 42, 192, 36, -1);
    lv_obj_add_event_cb(b, keuze_gekozen_, LV_EVENT_CLICKED, (void *) (intptr_t) i);
    lv_obj_t *l = label(b, namen[i], 12, 0, 168, f.f12, 0xE4E7EC);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, 0);
    if (i == gekozen) {   // huidige keuze: lichte rand
      lv_obj_set_style_border_color(b, k(0xBFC2C9), 0);
      lv_obj_set_style_border_opa(b, 200, 0);
    }
  }
}
// Knop "Naam  v" die de keuzelijst opent (in plaats van een dropdown)
inline lv_obj_t *keuzelijst_(lv_obj_t *body, int y, int soort, const Fonts &f) {
  const char *naam = soort == 0 ? SKIN_NAMEN[thema().skin] : WALL_NAMEN[thema().wall];
  lv_obj_t *b = knop(body, 0, y, 216, 34, -1);
  lv_obj_add_event_cb(b, keuze_open_, LV_EVENT_CLICKED, (void *) (intptr_t) soort);
  lv_obj_t *l = label(b, naam, 12, 0, 170, f.f12, TEXT);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, 0);
  if (f.ico != nullptr) {
    lv_obj_t *p = lv_label_create(b);
    lv_label_set_text(p, "\U000F0140");   // mdi:chevron-down
    lv_obj_set_style_text_font(p, f.ico, 0);
    lv_obj_set_style_text_color(p, k(MUTED), 0);
    lv_obj_align(p, LV_ALIGN_RIGHT_MID, -10, 0);
  }
  return b;
}
inline void knop_tekst_(lv_obj_t *b, const char *t, const Fonts &f) {
  lv_obj_t *l = label(b, t, 0, 0, lv_obj_get_style_width(b, LV_PART_MAIN) - 10, f.f12, 0xE4E7EC);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(l);
}
inline void inst_bouwen(lv_obj_t *laag, const Fonts &f) {
  // 2026-11-22: alles past zonder scrollen in de 230 px hoge body (de onderste knop verdween onder Terug/Thuis)
  lv_obj_t *body = body_maken(laag, false);
  const Thema &t = thema();
  label(body, "UI-instellingen", 0, 0, 216, f.f20 != nullptr ? f.f20 : f.f16, TEXT);
  label(body, "Skin", 0, 32, 216, f.f12, MUTED);
  keuzelijst_(body, 50, 0, f);
  label(body, "Wallpaper", 0, 92, 216, f.f12, MUTED);
  keuzelijst_(body, 110, 1, f);
  char r[32];
  snprintf(r, sizeof(r), "Hoeken %d px", t.radius);
  knop_tekst_(knop(body, 0, 152, 104, 34, INST_AFRONDING), r, f);
  knop_tekst_(knop(body, 112, 152, 104, 34, KNOPTEST), "Knoppentest", f);
  knop_tekst_(knop(body, 0, 194, 216, 34, INST_REMOTE), "Remote-instellingen", f);
}

// Lagen over page_home bouwen (eenmalig). home = page_home->obj.
inline void bouwen(lv_obj_t *home, const Fonts &f, const Beelden &bl, JsonArrayConst thuis_items = JsonArrayConst()) {
  Stand &s = st();
  if (s.toast_font == nullptr) s.toast_font = f.f14;
  if (s.laag != nullptr || home == nullptr) return;
  s.home = home;
  s.fonts = f;
  s.beelden = bl;
  s.laag = laag_maken(home, bl);
  thuis_bouwen(s.laag, f, bl, thuis_items);
  onderknop(s.laag, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(s.laag, ICO_HOME, "Thuis", 124, THUIS, f);
  s.tv = laag_maken(home, bl);
  tv_bouwen(s.tv, f, bl);
  s.licht = laag_maken(home, bl);
  licht_bouwen(s.licht, f);
  onderknop(s.licht, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(s.licht, ICO_HOME, "Thuis", 124, THUIS, f);
  s.airco = laag_maken(home, bl);
  airco_bouwen(s.airco, f);
  onderknop(s.airco, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(s.airco, ICO_HOME, "Thuis", 124, THUIS, f);
  s.scenes = laag_maken(home, bl);
  scenes_bouwen(s.scenes, f, bl, s.scenes_items ? s.scenes_items() : JsonArrayConst());
  onderknop(s.scenes, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(s.scenes, ICO_HOME, "Thuis", 124, THUIS, f);
  s.netflix = laag_maken(home, bl);
  netflix_bouwen(s.netflix, f);
  onderknop(s.netflix, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(s.netflix, ICO_HOME, "Thuis", 124, THUIS, f);
  s.nfd_laag = laag_maken(home, bl);
  nf_detail_bouwen(s.nfd_laag, f);
  onderknop(s.nfd_laag, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(s.nfd_laag, ICO_HOME, "Thuis", 124, THUIS, f);
  s.test = laag_maken(home, bl);
  test_bouwen(s.test, f);
  onderknop(s.test, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(s.test, ICO_HOME, "Thuis", 124, THUIS, f);
  s.inst = laag_maken(home, bl);
  inst_bouwen(s.inst, f);
  onderknop(s.inst, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(s.inst, ICO_HOME, "Thuis", 124, THUIS, f);
  kop_maken(s.laag, f);
}

// Nieuwe configuratie: tegels van NOIR Thuis opnieuw opbouwen (statusbalk en Terug/Thuis blijven)
inline void body_vervangen_(lv_obj_t *laag, lv_obj_t *&body, const std::function<void()> &maak) {
  if (laag == nullptr) return;
  if (body != nullptr) { lv_obj_delete(body); body = nullptr; }
  maak();
  lv_obj_move_background(body);
  // wallpaper blijft onderop
  for (uint32_t c = 0; c < lv_obj_get_child_count(laag); c++)
    if (lv_obj_check_type(lv_obj_get_child(laag, c), &lv_image_class)) { lv_obj_move_background(lv_obj_get_child(laag, c)); break; }
}
inline void thuis_herbouwen(JsonArrayConst items) {
  Stand &s = st();
  body_vervangen_(s.laag, s.thuis_body, [&]() { thuis_bouwen(s.laag, s.fonts, s.beelden, items); });
}
// Nieuwe scènes (configurator): NOIR Scènes opnieuw
inline void scenes_herbouwen() {
  Stand &s = st();
  body_vervangen_(s.scenes, s.scenes_body, [&]() { scenes_bouwen(s.scenes, s.fonts, s.beelden, s.scenes_items ? s.scenes_items() : JsonArrayConst()); });
}
// Nieuwe lijsten of laatst gestart: NOIR Netflix opnieuw (detail: nf_detail_tonen)
inline void netflix_herbouwen() {
  Stand &s = st();
  body_vervangen_(s.netflix, s.netflix_body, [&]() { netflix_bouwen(s.netflix, s.fonts); });
}
inline void nf::herbouwen_() { netflix_herbouwen(); }
// Detailscherm voor titel code (lijst*100 + i) opbouwen en tonen
inline void zeigen_seite(int seite);
inline void nf_detail_tonen(int code) {
  Stand &s = st();
  nf::Data &x = nf::d();
  if (!nf::titel_van(code, x.detail, x.detail_plek)) return;
  body_vervangen_(s.nfd_laag, s.nfd_body, [&]() { nf_detail_bouwen(s.nfd_laag, s.fonts); });
  zeigen_seite(S_NF_DETAIL);
}
// Nieuwe lampenlijst (configurator): NOIR Verlichting opnieuw
inline void licht_herbouwen() {
  Stand &s = st();
  for (Lamp &l : lampen()) l.lbl = nullptr;
  body_vervangen_(s.licht, s.licht_body, [&]() { licht_bouwen(s.licht, s.fonts); });
}

inline lv_obj_t *laag_van(int seite) {
  Stand &s = st();
  return seite == S_TV ? s.tv : seite == S_LICHT ? s.licht : seite == S_INST ? s.inst : seite == S_AIRCO ? s.airco : seite == S_TEST ? s.test : seite == S_SCENES ? s.scenes : seite == S_NETFLIX ? s.netflix : seite == S_NF_DETAIL ? s.nfd_laag : s.laag;
}

// Een NOIR-pagina tonen (de andere verbergen); de statusbalk verhuist mee
inline void zeigen_seite(int seite) {
  Stand &s = st();
  if (s.laag == nullptr) return;
  if (seite != S_LICHT) dimmer_dicht();
  s.seite = seite;
  s.zichtbaar = true;
  for (int i = 0; i < S_ANZ; i++) {
    lv_obj_t *l = laag_van(i);
    if (l == nullptr) continue;
    if (i == seite) { lv_obj_remove_flag(l, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(l); }
    else lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
  }
  if (s.kop != nullptr) { lv_obj_set_parent(s.kop, laag_van(seite)); lv_obj_move_foreground(s.kop); }
  if (seite == S_LICHT) licht_http::snel(lv_tick_get(), 0);   // meteen de echte stand ophalen
}
inline bool op_tv() { return st().zichtbaar && st().seite == S_TV; }
inline bool op_licht() { return st().zichtbaar && st().seite == S_LICHT; }
inline bool op_airco() { return st().zichtbaar && st().seite == S_AIRCO; }
inline bool op_test() { return st().zichtbaar && st().seite == S_TEST; }
inline bool op_netflix() { return st().zichtbaar && (st().seite == S_NETFLIX || st().seite == S_NF_DETAIL); }

// NOIR aan (laatst getoonde pagina) of helemaal weg (gewone Aggro600-startpagina zichtbaar)
inline void zeigen(bool an) {
  Stand &s = st();
  if (s.laag == nullptr) return;
  if (an) { zeigen_seite(s.seite); return; }
  s.zichtbaar = false;
  dimmer_dicht();
  for (int i = 0; i < S_ANZ; i++) if (laag_van(i) != nullptr) lv_obj_add_flag(laag_van(i), LV_OBJ_FLAG_HIDDEN);
}

// Skin/wallpaper/afronding gewisseld: alle lagen opnieuw (na de lopende gebeurtenis - de keuzelijst die het
// meldde, wordt zelf verwijderd)
inline void alles_herbouwen() {
  static bool gepland = false;
  if (gepland || st().laag == nullptr) return;
  gepland = true;
  lv_async_call([](void *) {
    gepland = false;
    Stand &s = st();
    if (s.laag == nullptr || s.home == nullptr) return;
    thema_kleuren_();   // "Eigen": kleuren van de Aggro600-skin zoals die nu is
    dimmer_dicht();
    keuze_dicht();
    const bool zicht = s.zichtbaar;
    const int seite = s.seite;
    lv_obj_t *weg[] = {s.laag, s.tv, s.licht, s.inst, s.airco, s.test, s.scenes, s.netflix, s.nfd_laag};
    for (lv_obj_t *o : weg) if (o != nullptr) lv_obj_delete(o);   // kop hangt aan een ervan
    s.laag = s.tv = s.licht = s.inst = s.airco = s.test = s.scenes = s.scenes_body = s.netflix = s.netflix_body = s.nfd_laag = s.nfd_body = nullptr;
    for (auto &t : knoptest::st_().tegel) t = nullptr;
    knoptest::st_().info = nullptr;
    airco::ui() = airco::Ui();
    s.kop = s.klok = s.batt_txt = s.batt_fill = s.ha_dot = s.tv_status = s.thuis_body = s.licht_body = nullptr;
    for (Lamp &l : lampen()) l.lbl = nullptr;
    bouwen(s.home, s.fonts, s.beelden, s.items ? s.items() : JsonArrayConst());
    tv_status(s.tv_state, "");
    s.seite = seite;
    if (zicht) zeigen_seite(seite);
  }, nullptr);
}
// Bij elk laden van de startpagina: de laatst getoonde NOIR-pagina (als NOIR aan staat)
inline void bij_home() { if (st().aan) zeigen(true); }

// TV-status uit HA (state van de media_player, attribuut source) op het TV-scherm
inline void tv_status(const std::string &state, const std::string &bron) {
  Stand &s = st();
  s.tv_state = state;
  if (s.tv_status == nullptr) return;
  std::string t;
  if (state == "off") t = "\xE2\x97\x8F Uit";
  else if (state.empty() || state == "unknown" || state == "unavailable") t = "\xE2\x97\x8F Niet bereikbaar";
  else { t = "\xE2\x97\x8F Aan"; if (!bron.empty()) t += " \xC2\xB7 " + bron; }
  if (t != lv_label_get_text(s.tv_status)) lv_label_set_text(s.tv_status, t.c_str());
}
inline bool tv_aan() { return st().tv_state == "on" || st().tv_state == "playing" || st().tv_state == "paused" || st().tv_state == "idle"; }

// Statusbalk bijwerken (elke seconde vanuit de firmware). pct < 0 = onbekend.
inline void status(int uu, int mm, int pct, bool laden, bool ha) {
  Stand &s = st();
  if (s.kop == nullptr || !s.zichtbaar) return;
  char t[16];
  if (uu >= 0) snprintf(t, sizeof(t), "%02d:%02d", uu, mm); else snprintf(t, sizeof(t), "--:--");
  if (strcmp(lv_label_get_text(s.klok), t) != 0) lv_label_set_text(s.klok, t);
  if (pct >= 0) snprintf(t, sizeof(t), "%d%%", pct); else t[0] = 0;
  if (strcmp(lv_label_get_text(s.batt_txt), t) != 0) lv_label_set_text(s.batt_txt, t);
  const int w = pct < 0 ? 0 : 1 + (pct * 18 + 50) / 100;   // 19 px = vol
  lv_obj_set_width(s.batt_fill, w);
  lv_obj_set_style_bg_color(s.batt_fill, k(laden ? 0xF2F3F5 : BATT_F), 0);
  lv_obj_set_style_bg_opa(s.ha_dot, ha ? 220 : 45, 0);
}

// Korte melding bovenaan (verdwijnt vanzelf na ms). Eigen label op lv_layer_top - de pil van de firmware
// (tasten::modus_anzeige) hoort bij de volgmodus en verdwijnt niet vanzelf (2026-11-21: bleef staan na Stumm).
inline void toast(const char *tekst, uint32_t ms = 1500) {
  static lv_obj_t *t = nullptr;
  static lv_timer_t *weg = nullptr;
  if (t == nullptr) {
    t = lv_label_create(lv_layer_top());
    lv_obj_add_flag(t, LV_OBJ_FLAG_USER_4);
    lv_obj_set_style_bg_opa(t, 240, 0);
    if (st().toast_font != nullptr) lv_obj_set_style_text_font(t, st().toast_font, 0);
    lv_obj_set_style_radius(t, 14, 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_border_color(t, k(RAND_P), 0);
    lv_obj_set_style_pad_hor(t, 16, 0);
    lv_obj_set_style_pad_ver(t, 8, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_set_style_bg_color(t, k(SURF), 0);   // skin kan intussen gewisseld zijn
  lv_obj_set_style_text_color(t, k(TEXT), 0);
  lv_obj_set_style_border_color(t, k(RAND_P), 0);
  lv_label_set_text(t, tekst);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(t);
  if (weg != nullptr) lv_timer_delete(weg);
  weg = lv_timer_create([](lv_timer_t *) { lv_obj_add_flag(t, LV_OBJ_FLAG_HIDDEN); weg = nullptr; }, ms, nullptr);
  lv_timer_set_repeat_count(weg, 1);
}

// ---- HA-aanroep en ingedrukt-houden (gedeeld door KEF en TV) ---------------------------------------------------
// HA-aanroep van de firmware (tk_cfg->ha_aufruf: via wifi, Bluetooth-link of dock, met wachtrij na het wekken)
inline std::function<void(const std::string &, Data)> &ha() {
  static std::function<void(const std::string &, Data)> z;
  return z;
}
struct Halten { int taste{0}; int n{0}; uint32_t sinds{0}; std::function<void()> f; };
inline Halten &halten() { static Halten h; return h; }
static const uint32_t HERHAAL_MS = 300;
static const int HERHAAL_MAX = 30;
inline void halten_start(int taste, std::function<void()> f) { Halten &h = halten(); h.taste = taste; h.n = 0; h.sinds = 0; h.f = std::move(f); }
inline void halten_los(int taste) { if (halten().taste == taste) halten().taste = 0; }
// Elke ~100 ms vanuit de firmware (interval): ingedrukt houden herhaalt
inline void herhaal_tik(uint32_t nu) {
  Halten &h = halten();
  if (h.taste == 0) { h.sinds = 0; return; }
  if (h.sinds == 0) { h.sinds = nu; return; }
  if (nu - h.sinds < HERHAAL_MS) return;
  h.sinds = nu;
  if (++h.n > HERHAAL_MAX) { h.taste = 0; return; }   // loslaten kwam nooit aan
  if (h.f) h.f();
}

// ---- KEF LSX II: hardwarevolume op elk scherm ------------------------------------------------------------------
namespace kef {

static const int T_LAUTER = 33, T_LEISER = 43, T_STUMM = 44;
using noir::Data;

inline std::string &entiteit() { static std::string e; return e; }
inline std::function<void(const std::string &, Data)> &zender() { return noir::ha(); }
inline std::function<void(const char *)> &melding() { static std::function<void(const char *)> m; return m; }

inline bool is_volume(int t) { return t == T_LAUTER || t == T_LEISER || t == T_STUMM; }
inline bool actief() { return !entiteit().empty() && zender() != nullptr; }

inline void stuur_(int taste) {
  const char *svc = taste == T_LAUTER ? "media_player.volume_up" : taste == T_LEISER ? "media_player.volume_down" : nullptr;
  if (svc != nullptr) zender()(svc, {{"entity_id", entiteit()}});
}

// Uit on_key, vóór alles: true = afgehandeld (niets anders krijgt de toets)
inline bool taste(int t, bool ingedrukt) {
  if (!actief() || !is_volume(t)) return false;
  if (!ingedrukt) { halten_los(t); return true; }
  if (t == T_STUMM) {
    // Dempen omschakelen. De remote kent de werkelijke mute-stand van de KEF niet: hij wisselt zelf tussen
    // is_volume_muted true/false (HA zet de tekst "true"/"false" zelf om).
    static bool gedempt = false;
    gedempt = !gedempt;
    zender()("media_player.volume_mute", {{"entity_id", entiteit()}, {"is_volume_muted", gedempt ? "true" : "false"}});
    if (melding()) melding()(gedempt ? "KEF gedempt" : "KEF geluid aan");
    return true;
  }
  stuur_(t);
  halten_start(t, [t]() { stuur_(t); });
  return true;
}
// oude naam (interval in de YAML)
inline void herhaal_tik(uint32_t nu) { noir::herhaal_tik(nu); }

}  // namespace kef

// ---- TV: fysieke toetsen op het NOIR TV-scherm -> webostv.button ------------------------------------------------
namespace tv {

inline std::string &entiteit() { static std::string e; return e; }

// Toetscodes (defaults.py): 14 Hoch, 35 Runter, 15 Links, 32 Rechts, 34 OK, 13 Zurueck, 3 "Menu"/Home am Geraet
inline const char *knop_van(int t) {
  switch (t) {
    case 14: return "UP";
    case 35: return "DOWN";
    case 15: return "LEFT";
    case 32: return "RIGHT";
    case 34: return "ENTER";
    case 13: return "BACK";
    case 3: return "HOME";
    default: return nullptr;
  }
}
inline void stuur(const char *knop) {
  if (noir::ha()) noir::ha()("webostv.button", {{"entity_id", entiteit()}, {"button", knop}});
}
// Uit on_key (na KEF): alleen als het NOIR TV-scherm zichtbaar is en de startpagina actief is
inline bool airco_taste(int t, bool ingedrukt) {
  if (t != 14 && t != 35 && t != 34) return false;
  if (!ingedrukt) { halten_los(t); return true; }
  if (t == 34) { airco::aanuit(); return true; }
  const int d = t == 14 ? 1 : -1;
  airco::temp_stap(d);
  halten_start(t, [d]() { airco::temp_stap(d); });
  return true;
}
inline bool taste(int t, bool ingedrukt, bool op_home) {
  if (op_home && op_test()) return knoptest::taste(t, ingedrukt);   // NOIR Knoppentest: alles blijft op het scherm
  if (op_home && op_airco()) return airco_taste(t, ingedrukt);   // NOIR Airco: pijlen = temperatuur, OK = aan/uit
  if (entiteit().empty() || !noir::ha() || !op_home || !(op_tv() || op_netflix())) return false;   // ook op NOIR Netflix
  // NOIR Netflix-dashboard: de mediatoetsen van de remote bedienen Netflix (2 play/pauze, 5 stop, 4/11 spoelen)
  if (op_netflix() && (t == 2 || t == 5 || t == 4 || t == 11)) {
    if (!ingedrukt) return true;
    if (t == 2) noir::ha()("media_player.media_play_pause", {{"entity_id", entiteit()}});
    else stuur(t == 5 ? "STOP" : t == 4 ? "REWIND" : "FASTFORWARD");
    return true;
  }
  const char *knop = knop_van(t);
  if (knop == nullptr) return false;
  if (!ingedrukt) { halten_los(t); return true; }
  stuur(knop);
  if (t == 14 || t == 35 || t == 15 || t == 32) halten_start(t, [knop]() { stuur(knop); });   // pijlen herhalen
  return true;
}

}  // namespace tv
}  // namespace noir
