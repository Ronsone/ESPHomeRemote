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
enum Actie : int { TERUG = 1, THUIS = 100, TV = 101, AUDIO = 102, LICHT = 103, SCENES = 104, INSTELLINGEN = 113 };

struct Fonts { const lv_font_t *f12{nullptr}, *f14{nullptr}, *f16{nullptr}, *ico{nullptr}; };
struct Beelden { const void *wall{nullptr}, *tv{nullptr}, *audio{nullptr}, *licht{nullptr}, *scenes{nullptr}; };

struct Stand {
  lv_obj_t *laag{nullptr}, *klok{nullptr}, *batt_txt{nullptr}, *batt_fill{nullptr}, *ha_dot{nullptr};
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

inline void tegel(lv_obj_t *body, const char *t, int x, int y, int actie, const void *icoon, const Fonts &f) {
  lv_obj_t *b = knop(body, x, y, 104, 66, actie);
  if (icoon != nullptr) {
    lv_obj_t *im = lv_image_create(b);
    lv_image_set_src(im, icoon);
    lv_obj_set_pos(im, 13, 10);
    lv_obj_set_style_image_opa(im, 210, 0);
    lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
  }
  label(b, t, 12, 66 - 25, 80, f.f14, TEXT2);
}

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

// Laag over page_home bouwen (eenmalig). home = page_home->obj.
inline void bouwen(lv_obj_t *home, const Fonts &f, const Beelden &bl) {
  Stand &s = st();
  if (s.toast_font == nullptr) s.toast_font = f.f14;
  if (s.laag != nullptr || home == nullptr) return;
  lv_obj_t *laag = s.laag = lv_obj_create(home);
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

  // Statusbalk: klok links, HA-punt, accu-% en accusymbool rechts
  s.klok = label(laag, "--:--", 12, 7, 60, f.f12, STAT);
  s.batt_txt = label(laag, "", 151, 7, 31, f.f12, STAT);
  lv_obj_set_style_text_align(s.batt_txt, LV_TEXT_ALIGN_RIGHT, 0);
  s.ha_dot = detail(laag, 143, 11, 3, 3, DOT, 45, LV_RADIUS_CIRCLE);
  lv_obj_t *bat = detail(laag, 192, 9, 25, 11, 0x08090B, 100, 3);
  lv_obj_set_style_border_width(bat, 1, 0);
  lv_obj_set_style_border_color(bat, k(BATT), 0);
  s.batt_fill = detail(bat, 3, 3, 15, 5, BATT_F, 220, 1);
  detail(laag, 218, 12, 2, 5, BATT, 180, 1);

  lv_obj_t *body = lv_obj_create(laag);
  lv_obj_remove_style_all(body);
  lv_obj_set_pos(body, 12, 30);
  lv_obj_set_size(body, 216, 230);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
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
  tegel(body, "TV", 0, 58, TV, bl.tv, f);
  tegel(body, "Audio", 112, 58, AUDIO, bl.audio, f);
  tegel(body, "Verlichting", 0, 132, LICHT, bl.licht, f);
  tegel(body, "Sc\xC3\xA8nes", 112, 132, SCENES, bl.scenes, f);

  onderknop(laag, ICO_LINKS, "Terug", 12, TERUG, f);
  onderknop(laag, ICO_HOME, "Thuis", 124, THUIS, f);
  lv_obj_add_flag(laag, LV_OBJ_FLAG_HIDDEN);
}

inline void zeigen(bool an) {
  Stand &s = st();
  if (s.laag == nullptr) return;
  s.zichtbaar = an;
  if (an) {
    lv_obj_remove_flag(s.laag, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s.laag);
  } else {
    lv_obj_add_flag(s.laag, LV_OBJ_FLAG_HIDDEN);
  }
}
// Bij elk laden van de startpagina: NOIR Thuis tonen (als het aan staat)
inline void bij_home() { if (st().aan) zeigen(true); }

// Statusbalk bijwerken (elke seconde vanuit de firmware). pct < 0 = onbekend.
inline void status(int uu, int mm, int pct, bool laden, bool ha) {
  Stand &s = st();
  if (s.laag == nullptr || !s.zichtbaar) return;
  char t[12];
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

// ---- KEF LSX II: hardwarevolume op elk scherm --------------------------------------------------------------------
namespace kef {

static const int T_LAUTER = 33, T_LEISER = 43, T_STUMM = 44;
static const uint32_t HERHAAL_MS = 300;
static const int HERHAAL_MAX = 30;

inline std::string &entiteit() { static std::string e; return e; }
// HA-aanroep van de firmware (tk_cfg->ha_aufruf: via wifi, Bluetooth-link of dock, met wachtrij na het wekken)
using Data = std::map<std::string, std::string>;
inline std::function<void(const std::string &, Data)> &zender() {
  static std::function<void(const std::string &, Data)> z;
  return z;
}
inline std::function<void(const char *)> &melding() { static std::function<void(const char *)> m; return m; }
inline int &vastgehouden() { static int t = 0; return t; }
inline int &herhalingen() { static int n = 0; return n; }

inline bool is_volume(int t) { return t == T_LAUTER || t == T_LEISER || t == T_STUMM; }
inline bool actief() { return !entiteit().empty() && zender() != nullptr; }

inline void stuur_(int taste) {
  const char *svc = taste == T_LAUTER ? "media_player.volume_up" : taste == T_LEISER ? "media_player.volume_down" : nullptr;
  if (svc != nullptr) zender()(svc, {{"entity_id", entiteit()}});
}

// Uit on_key, vóór alles: true = afgehandeld (niets anders krijgt de toets). Herhalen regelt herhaal_tik().
inline bool taste(int t, bool ingedrukt) {
  if (!actief() || !is_volume(t)) return false;
  if (!ingedrukt) {
    if (vastgehouden() == t) vastgehouden() = 0;
    return true;
  }
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
  vastgehouden() = t;
  herhalingen() = 0;
  return true;
}

// Elke ~100 ms vanuit de firmware (interval): ingedrukt houden herhaalt
inline void herhaal_tik(uint32_t nu) {
  static uint32_t sinds = 0;
  const int t = vastgehouden();
  if (t == 0) { sinds = 0; return; }
  if (sinds == 0) { sinds = nu; return; }
  if (nu - sinds < HERHAAL_MS) return;
  sinds = nu;
  if (++herhalingen() > HERHAAL_MAX) { vastgehouden() = 0; return; }   // loslaten kwam nooit aan
  stuur_(t);
}

}  // namespace kef
}  // namespace noir
