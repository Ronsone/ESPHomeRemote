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
#include <string>

namespace noir {

// Kleuren van de simulator (skin NOIR / Obsidian)
static const uint32_t BG = 0x07080A, SURF = 0x1A1C20, SURF2 = 0x101114, SURF_DIM = 0x101216, TEXT = 0xF2F3F5,
                      TEXT2 = 0xE4E7EC, MUTED = 0x969AA3, STAT = 0xB6BAC3, RAND = 0x383C43, RAND_P = 0xAEB4BF,
                      DRUK = 0x363A43, DRUK2 = 0x252931, LIJN = 0xC9D0DC, BATT = 0xA4AAB5, BATT_F = 0xD3D8E0,
                      DOT = 0xD7DCE5;

// Acties van de tegels (zelfde betekenis als action() in de simulator: 101-104 tegels, 113 instellingen, 1 Terug,
// 100 Thuis)
enum Actie : int { TERUG = 1, THUIS = 100, TV = 101, AUDIO = 102, LICHT = 103, SCENES = 104, AIRCO = 105, INSTELLINGEN = 113,
                   TV_AANUIT = 120, TV_APP = 200 /* 200-205: app-tegels */ };

struct Fonts { const lv_font_t *f12{nullptr}, *f14{nullptr}, *f16{nullptr}, *ico{nullptr}, *f20{nullptr}; };

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

enum Seite : int { S_THUIS = 0, S_TV = 1, S_ANZ = 2 };

struct Stand {
  lv_obj_t *laag{nullptr};                  // NOIR Thuis
  lv_obj_t *tv{nullptr};                    // NOIR TV (2026-11-21)
  lv_obj_t *kop{nullptr};                   // statusbalk, hangt aan de zichtbare laag
  lv_obj_t *klok{nullptr}, *batt_txt{nullptr}, *batt_fill{nullptr}, *ha_dot{nullptr};
  lv_obj_t *tv_status{nullptr};
  int seite{S_THUIS};                       // laatst getoonde NOIR-pagina
  std::string tv_state;                     // "on", "off", "unavailable" ... (uit HA)
  bool aan{false};      // NOIR als startscherm gebruiken
  bool zichtbaar{false};
  std::function<void(int)> actie;   // wordt door de firmware ingevuld (tasten::noir_actie)
  const lv_font_t *toast_font{nullptr};
};
inline Stand &st() { static Stand s; return s; }

inline lv_color_t k(uint32_t c) { return lv_color_hex(c); }

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
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

inline void klik_(lv_event_t *e) {
  const int a = (int) (intptr_t) lv_event_get_user_data(e);
  if (st().actie) st().actie(a);
}

// Knop zoals button() in de simulator (grafietvlak, verloop, fijne bovenrand, aparte ingedrukte toestand)
inline lv_obj_t *knop(lv_obj_t *p, int x, int y, int w, int h, int actie) {
  lv_obj_t *b = lv_button_create(p);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, k(SURF), 0);
  lv_obj_set_style_bg_opa(b, 232, 0);
  lv_obj_set_style_radius(b, h >= 60 ? 16 : h <= 34 ? 11 : 13, 0);
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
  lv_obj_add_event_cb(b, klik_, LV_EVENT_CLICKED, (void *) (intptr_t) actie);
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
  lv_obj_set_style_radius(b, 14, 0);
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
  lv_obj_set_style_bg_color(laag, k(BG), 0);
  lv_obj_set_style_bg_opa(laag, LV_OPA_COVER, 0);
  lv_obj_remove_flag(laag, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(laag, LV_OBJ_FLAG_CLICKABLE);   // tikken op de achtergrond gaat niet door naar de Aggro600-startpagina
  if (bl.wall != nullptr) {   // Obsidian-wallpaper, dekking 210 (wallpaper_mode 0)
    lv_obj_t *bg = lv_image_create(laag);
    lv_image_set_src(bg, bl.wall);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_set_style_image_opa(bg, 210, 0);
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

// NOIR Thuis (simulator build(0))
inline void thuis_bouwen(lv_obj_t *laag, const Fonts &f, const Beelden &bl) {
  lv_obj_t *body = body_maken(laag, false);
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
  // 2026-11-22: Airco erbij - drie rijen van 56 hoog (simulator: twee van 66), Airco over de volle breedte
  tegel(body, "TV", 0, 52, TV, bl.tv, f, 104, 56);
  tegel(body, "Audio", 112, 52, AUDIO, bl.audio, f, 104, 56);
  tegel(body, "Verlichting", 0, 114, LICHT, bl.licht, f, 104, 56);
  tegel(body, "Sc\xC3\xA8nes", 112, 114, SCENES, bl.scenes, f, 104, 56);
  tegel(body, "Airco", 0, 176, AIRCO, nullptr, f, 216, 50, ICO_SNEEUW);
  onderknop(laag, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(laag, ICO_HOME, "Thuis", 124, THUIS, f);
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

// Lagen over page_home bouwen (eenmalig). home = page_home->obj.
inline void bouwen(lv_obj_t *home, const Fonts &f, const Beelden &bl) {
  Stand &s = st();
  if (s.toast_font == nullptr) s.toast_font = f.f14;
  if (s.laag != nullptr || home == nullptr) return;
  s.laag = laag_maken(home, bl);
  thuis_bouwen(s.laag, f, bl);
  s.tv = laag_maken(home, bl);
  tv_bouwen(s.tv, f, bl);
  kop_maken(s.laag, f);
}

inline lv_obj_t *laag_van(int seite) { return seite == S_TV ? st().tv : st().laag; }

// Een NOIR-pagina tonen (de andere verbergen); de statusbalk verhuist mee
inline void zeigen_seite(int seite) {
  Stand &s = st();
  if (s.laag == nullptr) return;
  s.seite = seite;
  s.zichtbaar = true;
  for (int i = 0; i < S_ANZ; i++) {
    lv_obj_t *l = laag_van(i);
    if (l == nullptr) continue;
    if (i == seite) { lv_obj_remove_flag(l, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(l); }
    else lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
  }
  if (s.kop != nullptr) { lv_obj_set_parent(s.kop, laag_van(seite)); lv_obj_move_foreground(s.kop); }
}
inline bool op_tv() { return st().zichtbaar && st().seite == S_TV; }

// NOIR aan (laatst getoonde pagina) of helemaal weg (gewone Aggro600-startpagina zichtbaar)
inline void zeigen(bool an) {
  Stand &s = st();
  if (s.laag == nullptr) return;
  if (an) { zeigen_seite(s.seite); return; }
  s.zichtbaar = false;
  for (int i = 0; i < S_ANZ; i++) if (laag_van(i) != nullptr) lv_obj_add_flag(laag_van(i), LV_OBJ_FLAG_HIDDEN);
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
    lv_obj_set_style_bg_color(t, k(SURF), 0);
    lv_obj_set_style_bg_opa(t, 240, 0);
    lv_obj_set_style_text_color(t, k(TEXT), 0);
    if (st().toast_font != nullptr) lv_obj_set_style_text_font(t, st().toast_font, 0);
    lv_obj_set_style_radius(t, 14, 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_border_color(t, k(RAND_P), 0);
    lv_obj_set_style_pad_hor(t, 16, 0);
    lv_obj_set_style_pad_ver(t, 8, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_label_set_text(t, tekst);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(t);
  if (weg != nullptr) lv_timer_delete(weg);
  weg = lv_timer_create([](lv_timer_t *) { lv_obj_add_flag(t, LV_OBJ_FLAG_HIDDEN); weg = nullptr; }, ms, nullptr);
  lv_timer_set_repeat_count(weg, 1);
}

// ---- HA-aanroep en ingedrukt-houden (gedeeld door KEF en TV) ---------------------------------------------------
using Data = std::map<std::string, std::string>;
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
inline bool taste(int t, bool ingedrukt, bool op_home) {
  if (entiteit().empty() || !noir::ha() || !op_home || !op_tv()) return false;
  const char *knop = knop_van(t);
  if (knop == nullptr) return false;
  if (!ingedrukt) { halten_los(t); return true; }
  stuur(knop);
  if (t == 14 || t == 35 || t == 15 || t == 32) halten_start(t, [knop]() { stuur(knop); });   // pijlen herhalen
  return true;
}

}  // namespace tv
}  // namespace noir
