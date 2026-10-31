/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * FMini-1: the whole user interface of the FMINI-only firmware (FELUCCA_FMINI_ONLY=1). Felucca's menu, layers, sequencer
 * pages and the rest of its screens are not reachable: main.c calls fa_input / fa_leds / fa_draw instead of
 * ui_input / ui_leds / ui_draw, and fa_splash instead of draw_splash. The sound engine, the FX, MIDI, USB and the
 * storage are Felucca's.
 *
 * The screen is the approved panel (a 240 x 240 version of the 320 x 240 prototype): wood sides, the tabs
 * CTRL OSC MIX FILT F.ENV A.ENV FX, four fluted knobs with a silver cap and a 0..10 scale, the sliding-box switches
 * (CTRL red, MIX blue, FX orange), the waveform boxes, the filter and contour graphs, the patch number and name
 * bottom left and "n/7" small bottom right.
 *
 * Controls: KNOB 1..4 the page's four knobs. SELECT moves a highlight over every control of every page (the knobs, the
 * switches, the waveforms; past the last control of a page it goes on to the next); ALGORITHM changes the highlighted
 * control (a switch: right = on, left = off). PRESETS: the factory patch F1..F8. Buttons: FX, ENV (F.ENV, again
 * A.ENV), LFO (CTRL), EDIT (OSC), SCL (MIX), GLO (FILT) open a page; OCT- / OCT+ the keys' octave. The keys, MIDI
 * and the MASTER pot play and sound as in Felucca. Track 1 is the instrument; the other tracks are muted.
 * Included by felucca.c before main.c. */
#if FELUCCA_FMINI_ONLY
#define FA_RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define FA_OR FA_RGB(0xE0, 0x75, 0x2B)                   /* orange: values, curves, the active tab */
#define FA_CR FA_RGB(0xEC, 0xE4, 0xD0)                   /* cream: labels */
#define FA_MU FA_RGB(0x9A, 0x8F, 0x7E)                   /* muted */
#define FA_RED FA_RGB(0xD6, 0x38, 0x2B)
#define FA_BLUE FA_RGB(0x2F, 0x7F, 0xE0)
#define FA_BOX FA_RGB(0x0F, 0x0E, 0x0D)
#define FA_LINE FA_RGB(0x3A, 0x35, 0x30)
#define FA_RULE FA_RGB(0x4A, 0x44, 0x3D)
#define FA_NPAGE 7u

static uint8_t fa_page;                                  /* 0..6 */
static uint8_t fa_env_last;                              /* ENV: the page it opened last (F.ENV or A.ENV) */
static uint32_t fa_sig_prev = 0xFFFFFFFFu;
static uint16_t fa_knob_bmp[38 * 38];                    /* the baked knob: 0 = transparent */
static uint8_t fa_knob_ok;

static const char *const FA_TAB[FA_NPAGE] = {"CTRL", "OSC", "MIX", "FILT", "F.ENV", "A.ENV", "FX"};

/* ---------------------------------------------------------- colours --- */
static uint16_t fa_mix(uint16_t a, uint16_t b, uint32_t t)   /* a + (b - a) * t / 256 per channel */
{
    uint32_t r = ((a >> 11) * (256u - t) + (b >> 11) * t) >> 8, g = (((a >> 5) & 63u) * (256u - t) + ((b >> 5) & 63u) * t) >> 8,
             bl = ((a & 31u) * (256u - t) + (b & 31u) * t) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static uint16_t fa_bg(int32_t y)                         /* the panel's vertical gradient #24211e -> #141211 */
{
    return fa_mix(FA_RGB(0x24, 0x21, 0x1E), FA_RGB(0x14, 0x12, 0x11), (uint32_t)clamp(y, 0, 239) * 256u / 240u);
}
static const uint16_t FA_SILVER[5] = {FA_RGB(0xF4, 0xF4, 0xF2), FA_RGB(0x8D, 0x8D, 0x8B), FA_RGB(0xE2, 0xE2, 0xDF),
                                      FA_RGB(0x6E, 0x6E, 0x6C), FA_RGB(0xCF, 0xCF, 0xCC)};
static uint16_t fa_silver(uint32_t t)                    /* 0..255 along the gradient (stops .3 .55 .8) */
{
    static const uint8_t ST[5] = {0, 77, 140, 204, 255};
    uint32_t i = 0;
    while (i < 3u && t > ST[i + 1])
        i++;
    return fa_mix(FA_SILVER[i], FA_SILVER[i + 1], (t - ST[i]) * 256u / (uint32_t)(ST[i + 1] - ST[i]));
}

/* the angle of (dx, dy): 0..1023 for a turn, 0 up, clockwise (about 4 degrees at worst) */
static uint32_t fa_ang(int32_t dx, int32_t dy)
{
    int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, t, a;
    if (!ax && !ay)
        return 0;
    if (ay >= ax) {                                       /* up / down sectors: 0 .. 128 from the vertical */
        t = ax * 128 / ay;
        a = t;
        if (dy > 0)
            a = 512 - a;                                  /* down: 512 - */
    } else {
        t = ay * 128 / ax;
        a = 256 - t;                                      /* right: 256 - ; */
        if (dy > 0)
            a = 256 + t;
    }
    if (dx < 0)
        a = 1024 - a;
    return (uint32_t)a & 1023u;
}

/* the knob, baked once: shadow, the fluted skirt, the plate, the black ring, the brushed silver cap */
static void fa_bake_knob(void)
{
    int32_t x, y;
    for (y = 0; y < 38; y++)
        for (x = 0; x < 38; x++) {
            int32_t dx = 2 * x + 1 - 38, dy = 2 * y + 1 - 38, r2 = dx * dx + dy * dy, sh2 = dx * dx + (dy - 2) * (dy - 2);
            uint32_t ang = fa_ang(dx, dy), ph = (ang * 12u) & 1023u;
            int32_t wob = (int32_t)(ph < 512u ? ph : 1023u - ph) * 3 / 512 - 1;   /* the 12 flutes: -1 .. +2 half pixels */
            int32_t rs = 33 + wob;                                                 /* the skirt, in half pixels */
            uint16_t c = 0;
            if (sh2 <= (rs + 2) * (rs + 2))
                c = FA_RGB(0x0B, 0x0A, 0x09);                                      /* the shadow */
            if (r2 <= rs * rs) {
                int32_t hx = dx + 7, hy = dy + 13, d2 = hx * hx + hy * hy;          /* the highlight, up and left */
                uint32_t t = (uint32_t)clamp(d2 * 256 / (50 * 50), 0, 255);
                c = fa_mix(FA_RGB(0x3D, 0x39, 0x35), FA_RGB(0x0C, 0x0B, 0x0A), t);
                if (r2 > (rs - 2) * (rs - 2))
                    c = FA_RGB(0x04, 0x04, 0x04);                                   /* its black edge */
            }
            if (r2 <= 28 * 28)
                c = r2 > 26 * 26 ? FA_RGB(0x2B, 0x28, 0x25) : FA_RGB(0x0D, 0x0C, 0x0B);   /* the plate and its rim */
            if (r2 <= 20 * 20)
                c = FA_RGB(0x08, 0x08, 0x08);                                       /* the black ring */
            if (r2 <= 19 * 19) {                                                    /* the silver cap */
                uint32_t t = (uint32_t)clamp((dx + dy + 38) * 255 / 76, 0, 255), br = (ang * 28u * 4u) & 1023u;
                c = fa_silver(t);
                if (br < 40u && r2 > 3 * 3)
                    c = fa_mix(c, 0, 40);                                           /* the brushing */
                if (r2 > 17 * 17)
                    c = fa_mix(c, 0xFFFF, 60);                                      /* the lit rim */
            }
            fa_knob_bmp[y * 38 + x] = c ? (uint16_t)(c | 0x0841u) : 0u;
        }
    fa_knob_ok = 1;
}
static void fa_knob_draw(int32_t cx, int32_t cy)
{
    int32_t x, y;
    if (!fa_knob_ok)
        fa_bake_knob();
    for (y = 0; y < 38; y++)
        for (x = 0; x < 38; x++) {
            uint16_t c = fa_knob_bmp[y * 38 + x];
            if (c)
                cv_pset(cx - 19 + x, cy - 19 + y, c);
        }
}

/* ------------------------------------------------------------- text --- */
static int32_t fa_cap_y(const aafont_t *f) { return f == &AF_L ? AF_L_CAP_Y : f == &AF_N ? AF_N_CAP_Y : f == &AF_X ? AF_X_CAP_Y : f == &AF_S ? AF_S_CAP_Y : AF_M_CAP_Y; }
static int32_t fa_cap_h(const aafont_t *f) { return f == &AF_L ? AF_L_CAP_H : f == &AF_N ? AF_N_CAP_H : f == &AF_X ? AF_X_CAP_H : f == &AF_S ? AF_S_CAP_H : AF_M_CAP_H; }
/* text with its capitals starting at row ytop, centred on cx */
static void fa_ctext(int32_t cx, int32_t ytop, const aafont_t *f, const char *s, uint16_t fg)
{
    cv_text_on(cx - text_w(f, s) / 2, ytop - fa_cap_y(f), f, s, fg, fa_bg(ytop));
}
/* text on a pop-up's own background (fa_ctext's antialiasing is against the panel's gradient) */
static void fa_pc_text(int32_t cx, int32_t ytop, const aafont_t *f, const char *s, uint16_t fg, uint16_t bg)
{
    cv_text_on(cx - text_w(f, s) / 2, ytop - fa_cap_y(f), f, s, fg, bg);
}
static void fa_pl_text(int32_t x, int32_t ytop, const aafont_t *f, const char *s, uint16_t fg, uint16_t bg)
{
    cv_text_on(x, ytop - fa_cap_y(f), f, s, fg, bg);
}
static void fa_ltext(int32_t x, int32_t ytop, const aafont_t *f, const char *s, uint16_t fg)
{
    cv_text_on(x, ytop - fa_cap_y(f), f, s, fg, fa_bg(ytop));
}
static void fa_rtext(int32_t xr, int32_t ytop, const aafont_t *f, const char *s, uint16_t fg)
{
    cv_text_r(xr, ytop - fa_cap_y(f), f, s, fg, fa_bg(ytop));
}

/* a polyline of points (x, y in 1/100 of w, h; y up) in the box x, y, w, h */
static void fa_poly(const int16_t *pt, uint32_t n, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    uint32_t i;
    for (i = 0; i + 1u < n; i++)
        cv_line_t(x + pt[2 * i] * w / 100, y + h / 2 - pt[2 * i + 1] * h / 200, x + pt[2 * i + 2] * w / 100,
                  y + h / 2 - pt[2 * i + 3] * h / 200, c, 2);
}
static const int16_t FA_WAVE[6][12] = {                 /* TRI SHARK SAW SQR PULSE W PULSE N: x 0..100, y -100..100 */
    {0, 0, 25, 100, 75, -100, 100, 0, -1, -1, -1, -1},
    {0, -100, 40, 100, 60, -20, 100, -100, -1, -1, -1, -1},
    {0, 0, 50, 100, 50, -100, 100, 0, -1, -1, -1, -1},
    {0, -100, 0, 100, 50, 100, 50, -100, 100, -100, -1, -1},
    {0, -100, 0, 100, 33, 100, 33, -100, 100, -100, -1, -1},
    {0, -100, 0, 100, 12, 100, 12, -100, 100, -100, -1, -1}};
static const uint8_t FA_WAVE_N[6] = {4, 4, 4, 5, 5, 5};

/* ------------------------------------------------------------ scene --- */
static void fa_num(char *s, int32_t ratio)               /* 0..1000 -> "0.0" .. "10.0" */
{
    ratio = clamp(ratio, 0, 1000);
    fmt_int(s, ratio / 100);
    s += str_len(s);
    *s++ = '.';
    *s++ = (char)('0' + ratio % 100 / 10);
    *s = 0;
}

/* a knob's number: its ratio as 0.0 .. 10.0, except the FILTER's CUTOFF and EMPHASIS, which keep their dial text (param_format:
 * the cutoff -5.0 .. +5.0, 0 in the middle; the emphasis 0.0 .. 10.0) */
static void fa_val(char *s, int32_t ratio, const fmgf_t *f)
{
    if (f->kind == FK_PAR && (f->a == P_E0 || f->a == P_E1))
        return;
    if (f->kind == FK_BYTE && (f->a == FG_F2 || f->a == FG_F3))    /* DET 2 / 3: the signed semitones (fmg_card_at) */
        return;
    fa_num(s, ratio);
}

static void fa_knob_col(uint32_t pg, uint32_t i, int32_t cx, int hot)
{
    const char *lb, *un;
    char val[16];
    int32_t ratio, k, cy = 84, n = 11, a;
    uint32_t ic;
    const fmgf_t *f = fmg_field(pg, i);
    uint16_t lab = hot ? FA_OR : FA_CR;
    fmg_card_at(pg, i, -1, &lb, val, &un, &ratio, &ic);
    n = f->kind == FK_NAME && f->b == FN_RANGE ? 6 : 11;
    for (k = 0; k < n; k++) {                              /* the scale: ticks, then the numerals or the ranges */
        int32_t ai = (-135 + 270 * k / (n - 1) + 360) % 360 * 1024 / 360, sn = SINE[ai & 1023], cs = SINE[(ai + 256) & 1023];
        cv_line_t(cx + sn * 22 / 32768, cy - cs * 22 / 32768, cx + sn * 25 / 32768, cy - cs * 25 / 32768, FA_CR, 1);
        if (n == 6 ? (k == 0 || k == 5) : (k == 0 || k == 5 || k == 10)) {
            char t[8];
            if (n == 6)
                str_cpy(t, FMG_RANGE[k], 8);
            else if (f->kind == FK_PAR && f->a == P_E0) {       /* the cutoff's dial: -5 0 +5 */
                t[0] = k == 10 ? '+' : 0;
                fmt_int(t + (k == 10), k - 5);
            } else
                fmt_int(t, k);
            fa_ctext(cx + sn * 31 / 32768, cy - cs * 31 / 32768 - 3, &AF_X, t, FA_MU);
        }
    }
    if (hot)                                               /* the cursor: an orange ring round the skirt */
        for (k = 0; k < 64; k++)
            cv_pset(cx + SINE[k * 16 & 1023] * 21 / 32768, cy - SINE[(k * 16 + 256) & 1023] * 21 / 32768, FA_OR);
    fa_knob_draw(cx, cy);
    a = (ratio < 0 ? 0 : ratio) * 270 / 1000 + 225;        /* the pointer: a dot between the cap and the skirt */
    {
        int32_t ai = a % 360 * 1024 / 360, px = cx + SINE[ai & 1023] * 12 / 32768, py = cy - SINE[(ai + 256) & 1023] * 12 / 32768;
        cv_rect(px - 1, py - 1, 3, 3, FA_RGB(0xF6, 0xF1, 0xE3));
    }
    fa_ctext(cx, 38, &AF_X, f->lb, lab);
    if (f->kind == FK_NAME && f->b == FN_RANGE) {
        str_cpy(val, FMG_RANGE[clamp((int32_t)fmg_byte(f->a), 0, 5)], 16);
    } else {
        fa_val(val, ratio, f);
    }
    fa_ctext(cx, 112, &AF_S, val, FA_OR);
    {
        char kn[5] = {'K', (char)('1' + i), 0, 0, 0};
        if (pg == 6u)                                      /* FX: the parameter each knob sets (the effect is the label above) */
            str_cpy(kn, i == 3u ? "GAIN" : "MIX", 5);
        fa_ctext(cx, 127, &AF_X, kn, FA_RGB(0x5D, 0x55, 0x4A));
    }
}

static void fa_switch(uint32_t pg, uint32_t ex, int32_t cx, int hot)
{
    const fmgf_t *f = fmg_field(pg, 4u + ex);
    int on = fmg_sw_on(f), x = cx - 15, y = pg == 0u ? 154 : 163;   /* CTRL: higher, no ON / OFF under it (VOICES below) */
    uint16_t col = pg == 0u ? FA_RED : pg == 2u ? FA_BLUE : on ? FA_OR : FA_RGB(0xD9, 0xD2, 0xC0);
    if (hot)
        cv_frame(x - 3, y - 3, 36, 20, FA_OR);
    cv_rect(x, y, 31, 14, FA_RGB(0x4A, 0x44, 0x3D));
    cv_rect(x + 1, y + 1, 29, 12, FA_RGB(0x0D, 0x0C, 0x0B));
    cv_rect(on ? x + 15 : x + 2, y + 2, 14, 10, col);
    fa_ctext(cx, pg == 0u ? 141 : 150, &AF_X, f->lb, hot ? FA_OR : FA_CR);
    if (pg != 0u)
        fa_ctext(cx, 182, &AF_X, on ? "ON" : "OFF", on ? (pg == 2u ? FA_BLUE : FA_OR) : FA_MU);
}

/* CTRL's VOICES: MONO | POLY, the chosen half filled */
static void fa_voices(int hot)
{
    int poly = TSEL->p[P_VOICE] == V_POLY;
    int32_t x = 60, y = 184, ty = y + 1 + (13 - AF_X_CAP_H) / 2 - AF_X_CAP_Y;   /* the cap height centred in the 13 rows */
    const uint16_t dk = FA_RGB(0x0D, 0x0C, 0x0B), ink = FA_RGB(0x1A, 0x0F, 0x06), bg = FA_RGB(0xD9, 0xD2, 0xC0);   /* beige */
    if (hot)
        cv_frame(x - 2, y - 2, 124, 19, bg);                 /* the cursor: a second beige border */
    cv_rect(x, y, 120, 15, bg);
    cv_rect(x + 1, y + 1, 118, 13, dk);
    cv_rect(poly ? x + 60 : x + 1, y + 1, 59, 13, bg);
    cv_text_on(x + 30 - text_w(&AF_X, "MONO") / 2, ty, &AF_X, "MONO", poly ? FA_MU : ink, poly ? dk : bg);
    cv_text_on(x + 90 - text_w(&AF_X, "POLY") / 2, ty, &AF_X, "POLY", poly ? ink : FA_MU, poly ? bg : dk);
}

static void fa_graph(uint32_t pg)
{
    int32_t k[4], x, i;
    uint32_t j;
    int16_t pt[12];
    const char *lb, *un;
    char val[16];
    uint32_t ic;
    int32_t ratio;
    for (j = 0; j < 4u; j++) {
        fmg_card_at(pg, j, -1, &lb, val, &un, &ratio, &ic);
        k[j] = ratio < 0 ? 0 : ratio;                       /* 0..1000 */
    }
    cv_rect(14, 146, 212, 46, FA_LINE);
    cv_rect(15, 147, 210, 44, FA_BOX);
    if (pg == 3u) {                                         /* the ladder's response: FMINI_GRAPH, the cutoff's singing pitch moving across it */
        int32_t fo = FMINI_CUT[clamp((int32_t)TSEL->p[P_E0], 0, 127)], e = clamp((int32_t)TSEL->p[P_E1], 0, 127), er, ef, px = 0, py = 0, xs;
        er = e >= 120 ? 15 : e >> 3;                        /* between two of the table's 17 emphasis rows (0, 8 .. 120, 127) */
        ef = e >= 120 ? (e - 120) * 256 / 7 : (e & 7) * 32;
        xs = 17 + (fo - 24 * 16) * 206 / (112 * 16);        /* the singing pitch: a dotted line (the screen: MIDI 24 .. 136) */
        for (i = 0; i < 44; i += 4)
            cv_pset(xs, 148 + i, FA_MU);
        for (i = 0; i <= 206; i += 2) {
            int32_t c = (((24 * 16 + i * (112 * 16) / 206) - fo) * 256 / 24) + 48 * 256, ix, fr, a, b, db, y;
            c = clamp(c, 0, 71 * 256);
            ix = c >> 8;
            fr = c & 255;
            a = FMINI_GRAPH[er][ix] + (((FMINI_GRAPH[er][ix + 1] - FMINI_GRAPH[er][ix]) * fr) >> 8);
            b = FMINI_GRAPH[er + 1][ix] + (((FMINI_GRAPH[er + 1][ix + 1] - FMINI_GRAPH[er + 1][ix]) * fr) >> 8);
            db = (a + (((b - a) * ef) >> 8)) / 8;
            y = 190 - (clamp(db, -50, 30) + 50) / 2;        /* -50 .. +30 dB over the 40 rows */
            x = 17 + i;
            if (i)
                cv_line_t(px, py, x, y, FA_OR, 2);
            px = x;
            py = y;
        }
        fa_ltext(18, 198, &AF_X, "24 DB/OCT LADDER LOW PASS", FA_MU);
    } else {                                                /* a contour: attack, decay, sustain, release */
        int32_t a = 5 + k[0] * 25 / 1000, dd = 5 + k[1] * 30 / 1000, su = k[2] * 2 / 10 - 100;
        pt[0] = 0; pt[1] = -100;
        pt[2] = (int16_t)a; pt[3] = 100;
        pt[4] = (int16_t)(a + dd); pt[5] = (int16_t)su;
        pt[6] = (int16_t)(a + dd + 20); pt[7] = (int16_t)su;
        pt[8] = (int16_t)(a + dd + 35); pt[9] = -100;
        fa_poly(pt, 5, 19, 150, 202, 38, FA_OR);
        fa_ltext(18, 198, &AF_X, "ATTACK DECAY SUSTAIN", FA_MU);
    }
}

/* a smaller knob (a scale of ticks, no numerals) for a control that has no K slot: its label above, its value below */
static void fa_knob_mini(uint32_t pg, uint32_t i, int32_t cx, int32_t cy, int hot)
{
    const char *lb, *un;
    char val[16];
    int32_t ratio, k, a;
    uint32_t ic;
    const fmgf_t *f = fmg_field(pg, i);
    fmg_card_at(pg, i, -1, &lb, val, &un, &ratio, &ic);
    for (k = 0; k < 11; k++) {
        int32_t ai = (-135 + 270 * k / 10 + 360) % 360 * 1024 / 360, sn = SINE[ai & 1023], cs = SINE[(ai + 256) & 1023];
        cv_line_t(cx + sn * 22 / 32768, cy - cs * 22 / 32768, cx + sn * 25 / 32768, cy - cs * 25 / 32768, FA_CR, 1);
    }
    if (hot)
        for (k = 0; k < 64; k++)
            cv_pset(cx + SINE[k * 16 & 1023] * 21 / 32768, cy - SINE[(k * 16 + 256) & 1023] * 21 / 32768, FA_OR);
    fa_knob_draw(cx, cy);
    a = (ratio < 0 ? 0 : ratio) * 270 / 1000 + 225;
    {
        int32_t ai = a % 360 * 1024 / 360, px = cx + SINE[ai & 1023] * 12 / 32768, py = cy - SINE[(ai + 256) & 1023] * 12 / 32768;
        cv_rect(px - 1, py - 1, 3, 3, FA_RGB(0xF6, 0xF1, 0xE3));
    }
    fa_ctext(cx, 138, &AF_X, lb, hot ? FA_OR : FA_CR);
    if (f->kind != FK_NAME && (f->kind != FK_GLB || GP[f->a].fmt == F_PCT))
        fa_val(val, ratio, f);
    fa_ctext(cx, 201, &AF_S, val, FA_OR);
}

/* a 3 x 5 pixel font for the faders' small labels (A..Z): three bits a row, the top row first */
static const uint16_t FA_TINY[26] = {
    0x2BED, 0x6BAE, 0x3923, 0x6B6E, 0x79A7, 0x79A4, 0x396B, 0x5BED, 0x7497, 0x126A, 0x5BAD, 0x4927, 0x5FED,
    0x6B6D, 0x2B6A, 0x6BA4, 0x2B73, 0x6BAD, 0x388E, 0x7492, 0x5B6F, 0x5B6A, 0x5BFD, 0x5AAD, 0x5A92, 0x72A7,
};
/* the 4..5 x 6 glyphs of the letters the faders use (a '#' is ink), top row first; other letters: the 3 x 5 font above */
static const char *const FA_TINY6[][7] = {
    {"A", ".##.", "#..#", "#..#", "####", "#..#", "#..#"},
    {"C", ".###", "#...", "#...", "#...", "#...", ".###"},
    {"D", "###.", "#..#", "#..#", "#..#", "#..#", "###."},
    {"E", "####", "#...", "###.", "#...", "#...", "####"},
    {"I", "###", ".#.", ".#.", ".#.", ".#.", "###"},
    {"M", "#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#"},
    {"N", "#..#", "##.#", "##.#", "#.##", "#.##", "#..#"},
    {"O", ".##.", "#..#", "#..#", "#..#", "#..#", ".##."},
    {"R", "###.", "#..#", "###.", "#.#.", "#..#", "#..#"},
    {"T", "#####", "..#..", "..#..", "..#..", "..#..", "..#.."},
    {"Y", "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#.."},
};
static int32_t fa_tiny_w(char ch)
{
    uint32_t k;
    for (k = 0; k < NELEM(FA_TINY6); k++)
        if (FA_TINY6[k][0][0] == ch)
            return (int32_t)str_len(FA_TINY6[k][1]);
    return 3;
}
static void fa_tiny_text(int32_t cx, int32_t y, const char *s, uint16_t fg)
{
    int32_t w = -1, x, r, c;
    const char *p;
    for (p = s; *p; p++)
        w += fa_tiny_w(*p) + 1;
    x = cx - w / 2;
    for (p = s; *p; p++) {
        uint32_t k;
        int32_t gw = fa_tiny_w(*p);
        for (k = 0; k < NELEM(FA_TINY6); k++)
            if (FA_TINY6[k][0][0] == *p) {
                for (r = 0; r < 6; r++)
                    for (c = 0; c < gw; c++)
                        if (FA_TINY6[k][1 + r][c] == '#')
                            cv_pset(x + c, y + r, fg);
                break;
            }
        if (k == NELEM(FA_TINY6) && *p >= 'A' && *p <= 'Z') {
            uint32_t g = FA_TINY[*p - 'A'];
            for (r = 0; r < 5; r++)
                for (c = 0; c < 3; c++)
                    if ((g >> (14 - (r * 3 + c))) & 1u)
                        cv_pset(x + c, y + r, fg);
        }
        x += gw + 1;
    }
}

/* the minimal horizontal fader of the FX page (DECAY / TIME / RATE / TONE): a small label, a thin track filled up to
 * the value, the value below */
static void fa_fader(uint32_t pg, uint32_t i, int32_t cx, int hot)
{
    const char *lb, *un;
    char val[16];
    int32_t ratio, x = cx - 22, y = 168, fw;
    uint32_t ic;
    const fmgf_t *f = fmg_field(pg, i);
    fmg_card_at(pg, i, -1, &lb, val, &un, &ratio, &ic);
    fw = (ratio < 0 ? 0 : ratio) * 42 / 1000;
    if (hot)
        cv_frame(x - 3, y - 4, 50, 15, FA_OR);
    cv_rect(x, y, 44, 7, FA_LINE);
    cv_rect(x + 1, y + 1, 42, 5, FA_BOX);
    cv_rect(x + 1, y + 1, fw, 5, FA_OR);
    cv_rect(x + 1 + fw - (fw > 1 ? 1 : 0), y - 1, 2, 9, FA_RGB(0xF6, 0xF1, 0xE3));     /* the cap */
    fa_tiny_text(cx, 152, lb, hot ? FA_OR : FA_RGB(0xA6, 0x9C, 0x88));
    if (f->kind != FK_NAME && (f->kind != FK_GLB || GP[f->a].fmt == F_PCT))
        fa_val(val, ratio, f);
    else if (f->a == G_CRATE)                              /* RATE in Hz */
        str_cpy(val + str_len(val), "Hz", 4);
    fa_ctext(cx, 186, &AF_S, val, FA_OR);
}

/* the FX page's EDIT button under each effect: a beige frame, a dark inside, EDIT; the cursor an orange frame */
static void fa_edit_btn(int32_t cx, int hot)
{
    const uint16_t dk = FA_RGB(0x0D, 0x0C, 0x0B), bg = FA_RGB(0xD9, 0xD2, 0xC0);
    int32_t x = cx - 22, y = 156, ty = y + 1 + (19 - AF_X_CAP_H) / 2 - AF_X_CAP_Y;
    if (hot)
        cv_frame(x - 3, y - 3, 50, 25, FA_OR);
    cv_rect(x, y, 44, 21, bg);
    cv_rect(x + 1, y + 1, 42, 19, dk);
    cv_text_on(cx - text_w(&AF_X, "EDIT") / 2, ty, &AF_X, "EDIT", hot ? FA_OR : FA_CR, dk);
}

/* one knob of an FX pop-up: a scale of ticks, the dial and its pointer, the label above and the value below */
static void fa_pop_knob(uint32_t pg, uint32_t i, int32_t cx, int32_t cy, int hot)
{
    const char *lb, *un;
    char val[16];
    int32_t ratio, k, a;
    uint32_t ic;
    const fmgf_t *f = fmg_field(pg, i);
    fmg_card_at(pg, i, -1, &lb, val, &un, &ratio, &ic);
    for (k = 0; k < 11; k++) {
        int32_t ai = (-135 + 270 * k / 10 + 360) % 360 * 1024 / 360, sn = SINE[ai & 1023], cs = SINE[(ai + 256) & 1023];
        cv_line_t(cx + sn * 22 / 32768, cy - cs * 22 / 32768, cx + sn * 25 / 32768, cy - cs * 25 / 32768, FA_CR, 1);
    }
    if (hot)
        for (k = 0; k < 64; k++)
            cv_pset(cx + SINE[k * 16 & 1023] * 21 / 32768, cy - SINE[(k * 16 + 256) & 1023] * 21 / 32768, FA_OR);
    fa_knob_draw(cx, cy);
    a = (ratio < 0 ? 0 : ratio) * 270 / 1000 + 225;
    {
        int32_t ai = a % 360 * 1024 / 360, px = cx + SINE[ai & 1023] * 12 / 32768, py = cy - SINE[(ai + 256) & 1023] * 12 / 32768;
        cv_rect(px - 1, py - 1, 3, 3, FA_RGB(0xF6, 0xF1, 0xE3));
    }
    fa_ctext(cx, cy - 42, &AF_X, f->lb, hot ? FA_OR : FA_CR);
    if (f->kind == FK_PAR || (f->kind == FK_GLB && GP[f->a].fmt == F_PCT))
        fa_val(val, ratio, f);
    fa_ctext(cx, cy + 32, &AF_S, val, FA_OR);
    {
        char kn[4] = {'K', (char)('1' + i), 0, 0};
        fa_ctext(cx, cy + 50, &AF_X, kn, FA_RGB(0x5D, 0x55, 0x4A));
    }
}
static void fa_fx_dialog(void)
{
    uint32_t pg = 6u + fmg_fx, i;
    const uint16_t bgd = FA_RGB(0x1B, 0x19, 0x17);
    cv_rect(12, 42, 216, 164, FA_OR);
    cv_rect(14, 44, 212, 160, bgd);
    fa_ctext(120, 50, &AF_S, FMG_PAGE[pg].name, FA_CR);
    cv_rect(20, 66, 200, 1, FA_RULE);
    for (i = 0; i < 4u; i++)
        fa_pop_knob(pg, i, 39 + (int32_t)i * 54, 112, fmg_cur[pg] == i);
    fa_ctext(120, 190, &AF_X, fmg_fx == 2u ? "K1-K4  SCL=TAP  FX=NEXT" : "K1-K4  SELECT  ALGO  FX=NEXT", FA_MU);
}

static void fa_wave_boxes(uint32_t pg, uint32_t cur)
{
    static const char *const WV[6] = {"TRI", "SHARK", "SAW", "SQR", "PULSE W", "PULSE N"};
    uint32_t j;
    for (j = 0; j < 3u; j++) {                             /* each box sits under its oscillator's knob (the columns of fa_scene) */
        int32_t cx = 36 + (int32_t)j * 56, x = cx - 26, w = clamp((int32_t)fmg_byte(FG_W1 + j), 0, 5);
        int hot = cur == 4u + j;
        cv_rect(x, 146, 52, 36, hot ? FA_OR : FA_LINE);
        cv_rect(x + 1, 147, 50, 34, FA_BOX);
        fa_poly(FA_WAVE[w], FA_WAVE_N[w], x + 5, 151, 42, 26, FA_OR);
        fa_ctext(cx, 188, &AF_X, WV[w], hot ? FA_OR : FA_CR);
    }
    fa_knob_mini(pg, 7, 204, 173, cur == 7u);
}

/* ------------------------------------------------------- presets --- */
static uint8_t fa_base;                                   /* the factory patch last loaded (0..199): the one PRESETS goes on from, edited or not (an edit makes SLOT OWN) */
/* 1.14.1: no user bank. The 200 slots are the presets, all of them the same way (000..199, all factory patches, then
 * whatever was saved): PRESETS walks them, SAVE writes one (fmini_bank.c) */
static int fa_on_user(void) { return 0; }                 /* (kept for the callers: a track is never on a user preset now) */
/* the slot number on the screen: three digits, 000..199 */
static void fa_slot_num(char *l, uint32_t k)
{
    l[0] = (char)('0' + (k / 100u) % 10u);
    l[1] = (char)('0' + (k / 10u) % 10u);
    l[2] = (char)('0' + k % 10u);
    l[3] = 0;
}

/* SAVE: a small dialog. PRESETS picks the slot U01..U32, SELECT moves along the name and ALGORITHM changes the letter,
 * SAVE writes it, any page button cancels */
static uint8_t fa_sv;                                     /* 1: the dialog is open */
static uint8_t fa_sv_conf, fa_sv_yes;                     /* 1: the OVERWRITE question is up; yes: ALGORITHM put it on YES (NO by default) */
static uint8_t fa_sv_slot, fa_sv_pos;
static char fa_sv_name[13];
static uint32_t fa_msg_until;                             /* "SAVED U05" shown until fm1_ms passes this */
static char fa_msg[16];
static const char FA_CH[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";

static uint32_t fa_cur_slot(void)                         /* the slot the track plays (OWN: the patch it came from) */
{
    const track_t *t = TSEL;
    return t->p[P_E7] >= 0 && t->p[P_E7] < (int32_t)FMINI_NSLOT ? (uint32_t)t->p[P_E7] : fa_base % FMINI_NSLOT;
}
static void fa_sv_fill(void)                              /* the slot's name, as it is now (overwrite) */
{
    str_cpy(fa_sv_name, ENGINES[ENGI_FMINI]->presets[fa_sv_slot].name, 13);
}
static void fa_sv_open(void)
{
    fa_sv = 1;
    fa_sv_conf = 0;
    fa_sv_pos = 0;
    fa_sv_slot = (uint8_t)fa_cur_slot();                  /* the slot being played: SAVE overwrites it */
    fa_sv_fill();
}
static void fa_say(const char *a, uint32_t k)
{
    char l[4];
    fa_slot_num(l, k);
    str_cpy(fa_msg, a, 9);
    str_cpy(fa_msg + str_len(fa_msg), l, 4);
    fa_msg_until = fm1_ms + 1500u;
}
static uint8_t fa_last_user;
static int fa_save_slot(uint32_t k, const char *name);    /* (below: the track's sound into slot k) */
static void fa_sv_do(void)
{
    uint32_t k = fa_sv_slot, n = str_len(fa_sv_name);
    int rc;
    fa_sv_conf = 0;
    while (n && fa_sv_name[n - 1u] == ' ')                /* no trailing blanks */
        fa_sv_name[--n] = 0;
    rc = fa_save_slot(k, fa_sv_name);
    fa_sv = 0;
    if (rc == 0) {
        TSEL->p[P_E7] = (int16_t)k;                       /* the track now plays the slot it saved */
        fa_base = (uint8_t)k;
        fa_say("SAVED ", k);
    } else
        str_cpy(fa_msg, "SAVE ERROR", 16), fa_msg_until = fm1_ms + 1500u;
    ui.force = 1;
}
static void fa_sv_press(void)                             /* SAVE in the dialog: the question, then YES writes / NO goes back */
{
    if (!fa_sv_conf) {
        fa_sv_conf = 1;
        fa_sv_yes = 0;
        ui.force = 1;
        return;
    }
    if (fa_sv_yes)
        fa_sv_do();
    else {
        fa_sv_conf = 0;
        ui.force = 1;
    }
}
static int fa_msg_on(void) { return fa_msg_until && (int32_t)(fm1_ms - fa_msg_until) < 0; }

static void fa_dialog(void)
{
    uint32_t k = fa_sv_slot, i;
    char l[4];
    const uint16_t bgd = FA_RGB(0x1B, 0x19, 0x17);
    cv_rect(10, 26, 220, 188, FA_OR);
    cv_rect(12, 28, 216, 184, bgd);
    fa_slot_num(l, k);
    fa_pc_text(120, 36, &AF_M, "SAVE PRESET", FA_CR, bgd);
    fa_pl_text(22, 62, &AF_L, l, FA_OR, bgd);                    /* every slot holds a preset: SAVE replaces it */
    fa_pl_text(22 + text_w(&AF_L, l) + 10, 70, &AF_M, "OVERWRITE", FA_RED, bgd);
    {   int32_t x = 20;                                   /* the name, letter by letter (each cell as wide as its letter), the cursor underlined */
        for (i = 0; i < 12u; i++) {
            char c[2] = {fa_sv_name[i] ? fa_sv_name[i] : ' ', 0};
            int32_t w = text_w(&AF_L, c), cw = c[0] == ' ' ? text_w(&AF_L, "M") : w;
            cv_rect(x, 118, cw, 1, i == fa_sv_pos ? FA_OR : FA_LINE);
            if (i == fa_sv_pos)
                cv_rect(x, 119, cw, 1, FA_OR);
            if (c[0] != ' ')
                fa_pl_text(x + (cw - w) / 2, 100, &AF_L, c, i == fa_sv_pos ? FA_OR : FA_CR, bgd);
            x += cw;
        }
    }
    fa_pc_text(120, 146, &AF_S, "PRESETS SLOT   SELECT/ALGO NAME", FA_MU, bgd);
    fa_pc_text(120, 170, &AF_S, "SAVE = NEXT   ANY PAGE = CANCEL", FA_MU, bgd);
}

/* SAVE again: the question. NO is the default; ALGORITHM turns it to YES, SAVE on YES writes the slot, SAVE on NO
 * goes back to the name */
static void fa_confirm_dialog(void)
{
    const uint16_t bgd = FA_RGB(0x1B, 0x19, 0x17);
    char l[4], nm[24];
    uint32_t i;
    static const int32_t BX[2] = {34, 126};              /* NO, YES: the boxes */
    static const char *const BL[2] = {"NO", "YES"};
    fa_slot_num(l, fa_sv_slot);
    str_cpy(nm, l, 24);
    str_cpy(nm + str_len(nm), " ", 24 - str_len(nm));
    str_cpy(nm + str_len(nm), fa_sv_name, 24 - str_len(nm));
    cv_rect(10, 46, 220, 148, FA_OR);
    cv_rect(12, 48, 216, 144, bgd);
    fa_pc_text(120, 58, &AF_L, "OVERWRITE?", FA_RED, bgd);
    fa_pc_text(120, 98, &AF_M, nm, FA_CR, bgd);
    for (i = 0; i < 2u; i++) {
        uint8_t sel = (i == 1u) == (fa_sv_yes != 0);
        cv_rect(BX[i], 130, 80, 36, sel ? FA_OR : FA_LINE);
        if (!sel)
            cv_rect(BX[i] + 2, 132, 76, 32, bgd);
        fa_pc_text(BX[i] + 40, 139, &AF_L, BL[i], sel ? bgd : FA_CR, bgd);
    }
    fa_pc_text(120, 176, &AF_S, "ALGO: YES / NO   SAVE: OK", FA_MU, bgd);
}

/* RESTORE: the editor asks to write all the presets. NO is the default; ALGORITHM turns it to YES, SAVE on YES accepts
 * (then the progress), SAVE on NO or a page button refuses */
static void fa_restore_dialog(void)
{
    const uint16_t bgd = FA_RGB(0x1B, 0x19, 0x17);
    static const int32_t BX[2] = {34, 126};              /* NO, YES: the boxes */
    static const char *const BL[2] = {"NO", "YES"};
    uint32_t i;
    cv_rect(10, 40, 220, 160, FA_OR);
    cv_rect(12, 42, 216, 156, bgd);
    /* what it is about: a backup (all 200 presets), one preset's reset, or every factory preset's */
    {
        char q[24] = "ALL 200 PRESETS";
        const char *top = fb_rs_k == FB_RS_ALL ? "RESET ALL" : fb_rs_k == FB_NSLOT ? "OVERWRITE" : "RESET";
        if (fb_rs_k < FB_NSLOT) {
            str_cpy(q, "PRESET ", 24);
            fa_slot_num(q + 7, fb_rs_k);
        } else if (fb_rs_k == FB_RS_ALL)
            str_cpy(q, "200 FACTORY", 24);
        if (fb_rs == FB_RS_OK) {                          /* accepted: the device is writing (a backup) or has reset */
            fa_pc_text(120, 84, &AF_L, fb_rs_k == FB_NSLOT ? "RESTORING" : "RESETTING", FA_OR, bgd);
            fa_pc_text(120, 124, &AF_M, q, FA_CR, bgd);
            fa_pc_text(120, 170, &AF_S, "DO NOT TURN OFF", FA_MU, bgd);
            return;
        }
        str_cpy(q + str_len(q), "?", 2);
        fa_pc_text(120, 52, &AF_L, top, FA_RED, bgd);
        fa_pc_text(120, 92, &AF_M, q, FA_CR, bgd);
    }
    for (i = 0; i < 2u; i++) {
        uint8_t sel = (i == 1u) == (fb_rs_yes != 0);
        cv_rect(BX[i], 118, 80, 36, sel ? FA_OR : FA_LINE);
        if (!sel)
            cv_rect(BX[i] + 2, 120, 76, 32, bgd);
        fa_pc_text(BX[i] + 40, 127, &AF_L, BL[i], sel ? bgd : FA_CR, bgd);
    }
    fa_pc_text(120, 176, &AF_S, "ALGO: YES / NO   SAVE: OK", FA_MU, bgd);
}

/* ARP: a pop-up like SAVE's. KNOB 1..4 are MODE, RATE, OCT, GATE; SELECT picks a line (the fifth is the tempo) and
 * ALGORITHM changes it; ARP again, HOME or a page button closes it. The arpeggio goes on after closing */
static uint8_t fa_ar;                                     /* 1: the pop-up is open */
static uint8_t fa_ar_row;                                 /* 0 MODE, 1 RATE, 2 OCT, 3 GATE, 4 HOLD, 5 BPM */
#define FA_AR_N 6u
static const uint8_t FA_AR_ID[5] = {P_AMODE, P_ARATE, P_AOCT, P_AGATE, P_AHOLD};
static const char *const FA_AR_LB[FA_AR_N] = {"MODE", "RATE", "OCTAVES", "GATE", "HOLD", "TEMPO"};

static void fa_ar_turn(uint32_t row, int32_t s)
{
    track_t *t = TSEL;
    if (row < 5u) {
        const param_desc_t *d = track_desc(t, FA_AR_ID[row]);
        int16_t *vp = &t->p[FA_AR_ID[row]];
        *vp = (int16_t)param_turn(d, *vp, s);
        motion_capture(t, FA_AR_ID[row], *vp);
        ml_knob(t, FA_AR_ID[row]);
    } else {
        song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + fmg_acc(EN_ALGO, s, 200), GP[G_BPM].min, GP[G_BPM].max);
        fmg_tempo_sync();
    }
}
/* a pop-up's frame: the orange border, the title (the same size for every pop-up) */
static void fa_frame(const char *title)
{
    cv_rect(10, 24, 220, 186, FA_OR);
    cv_rect(12, 26, 216, 182, FA_RGB(0x1B, 0x19, 0x17));
    fa_ctext(120, 36, &AF_M, title, FA_CR);
}
/* a line of a pop-up: the label on the left, the value on the right, the highlight under the chosen one */
static void fa_line(int32_t y, const char *lb, const char *val, int hot)
{
    if (hot)
        cv_rect(16, y - 5, 208, 21, FA_RGB(0x2E, 0x28, 0x22));
    fa_ltext(22, y, &AF_M, lb, hot ? FA_OR : FA_CR);
    fa_rtext(218, y, &AF_M, val, hot ? FA_OR : FA_CR);
}
static void fa_ar_dialog(void)
{
    const track_t *t = TSEL;
    uint32_t i;
    fa_frame("ARPEGGIATOR");
    for (i = 0; i < FA_AR_N; i++) {
        char val[16];
        const char *un;
        if (i < 5u)
            param_format(track_desc(t, FA_AR_ID[i]), t->p[FA_AR_ID[i]], val, &un);
        else
            fmt_int(val, song.g[G_BPM]);
        fa_line(62 + (int32_t)i * 22, FA_AR_LB[i], val, i == fa_ar_row);
    }
    fa_ctext(120, 196, &AF_S, "SELECT  ALGO  K1-K4  ARP=CLOSE", FA_MU);
}

/* the ARP button: a tap turns it on / off (on another screen, while it is on, a tap only opens the pop-up); held HOLD on / off.
 * The pop-up stays open until it is closed: no timeout */
static uint8_t fa_arp_down, fa_arp_done, fa_ar_last = 1;
/* SEQ held (as ARP's HOLD): the sequencer's settings pop-up, MODE RATE GATE TEMPO (the TEMPO row: TAP TEMPO). A tap on
 * the SEQ button is the sequence pop-up as before (the grid); the hold is decided on release (fa_seq_done: it was one) */
static uint8_t fa_sc;                                     /* 1: the settings pop-up is open */
static uint8_t fa_sc_row;                                 /* 0 MODE, 1 RATE, 2 GATE, 3 TEMPO */
#define FA_SC_N 4u
static const uint8_t FA_SC_ID[3] = {P_SMODE, P_SDIV, P_SGATE};
static const char *const FA_SC_LB[FA_SC_N] = {"MODE", "RATE", "GATE", "TEMPO"};
static uint8_t fa_seq_down, fa_seq_done;
static uint32_t fa_seq_t0;
static uint32_t fa_arp_t0;

/* ----------------------------------------------------------- SEQ --- */
/* SEQ: a pop-up with the 16 slots of a sequence of up to 16 notes. REC (it lights) records the notes you play into the
 * next slots (a take of 16 ends by itself; REC again ends it), PLAY / STOP plays the loop at the TEMPO (ALGORITHM
 * changes it); the slot playing is lit, and so are the keys of the notes that sound. The steps are the track's own */
static uint8_t fa_sq;                                     /* 1: the pop-up is open */
static uint8_t fa_sq_rec;                                 /* REC armed */
static uint8_t fa_sq_cur;                                 /* the slot the next note goes into */
static uint8_t fa_sq_fresh;                               /* armed, nothing played yet: the old take is still there */
static uint32_t fa_sq_blink(void) { return fa_sq_rec ? (fm1_ms >> 8) & 1u : 0u; }
static uint32_t fa_sq_title_until;                        /* SEQ opened: the word SEQUENCER shows over the grid for 0.5 s */
static int fa_sq_title_on(void) { return fa_sq_title_until && (int32_t)(fm1_ms - fa_sq_title_until) < 0; }
static uint8_t fa_rec_down, fa_rec_done;                  /* REC held: the tap is due on release; HOLD REC TO CLEAR done */
static uint32_t fa_rec_t0;

/* HOLD REC TO CLEAR: the 64 steps go empty; the REC lamp keeps its state */
static void fa_seq_clear(void)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        memset(&trk[0].step[i], 0, sizeof trk[0].step[i]);
    fa_sq_cur = 0;
    fa_sq_fresh = 1;
}

/* TAP TEMPO (over the ARP or SEQ pop-up): the BPM in big type, EDIT taps it (the EDIT lamp blinks at the tempo);
 * ALGORITHM turns the tempo; SELECT, a knob or any other button closes it (and that press does nothing else) */
static uint8_t fa_tp;                                     /* 1: the TAP TEMPO pop-up is open */
#define FA_TP_N 5u                                        /* the taps averaged: the last five (four beats) */
static uint32_t fa_tp_ts[FA_TP_N];                        /* the times of the taps of this measurement, the last one last */
static uint32_t fa_tp_n;                                  /* their number (0 at the start: the first tap sets no tempo) */
static void fa_tp_begin(void)
{
    fa_tp = 1;
    fa_tp_n = 0;
}
static void fa_tp_turn(int32_t s)
{
    song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + fmg_acc(EN_ALGO, s, 200), GP[G_BPM].min, GP[G_BPM].max);
    fmg_tempo_sync();
}
/* the tempo is the mean beat of the taps of this measurement: from the first two taps on, the span of the last five
 * (or fewer) over its beats, so one early or late tap moves it little. A tap less than 250 ms after the last is a
 * bounce (no tap); one more than 1500 ms after it starts a new measurement (40..240 BPM is what counts) */
static void fa_tp_tap(void)
{
    uint32_t now = fm1_ms, span, beats, k;
    if (fa_tp_n) {
        uint32_t dt = now - fa_tp_ts[fa_tp_n - 1u];
        if (dt < 250u)
            return;
        if (dt > 1500u)
            fa_tp_n = 0;
    }
    if (fa_tp_n == FA_TP_N) {                             /* full: the oldest tap goes */
        for (k = 1; k < FA_TP_N; k++)
            fa_tp_ts[k - 1u] = fa_tp_ts[k];
        fa_tp_n--;
    }
    fa_tp_ts[fa_tp_n++] = now;
    if (fa_tp_n < 2u)
        return;
    span = now - fa_tp_ts[0];
    beats = fa_tp_n - 1u;                                 /* the intervals: each one a beat */
    song.g[G_BPM] = (int16_t)clamp((int32_t)((60000u * beats + span / 2u) / span), GP[G_BPM].min, GP[G_BPM].max);
    fmg_tempo_sync();
}
static void fa_tp_draw(void)
{
    char b[8];
    cv_rect(26, 60, 188, 112, FA_OR);
    cv_rect(28, 62, 184, 108, FA_RGB(0x1B, 0x19, 0x17));
    fmt_int(b, song.g[G_BPM]);
    fa_ctext(120, 70, &AF_L, b, FA_OR);
    fa_ctext(120, 108, &AF_S, "BPM", FA_MU);
    fa_ctext(120, 136, &AF_S, "TAP TEMPO: button EDIT", FA_CR);
    fa_ctext(120, 153, &AF_X, "ALGO=TEMPO  OTHER KEY=CLOSE", FA_MU);
}
/* the SEQ settings pop-up: ALGORITHM turns the line SELECT picked; the TEMPO line opens TAP TEMPO (as the ARP's) */
static void fa_sc_turn(uint32_t row, int32_t s)
{
    track_t *t = TSEL;
    if (row < 3u) {
        const param_desc_t *d = track_desc(t, FA_SC_ID[row]);
        int16_t *vp = &t->p[FA_SC_ID[row]];
        *vp = (int16_t)param_turn(d, *vp, s);
        motion_capture(t, FA_SC_ID[row], *vp);
        ml_knob(t, FA_SC_ID[row]);
    } else {
        fa_tp_turn(s);
    }
}
static void fa_sc_dialog(void)
{
    const track_t *t = TSEL;
    uint32_t i;
    fa_frame("SEQUENCER");
    for (i = 0; i < FA_SC_N; i++) {
        char val[16];
        const char *un;
        if (i < 3u)
            param_format(track_desc(t, FA_SC_ID[i]), t->p[FA_SC_ID[i]], val, &un);
        else
            fmt_int(val, song.g[G_BPM]);
        fa_line(70 + (int32_t)i * 28, FA_SC_LB[i], val, i == fa_sc_row);
    }
    fa_ctext(120, 196, &AF_S, "SELECT  ALGO  K1-K3  SEQ=CLOSE", FA_MU);
}

static void fa_seq_note(track_t *t, uint32_t note)
{
    step_t *s;
    uint32_t i;
    if (!fa_sq_rec || t != &trk[0] || note > 127u)
        return;
    if (fa_sq_fresh) {                                    /* the first note of a take: the old one goes */
        for (i = 0; i < NSTEP; i++)
            memset(&t->step[i], 0, sizeof t->step[i]);
        fa_sq_fresh = 0;
        fa_sq_cur = 0;
    }
    if (fa_sq_cur >= NSTEP)
        return;
    s = &t->step[fa_sq_cur];
    memset(s, 0, sizeof *s);
    s->note[0] = (uint8_t)note;
    s->n = 1;
    s->time = ST_NOTE;
    s->vel = 100;
    fa_sq_cur++;
    t->p[P_SLEN] = fa_sq_cur;
    if (fa_sq_cur >= NSTEP)
        fa_sq_rec = 0;                                    /* 64 notes: the take is done */
}

/* 64 squares, 8 by 8: dark gray = empty, red = recorded, orange = the step that plays now; while REC is armed the
 * next square blinks cream */
/* the SEQUENCER pop-up: the full height of the screen, the width of the other pop-ups. 64 squares, 8 by 8, rounded:
 * dark gray = empty, red = recorded, orange = the step that plays now; while REC is armed the next square blinks cream */
#define SQ_PITCH 23
#define SQ_SIZE 18
static void fa_seq_dialog(void)
{
    const track_t *t = TSEL;
    uint32_t i, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), now = song.playing ? t->seq_idx % len : 99u;
    const uint16_t bg = FA_RGB(0x1B, 0x19, 0x17);
    char b[16];
    cv_rect(10, 0, 220, 240, FA_OR);
    cv_rect(12, 2, 216, 236, bg);
    for (i = 0; i < NSTEP; i++) {
        int32_t x = 31 + (int32_t)(i % 8u) * SQ_PITCH, y = 10 + (int32_t)(i / 8u) * SQ_PITCH;
        uint16_t c = FA_RGB(0x2A, 0x25, 0x20);            /* empty */
        if (i == now)
            c = FA_OR;
        else if (i < len && t->step[i].n)
            c = FA_RED;                                   /* recorded */
        else if (fa_sq_rec && i == fa_sq_cur && fa_sq_blink())
            c = FA_CR;                                    /* the next slot of the take */
        cv_rrect(x, y, SQ_SIZE, SQ_SIZE, 4, c, bg);
    }
    str_cpy(b, fa_sq_rec ? "REC" : song.playing ? "PLAY" : "STOP", 5);
    fa_ltext(20, 200, &AF_M, b, fa_sq_rec ? FA_RED : song.playing ? FA_OR : FA_MU);
    fmt_int(b, song.g[G_BPM]);
    fa_rtext(220, 200, &AF_M, b, FA_CR);
    fa_ctext(120, 214, &AF_X, "HOLD REC TO CLEAR", FA_MU);
    fa_ctext(120, 225, &AF_X, "REC  PLAY  ALGO=TEMPO  SEQ=CLOSE", FA_MU);
    if (fa_sq_title_on()) {                               /* SEQ just opened: the word, for 0.5 s */
        cv_rect(60, 96, 120, 26, FA_OR);
        cv_rect(62, 98, 116, 22, bg);
        fa_ctext(120, 102, &AF_M, "SEQUENCER", FA_OR);
    }
}

/* the keys of the notes that sound (the arpeggio, the sequence, MIDI): lit like the keys held */
static uint32_t fa_sound_keys(void)
{
    const track_t *t = &trk[0];
    uint32_t m = 0, i, k;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate)
            for (k = 0; k < 27u; k++)
                if (kb_map(t, k) == t->v[i].note)
                    m |= 1u << k;
    return m;
}

/* ----------------------------------------------------------- GLO --- */
/* GLO opens the global settings pop-up: MIDI IN (USB / MIDI / ALL), CHANNEL (ALL / 1..16), MIDI CC (every
 * knob, switch and wave box of every screen and the ARP's lines, each with its own CC, 0 = none; a CC sent from the
 * controller while the list is open is taken for the highlighted line), PITCH BEND (the range) and ABOUT (the splash
 * with the version).
 * SELECT picks a line, ALGORITHM changes it (or opens MIDI CC / ABOUT); GLO goes back, then closes. The settings are
 * kept in the flash settings record (favorites.factory rows 7 to 9: no engine of this firmware uses them).
 * MIDI CLOCK (OFF by default: the internal clock, or USB / TRS) makes the ARP and the SEQ follow that input's MIDI clock. */
#define FA_NPG 44u                                        /* the controls of the seven screens (FMG_PAGE: 8+8+8+4+4+4+8) */
#define FA_NCC (FA_NPG + FA_AR_N)                         /* .. and the ARP pop-up's six lines */
static uint8_t fa_gl;                                     /* 0 closed, 1 the list, 2 MIDI CC, 3 ABOUT */
static uint8_t fa_gl_row;                                 /* 0 MIDI IN, 1 CHANNEL, 2 MIDI CC, 3 MIDI CLOCK, 4 PITCH BEND, 5 ABOUT */
static uint8_t fa_clk;                                    /* MIDI CLOCK: 0 OFF (internal), 1 USB, 2 TRS: the global CLK */
static const char *const FA_CLK[3] = {"OFF", "USB", "TRS"};
static uint8_t fa_cc_i;                                   /* the control highlighted in MIDI CC */
static uint8_t fa_cc[FA_NCC];                             /* its CC, 0 = none */
static uint8_t fa_cfg_dirty;
static const char *const FA_SRC[3] = {"USB", "MIDI", "ALL"};       /* fm_midi_src 0, 1, 3 (2 was Bluetooth: gone) */
static const char *const FA_BEND[4] = {"+/-2", "+/-12", "+2/-12", "+2/-24"};
static const char *const FA_PG_TAG[8] = {"CTRL", "OSC", "MIX", "FILT", "F.ENV", "A.ENV", "FX", "ARP"};
/* the CC each control answers to out of the box: the usual ones where there is one (5 glide time, 7 volume, 71 emphasis,
 * 73 attack, 74 cutoff, 75 decay, 91 / 93 / 94 sends), the free ones in order for the rest */
static const uint8_t FA_CC0[FA_NCC] = {
    20, 5, 21, 22, 23, 24, 25, 26,                        /* CTRL: TUNE GLIDE MOD MIX LFO RATE, MOD OSC MOD FLT GLIDE LFO SQR */
    27, 28, 29, 30, 31, 33, 34, 62,                       /* OSC: ranges, DETUNE 2, waves, DETUNE 3 */
    35, 36, 37, 39, 40, 41, 42, 43,                       /* MIX: levels, NOISE, switches */
    74, 71, 44, 45,                                       /* FILT */
    46, 47, 48, 49,                                       /* F.ENV */
    73, 75, 50, 7,                                        /* A.ENV: ATTACK DECAY SUSTAIN LEVEL */
    91, 94, 93, 51, 52, 53, 54, 55,                       /* FX: REVERB DELAY CHORUS DRIVE, switches */
    56, 57, 58, 59, 60, 61,                               /* ARP: MODE RATE OCTAVES GATE HOLD TEMPO */
};
static int fa_cc_valid(uint32_t cc) { return cc == 0u || ml_free_cc(cc); }

/* the FX pop-ups' other globals (the settings still keep their bytes; a factory patch, a saved one and the autosave set them) */
static const uint8_t FA_GID2[8] = {G_RTYPE, G_RDAMP, G_DTIME, G_DFDBK, G_ENGGO, G_CDEPTH, G_INFO, G_MIDI};
static uint8_t *fa_cfg_byte(uint32_t i) { return &favorites.factory[7u + i / 32u][i % 32u]; }
static void fa_cfg_load(void)
{
    uint32_t i;
    if (*fa_cfg_byte(0) == 0xA7u && *fa_cfg_byte(1) == 0x4Fu) {
        uint32_t src = *fa_cfg_byte(2), ch = *fa_cfg_byte(3);
        fm_midi_src = (uint8_t)(src == 0u || src == 1u ? src : 3u);   /* (2 was Bluetooth) */
        fm_midi_ch = (uint8_t)(ch > 16u ? 0u : ch);
        fm_bend_mode = (uint8_t)(*fa_cfg_byte(4u + FA_NCC) & 3u);
        {   uint32_t b = *fa_cfg_byte(17u + FA_NCC);   /* 0xC0 | clock; an erased byte (0xFF) reads OFF */
            fa_clk = (b & 0xFCu) == 0xC0u && (b & 3u) <= 2u ? (uint8_t)(b & 3u) : 0u;
        }
        /* (the FX settings are not read back: each factory patch carries its own, a saved one and the autosave theirs) */
        fm_bt = 0;
        for (i = 0; i < FA_NCC; i++) {
            uint32_t c = *fa_cfg_byte(4u + i);
            fa_cc[i] = (uint8_t)(fa_cc_valid(c) ? c : FA_CC0[i]);
        }
    } else {
        fm_midi_src = 3;
        fm_midi_ch = 0;
        fm_bend_mode = 0;
        fm_bt = 0;
        fa_clk = 0;
        memcpy(fa_cc, FA_CC0, sizeof fa_cc);
    }
}
static void fa_cfg_save(void)
{
    uint32_t i;
    if (!fa_cfg_dirty && !fmg_gdirty)
        return;
    fa_cfg_dirty = 0;
    fmg_gdirty = 0;
    *fa_cfg_byte(0) = 0xA7;
    *fa_cfg_byte(1) = 0x4F;
    *fa_cfg_byte(2) = fm_midi_src;
    *fa_cfg_byte(3) = fm_midi_ch;
    for (i = 0; i < FA_NCC; i++)
        *fa_cfg_byte(4u + i) = fa_cc[i];
    *fa_cfg_byte(4u + FA_NCC) = fm_bend_mode;
    *fa_cfg_byte(17u + FA_NCC) = (uint8_t)(0xC0u | fa_clk);
    {
        static const uint8_t GID[4] = {G_RSIZE, G_DRREV, G_CRATE, G_DRLVL};
        for (i = 0; i < 4u; i++)
            *fa_cfg_byte(5u + FA_NCC + i) = (uint8_t)song.g[GID[i]];
        for (i = 0; i < 8u; i++)
            *fa_cfg_byte(9u + FA_NCC + i) = (uint8_t)song.g[FA_GID2[i]];
    }
    settings_save();                                      /* (deferred while anything plays) */
}
static void fa_gl_close(void)
{
    fa_gl = 0;
    fa_cfg_save();
}
static void fa_arp_show(void)
{
    fa_sv = 0;
    fa_sq = 0;
    fa_sq_rec = 0;
    fa_gl_close();
    fa_sc = 0;
    fa_ar = 1;
}

/* control n of the list -> its page and place, its tag and name */
static void fa_ctl_loc(uint32_t n, uint32_t *pg, uint32_t *i)
{
    uint32_t p = 0;
    while (p < 7u && n >= fmg_count(p) - (p == 0u)) {   /* (CTRL's VOICES has no CC: the list keeps its numbers) */
        n -= fmg_count(p) - (p == 0u);
        p++;
    }
    *pg = p;
    *i = n;
}
static void fa_ctl_name(uint32_t n, const char **tag, char *name)
{
    uint32_t pg, i;
    const char *lb;
    if (n >= FA_NPG) {
        *tag = FA_PG_TAG[7];
        str_cpy(name, FA_AR_LB[n - FA_NPG], 16);
        return;
    }
    fa_ctl_loc(n, &pg, &i);
    lb = fmg_field(pg, i)->lb;
    *tag = FA_PG_TAG[pg];
    str_cpy(name, lb, 16);
    if (pg == 1u)
        if (i < 3u)
            str_cpy(name + str_len(name), " RANGE", 7);
        else if (i >= 4u && i < 7u)
            str_cpy(name + str_len(name), " WAVE", 7);
    else if (pg == 2u)
        str_cpy(name + str_len(name), i < 4u ? " LEVEL" : " ON", 7);
    else if (pg == 0u && i == 1u)
        str_cpy(name, "GLIDE TIME", 16);
    else if (pg == 0u && i == 6u)
        str_cpy(name, "GLIDE ON", 16);
    else if (pg == 6u && i >= 4u)
        str_cpy(name, i == 4u ? "REVERB DECAY" : i == 5u ? "DELAY TIME" : i == 6u ? "CHORUS RATE" : "DRIVE TONE", 16);
}

/* a CC value v (0..127) for control n */
static void fa_ctl_apply(uint32_t n, uint32_t v)
{
    uint32_t pg, i;
    const fmgf_t *f;
    track_t *t = TSEL;
    if (n >= FA_NPG) {
        n -= FA_NPG;
        if (n < 5u)
            midi_cc_set(t, FA_AR_ID[n], v);
        else
            song.g[G_BPM] = (int16_t)(40 + (int32_t)v * 200 / 127);
            fmg_tempo_sync();
        return;
    }
    fa_ctl_loc(n, &pg, &i);
    if (pg == 6u && i >= 4u) {                            /* the FX page's slots 4..7 kept their CCs: DECAY TIME RATE TONE, now in the pop-ups */
        static const uint8_t POPI[4] = {2, 2, 3, 2};
        pg = 6u + (i - 3u);
        i = POPI[i - 4u];
    }
    f = fmg_field(pg, i);
    switch (f->kind) {
    case FK_PAR:
        midi_cc_set(t, f->a, v);
        break;
    case FK_NAME:
        fmg_write(f->a, v * 6u / 128u);
        break;
    case FK_GLIDE:
        fmg_write(f->a, v);
        {
            uint32_t sw2 = fmg_byte(FG_SW2), want = v ? (sw2 | FT_GLIDE) : (sw2 & ~(uint32_t)FT_GLIDE);
            if (want != sw2)
                fmg_write(FG_SW2, want);
        }
        fmg_glide_sync();
        break;
    case FK_BYTE:
        if (f->a == FG_F2 || f->a == FG_F3)
            fmg_det_cc(f->a, v);
        else
            fmg_write(f->a, v);
        break;
    case FK_GLB: {
        const param_desc_t *d = &GP[f->a];
        int32_t n = d->max - d->min + 1;
        song.g[f->a] = (int16_t)(d->min + (d->fmt == F_ENUM ? (int32_t)v * n / 128 : ((int32_t)v * (n - 1) + 63) / 127));
        fmg_gdirty = 1;
        break;
    }
    default:                                              /* a switch: on from 64 */
        fmg_turn(pg, i, v >= 64u ? 1 : -1, EN_ALGO);
        break;
    }
}
/* the CCs the ISR has noted: applied here (the main loop), or, in the MIDI CC list, taken for the highlighted line */
static void fa_cc_poll(void)
{
    uint32_t w, b, pend[4];
    fm1_irq_off();
    for (w = 0; w < 4u; w++) {
        pend[w] = fm_cc_pend[w];
        fm_cc_pend[w] = 0;
    }
    fm1_irq_on();
    for (w = 0; w < 4u; w++)
        for (b = 0; b < 32u && pend[w] >> b; b++)          /* (b < 32: a shift by 32 is undefined, and on a target that masks the count it
                                                            * never ended for CC 31, 63, 95 and 127: fm_cc_rx was read past its 128 bytes) */
            if ((pend[w] >> b) & 1u) {
                uint32_t cc = w * 32u + b, n, v = fm_cc_rx[cc];
                if (fa_gl == 2u) {                         /* the list: no control moves; MIDI LEARN: the CC is the highlighted line's */
                    if (ml_free_cc(cc)) {
                        for (n = 0; n < FA_NCC; n++)       /* (a CC has one control: the line that had it is OFF) */
                            if (fa_cc[n] == cc)
                                fa_cc[n] = 0;
                        fa_cc[fa_cc_i] = (uint8_t)cc;
                        fa_cfg_dirty = 1;
                    }
                    continue;
                }
                for (n = 0; n < FA_NCC; n++)
                    if (fa_cc[n] == cc) {
                        fmg_force_page = (int8_t)fa_page;
                        fa_ctl_apply(n, v);
                    }
            }
}

/* MIDI CLOCK: the ARP and the SEQ follow the clock of the chosen input (USB or TRS; OFF: the internal clock). It is the
 * global CLK of Felucca (0 INT, 1 USB, 2 TRS): the engine takes Start, Stop and Continue from that input only */
static void fa_clk_apply(void)
{
    song.g[G_CLOCK] = (int16_t)fa_clk;
}
static void fa_gl_dialog(void)
{
    char b[24];
    if (fa_gl == 1u) {
        fa_frame("GLOBAL");
        fa_line(62, "MIDI IN", FA_SRC[fm_midi_src == 3u ? 2u : fm_midi_src & 1u], fa_gl_row == 0u);
        if (fm_midi_ch)
            fmt_int(b, fm_midi_ch);
        else
            str_cpy(b, "ALL", 4);
        fa_line(84, "MIDI CHANNEL", b, fa_gl_row == 1u);
        fa_line(106, "MIDI CC", ">", fa_gl_row == 2u);
        fa_line(128, "MIDI CLOCK", FA_CLK[fa_clk], fa_gl_row == 3u);
        fa_line(150, "PITCH BEND", FA_BEND[fm_bend_mode & 3u], fa_gl_row == 4u);
        fa_line(172, "ABOUT", ">", fa_gl_row == 5u);
        fa_ctext(120, 198, &AF_S, "SELECT  ALGO  GLO=BACK", FA_MU);
    } else {                                              /* MIDI CC: a window of six lines on the list, a scroll bar */
        uint32_t top = fa_cc_i < 3u ? 0u : fa_cc_i - 3u, k;
        if (top + 6u > FA_NCC)
            top = FA_NCC - 6u;
        fa_frame("MIDI CC");
        for (k = 0; k < 6u; k++) {
            uint32_t n = top + k;
            const char *tag;
            char nm[16], cc[8];
            int32_t y = 58 + (int32_t)k * 21;
            int hot = n == fa_cc_i;
            fa_ctl_name(n, &tag, nm);
            if (fa_cc[n])
                fmt_int(cc, fa_cc[n]);
            else
                str_cpy(cc, "OFF", 4);
            if (hot)
                cv_rect(16, y - 4, 204, 20, FA_RGB(0x2E, 0x28, 0x22));
            fa_ltext(20, y + 3, &AF_X, tag, FA_MU);
            fa_ltext(54, y, &AF_M, nm, hot ? FA_OR : FA_CR);
            fa_rtext(214, y, &AF_M, cc, hot ? FA_OR : FA_CR);
        }
        {   /* the scroll bar: the track of the list, the thumb the six lines in view */
            const int32_t ty = 54, th = 130;
            int32_t thumb = (int32_t)(6u * (uint32_t)th / FA_NCC), at;
            if (thumb < 12)
                thumb = 12;
            at = (FA_NCC > 6u) ? (int32_t)top * (th - thumb) / (int32_t)(FA_NCC - 6u) : 0;
            cv_rect(223, ty, 3, th, FA_RGB(0x2A, 0x25, 0x20));
            cv_rect(223, ty + at, 3, thumb, FA_CR);
        }
        fa_ctext(120, 190, &AF_S, "MIDI LEARN: SEND A CC   ALGO = CC", FA_OR);
    }
}

/* the patch shown: a factory one by its number 000..199 and name, or a saved one (U01..) ; the last factory patch loaded
 * stays shown while its sound is edited (SLOT OWN), so there is always a number */
static int16_t fa_last_sl = -2;                           /* SLOT as last seen: -1 = not a factory patch (edited / saved one) */
static uint8_t fa_primed;                                 /* the first frame has been seen: no pop-up at power-on */
static uint32_t fa_pp_until;                              /* the preset pop-up shows until fm1_ms passes this */
static void fa_preset_label(char *num, char *name)
{
    uint32_t k = fa_cur_slot();
    fa_slot_num(num, k);
    str_cpy(name, ENGINES[ENGI_FMINI]->presets[k].name, 16);
}
static int fa_pp_on(void) { return fa_pp_until && (int32_t)(fm1_ms - fa_pp_until) < 0; }
/* the pop-up of a preset change: number and name large, 1.5 s */
static void fa_pp_draw(void)
{
    char num[8], nm[16];
    const aafont_t *f = &AF_L;
    fa_preset_label(num, nm);
    if (text_w(&AF_L, nm) > 204)
        f = &AF_M;
    cv_rect(12, 70, 216, 100, FA_OR);
    cv_rect(14, 72, 212, 96, FA_RGB(0x1B, 0x19, 0x17));
    fa_ctext(120, 84, &AF_L, num, FA_OR);
    fa_ctext(120, 126, f, nm, FA_CR);
}
/* every frame: a new preset (PRESETS, a Program Change, a saved one loaded) is shown in the pop-up; 1 = it changed now
 * (the caller goes HOME) */
static uint32_t fa_track_preset(void)
{
    const track_t *t = TSEL;
    int32_t sl = t->p[P_E7];
    uint32_t u = fa_on_user() ? t->user : 0u, ch = 0;
    if (sl >= 0 && sl < (int32_t)FMINI_NSLOT) {
        if (sl != fa_last_sl) {
            fa_base = (uint8_t)sl;
            fa_last_sl = (int16_t)sl;
            if (fa_primed) {
                fa_pp_until = fm1_ms + 1500u;
                ch = 1;
            }
        }
    } else
        fa_last_sl = -1;
    if (u != fa_last_user) {
        fa_last_user = (uint8_t)u;
        if (u && fa_primed) {
            fa_pp_until = fm1_ms + 1500u;
            ch = 1;
        }
    }
    return ch;
}

/* The battery, bottom right (1.8.0): a horizontal icon of four bars. The level comes from the ADC thresholds the firmware always had
 * (531 / 561 / 591: ui_draw.c batt_level), so 4 bars from 591, 3 from 561, 2 from 531 and 1 below; that last one is red (FA_BATT_LOW,
 * the stock "empty": calibrate it against song.batt_raw, which the console prints as batt_raw). USB power has no charger line to
 * read (ui_draw.c batt_shown): a host that has configured us and is not suspended counts as charging, and a bolt crosses the icon.
 * Not yet read (raw 0: the emulator, the first blocks): full. Bits 0..2 the bars, bit 3 low, bit 4 charging. */
#define FA_BATT_LOW 531
static uint32_t fa_batt(void)
{
    int32_t r = song.batt_raw;
    uint32_t bars = r == 0 || r >= 591 ? 4u : r >= 561 ? 3u : r >= FA_BATT_LOW ? 2u : 1u;
    return bars | (uint32_t)(bars == 1u) << 3 | (uint32_t)(usb.config && !usb.suspended) << 4;
}
/* xr: the right edge of the nub, y: the top of the 10 px body */
static void fa_battery(int32_t xr, int32_t y)
{
    uint32_t st = fa_batt(), bars = st & 7u, i;
    uint16_t bg = fa_bg(y + 4), ink = st & 8u ? FA_RED : FA_RGB(0xD9, 0xD2, 0xC0), lit = st & 8u ? FA_RED : FA_CR;
    int32_t x = xr - 25;
    cv_rect(x, y, 23, 10, ink);                              /* the outline: a 23 x 10 body and a 2 x 4 nub */
    cv_rect(x + 1, y + 1, 21, 8, bg);
    cv_rect(x + 23, y + 3, 2, 4, ink);
    for (i = 0; i < bars; i++)
        cv_rect(x + 2 + (int32_t)i * 5, y + 2, 4, 6, lit);
    if (st & 16u) {                                          /* the bolt over the bars: rows of {y, x0, x1} in the body, a dark edge, then an orange core */
        static const uint8_t R[7][3] = {{1, 12, 13}, {2, 11, 12}, {3, 10, 11}, {4, 9, 12}, {5, 10, 11}, {6, 9, 10}, {7, 8, 9}};
        for (i = 0; i < 7u; i++)
            cv_rect(x + (int32_t)R[i][1] - 1, y + (int32_t)R[i][0], (int32_t)(R[i][2] - R[i][1]) + 3, 1, FA_RGB(0x0E, 0x0D, 0x0C));
        for (i = 0; i < 7u; i++)
            cv_rect(x + (int32_t)R[i][1], y + (int32_t)R[i][0], (int32_t)(R[i][2] - R[i][1]) + 1, 1, FA_OR);
    }
}

static void fa_scene(void)
{
    uint32_t pg = fa_page, i, cur = fmg_curi(pg), ex = fmg_count(pg) > 4u ? fmg_count(pg) - 4u : 0u;
    char b[24];
    if (pg == 0u && ex > 4u)                                 /* CTRL: the fifth is VOICES, drawn apart */
        ex = 4u;
    int32_t x;
    cv_rect(8, 0, 224, 240, fa_bg(0));                       /* the panel: the gradient in 20 bands */
    for (i = 0; i < 20u; i++)
        cv_rect(8, (int32_t)i * 12, 224, 12, fa_bg((int32_t)i * 12 + 6));
    for (x = 0; x < 8; x++) {                                /* the wood sides: a horizontal gradient and the grain */
        uint16_t w = x < 4 ? fa_mix(FA_RGB(0x4E, 0x2B, 0x14), FA_RGB(0x8A, 0x54, 0x30), (uint32_t)x * 64u)
                           : fa_mix(FA_RGB(0x8A, 0x54, 0x30), FA_RGB(0x5C, 0x33, 0x19), (uint32_t)(x - 4) * 64u);
        cv_rect(x, 0, 1, 240, w);
        cv_rect(232 + x, 0, 1, 240, w);
    }
    for (i = 1; i < 240u; i += 7u) {
        cv_rect(0, (int32_t)i, 8, 1, FA_RGB(0x3A, 0x1C, 0x0B));
        cv_rect(232, (int32_t)i, 8, 1, FA_RGB(0x3A, 0x1C, 0x0B));
        cv_rect(0, (int32_t)i + 3, 8, 1, FA_RGB(0x42, 0x20, 0x0D));
        cv_rect(232, (int32_t)i + 3, 8, 1, FA_RGB(0x42, 0x20, 0x0D));
    }
    cv_rect(8, 0, 224, 22, FA_RGB(0x0E, 0x0D, 0x0C));        /* the header: the name and the seven tabs */
    {                                                    /* the logo as on the splash: FM and 1 orange, ini- cream */
        int32_t lx = 12;
        cv_text_on(lx, 3, &AF_S, "FM", FA_OR, FA_RGB(0x0E, 0x0D, 0x0C));
        lx += text_w(&AF_S, "FM");
        cv_text_on(lx, 3, &AF_S, "ini-", FA_CR, FA_RGB(0x0E, 0x0D, 0x0C));
        lx += text_w(&AF_S, "ini-");
        cv_text_on(lx, 3, &AF_S, "1", FA_OR, FA_RGB(0x0E, 0x0D, 0x0C));
    }
    {
        static const uint8_t TW[FA_NPAGE] = {24, 22, 20, 22, 30, 30, 18};
        int32_t tx = 60;
        for (i = 0; i < FA_NPAGE; i++) {
            uint16_t bgc = i == pg ? FA_OR : FA_RGB(0x1E, 0x1B, 0x19);
            cv_rect(tx, 2, TW[i] - 1, 18, bgc);
            cv_text_on(tx + (TW[i] - 1) / 2 - text_w(&AF_X, FA_TAB[i]) / 2, 11 - AF_X_CAP_Y - AF_X_CAP_H / 2 + 1, &AF_X,
                       FA_TAB[i], i == pg ? FA_RGB(0x1A, 0x0F, 0x06) : FA_MU, bgc);
            tx += TW[i];
        }
    }
    cv_rect(14, 30, 212, 1, FA_RULE);                         /* the section title on its rule */
    {
        int32_t w = text_w(&AF_S, FMG_PAGE[pg].name) + 14;
        cv_rect(120 - w / 2, 24, w, 13, FA_RGB(0x1B, 0x19, 0x17));
        cv_text_on(120 - text_w(&AF_S, FMG_PAGE[pg].name) / 2, 24 - AF_S_CAP_Y + 2, &AF_S, FMG_PAGE[pg].name, FA_CR,
                   FA_RGB(0x1B, 0x19, 0x17));
    }
    for (i = 0; i < 4u; i++)
        fa_knob_col(pg, i, 36 + (int32_t)i * 56, cur == i);
    if (pg == 1u) {
        fa_wave_boxes(pg, cur);
    } else if (pg == 3u || pg == 4u || pg == 5u) {
        fa_graph(pg);
    } else if (pg == 6u) {                                   /* FX: DECAY TEMPO RATE TONE under their sends, a thin bar between the four sections */
        for (i = 0; i < 4u; i++)
            fa_edit_btn(36 + (int32_t)i * 56, cur == 4u + i);
        for (i = 1; i < 4u; i++) {                         /* the bars between the sections: a bright core with a faint glow */
            cv_rect(7 + (int32_t)i * 56, 38, 3, 168, FA_RGB(0x4A, 0x40, 0x32));
            cv_rect(8 + (int32_t)i * 56, 38, 1, 168, FA_RGB(0xD2, 0xC2, 0xA2));
        }
    } else {
        for (i = 0; i < ex; i++)
            fa_switch(pg, i, 36 + (int32_t)i * 56, cur == 4u + i);
        if (pg == 0u)
            fa_voices(cur == 8u);
    }
    {   /* the foot: the patch's number (000..199, or U01.. of a saved one) and name, the battery at the right */
        int32_t w;
        char nm[16];
        fa_preset_label(b, nm);
        w = text_w(&AF_N, b);
        fa_ltext(14, 214, &AF_N, b, FA_OR);
        fa_ltext(14 + w + 6, 214, &AF_N, nm, FA_CR);
        fa_battery(226, 227);
    }
    if (fb_rs_state() == FB_RS_ASK || fb_rs == FB_RS_OK)
        fa_restore_dialog();                              /* the restore question / its progress: over everything */
    else if (fa_gl == 1u || fa_gl == 2u)
        fa_gl_dialog();
    else if (fa_sc)
        fa_sc_dialog();
    else if (fa_sq)
        fa_seq_dialog();
    else if (fa_ar)
        fa_ar_dialog();
    else if (fa_sv)
        fa_sv_conf ? fa_confirm_dialog() : fa_dialog();
    else if (fmg_fx)
        fa_fx_dialog();
    else if (fa_msg_on()) {
        int32_t mw = text_w(&AF_M, fa_msg) + 20;
        cv_rect(120 - mw / 2, 96, mw, 26, FA_OR);
        cv_rect(120 - mw / 2 + 2, 98, mw - 4, 22, FA_RGB(0x1B, 0x19, 0x17));
        fa_ctext(120, 102, &AF_M, fa_msg, FA_OR);
    }
    if (fb_rs_state() == FB_RS_ASK || fb_rs == FB_RS_OK)
        return;                                          /* the restore question is up: no other pop-up over it */
    if (fa_tp)
        fa_tp_draw();
    if (fa_pp_on())
        fa_pp_draw();
}

static uint32_t fa_signature(void)
{
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    h = (h ^ fa_page ^ (uint32_t)fb_rs_state() << 8 ^ (uint32_t)fb_rs_yes << 12) * 16777619u;
    for (i = 0; i < FMG_NPG; i++)
        h = (h ^ fmg_cur[i]) * 16777619u;
    for (i = 0; i < FG_SIZE; i++)
        h = (h ^ fmini_patch[0][i]) * 16777619u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h = (h ^ fa_base ^ (uint32_t)fa_pp_on() << 8) * 16777619u;
    h = (h ^ fa_gl ^ (uint32_t)fa_gl_row << 3 ^ (uint32_t)fa_cc_i << 6 ^ (uint32_t)fm_midi_src << 12 ^ (uint32_t)fm_bt << 17 ^ (uint32_t)fm_midi_ch << 14 ^ (uint32_t)fm_bend_mode << 19) * 16777619u;
    for (i = 0; i < FA_NCC; i++)
        h = (h ^ fa_cc[i]) * 16777619u;
    h = (h ^ fa_tp ^ (uint32_t)fa_clk << 1) * 16777619u;
    h = (h ^ fa_sv ^ (uint32_t)fa_ar << 4 ^ (uint32_t)fa_ar_row << 5 ^ (uint32_t)song.g[G_BPM] << 8) * 16777619u;
    h = (h ^ (uint32_t)song.g[G_RSIZE] ^ (uint32_t)song.g[G_DRREV] << 7 ^ (uint32_t)song.g[G_CRATE] << 14 ^ (uint32_t)song.g[G_DRLVL] << 21) * 16777619u;   /* FX: DECAY TIME RATE TONE */
    for (i = 0; i < 8u; i++)                                 /* the pop-ups' others, and which pop-up is open */
        h = (h ^ (uint32_t)song.g[FA_GID2[i]]) * 16777619u;
    h = (h ^ fmg_fx) * 16777619u;
    h = (h ^ fa_sv_slot ^ (uint32_t)fa_sv_pos << 8 ^ (uint32_t)(fa_sv_conf | fa_sv_yes << 1) << 12 ^ (uint32_t)t->user << 16 ^ (uint32_t)fa_msg_on() << 24) * 16777619u;
    for (i = 0; i < 12u; i++)
        h = (h ^ (uint8_t)fa_sv_name[i]) * 16777619u;
    h = (h ^ fa_sq ^ (uint32_t)fa_sq_rec << 1 ^ (uint32_t)fa_sq_cur << 2 ^ (uint32_t)song.playing << 8 ^ (uint32_t)fa_sq_title_on() << 7 ^
         (uint32_t)(song.playing ? t->seq_idx : 0u) << 9 ^ (uint32_t)fa_sq_blink() << 16 ^ (uint32_t)fa_sc << 20 ^ (uint32_t)fa_sc_row << 21) * 16777619u;
    for (i = 0; i < NSTEP; i++)
        h = (h ^ (uint32_t)(t->step[i].n ? t->step[i].note[0] : 0u)) * 16777619u;
    h = (h ^ fa_batt()) * 16777619u;                         /* the battery icon */
    return (h ^ t->preset) * 16777619u;
}

static void fa_splash(void);
static void fa_draw(void)
{
    uint32_t sig = fa_signature(), band;
    if (sig == fa_sig_prev && !ui.force)
        return;
    fa_sig_prev = sig;
    ui.force = 0;
    fmg_force_page = (int8_t)fa_page;
    if (fa_gl == 3u) {                                    /* ABOUT: the splash, the version bottom right */
        fa_splash();
        return;
    }
    for (band = 0; band < 2u; band++) {
        cv_begin(240, 120, FA_RGB(0x17, 0x15, 0x13));
        cv_oy = -(int32_t)band * 120;
        cv_cy0 = 0;
        fa_scene();
        cv_oy = 0;
        cv_blit(0, band * 120u);
    }
}

#include "fmini_splash.h"
static void fa_splash(void)                              /* the picture (assets/fmini-splash.png), the version bottom right: power-on (3 s, main.c) and ABOUT */
{
    uint32_t band, x, y;
    for (band = 0; band < 2u; band++) {
        cv_begin(240, 120, 0);
        cv_oy = -(int32_t)band * 120;
        for (y = band * 120u; y < band * 120u + 120u; y++)
            for (x = 0; x < 240u; x++)
                cv_pset((int32_t)x, (int32_t)y, FMINI_SPLASH_PAL[FMINI_SPLASH[y * 240u + x]]);
        cv_text_on(232 - text_w(&AF_S, FMINI_VERSION), 220, &AF_S, FMINI_VERSION, FA_CR, 0);
        cv_oy = 0;
        cv_blit(0, band * 120u);
    }
}

/* ---------------------------------------------------------- control --- */
static void fa_goto(uint32_t pg)
{
    fa_page = (uint8_t)(pg % FA_NPAGE);
    fa_cfg_save();                                        /* (the FX page's globals) */
}
/* HOME: the first screen, CTRL; the pop-ups close. A preset change does the same (fa_input) */
static void fa_home(void)
{
    fa_sv = 0;
    fa_ar = 0;
    fa_sc = 0;
    fa_sq = 0;
    fa_sq_rec = 0;
    fa_gl_close();
    fa_goto(0);
}

/* SELECT: one control on; past a page's last control the next page's first (the other way: the last) */
static void fa_select(int32_t s)
{
    uint32_t pg = fa_page, n = fmg_count(pg);
    int32_t c = (int32_t)fmg_curi(pg) + (s > 0 ? 1 : -1);
    if (c < 0) {
        fa_page = (uint8_t)((pg + FA_NPAGE - 1u) % FA_NPAGE);
        fmg_cur[fa_page] = (uint8_t)(fmg_count(fa_page) - 1u);
    } else if (c >= (int32_t)n) {
        fa_page = (uint8_t)((pg + 1u) % FA_NPAGE);
        fmg_cur[fa_page] = 0;
    } else {
        fmg_cur[pg] = (uint8_t)c;
    }
}

/* a factory patch n was just loaded into track t: what its preset_t does not carry (eng_fmini.c fmini_x_t): the arpeggiator
 * (on or off, with the MODE the ARP button turns on), the LFO and the common modulation, the FX settings of the pop-ups and the keys' octave.
 * The tempo stays the player's. Also the Program Change's way in (ui.c midi_pc_load) */
static const uint8_t FA_XG[12] = {G_RTYPE, G_RSIZE, G_RDAMP, G_DTIME, G_DRREV, G_DFDBK, G_ENGGO, G_CDEPTH, G_CRATE, G_INFO, G_DRLVL, G_MIDI};
static void fa_factory_extras(track_t *t, uint32_t n)
{
    fmini_x_t xs, *x = &xs;
    uint32_t i;
    if (n >= FMINI_NSLOT)
        return;
    fb_extra(n, x);                             /* the slot's record (saved or factory) */
    t->p[P_AMODE] = x->arp[0] ? x->arp[1] : 0;
    t->p[P_ARATE] = x->arp[2];
    t->p[P_AOCT] = x->arp[3];
    t->p[P_AGATE] = x->arp[4];
    t->p[P_ASWING] = x->arp[5];
    t->p[P_APROB] = x->arp[6];
    t->p[P_AHOLD] = 0;
    t->p[P_AORDER] = 0;
    t->p[P_LEVEL] = x->level;
    t->p[P_LRATE] = x->lfo[0];
    t->p[P_LWAVE] = x->lfo[1];
    t->p[P_LFADE] = x->lfo[2];
    t->p[P_LSYNC] = x->lfo[3];
    t->p[P_LD_PIT] = x->mod[0];
    t->p[P_LD_AMP] = x->mod[1];
    t->p[P_ED_PIT] = x->mod[2];
    if (t != &trk[0])
        return;
    for (i = 0; i < 12u; i++)
        song.g[FA_XG[i]] = (int16_t)clamp(x->fx[i], GP[FA_XG[i]].min, GP[FA_XG[i]].max);
    song.octave = x->oct;
    fa_ar_last = x->arp[1] ? x->arp[1] : 1u;
}

/* SAVE: the track's sound (its patch, the knobs, the sends, the arpeggiator, LFO, FX, octave) as slot k's record, with
 * the name; the steps are not in a preset. 0 = saved (the RAM list shows it at once) */
static int fa_save_slot(uint32_t k, const char *name)
{
    static const uint8_t ENV[4] = {P_ATK, P_DEC, P_SUS, P_REL};
    track_t *t = TSEL;
    uint32_t tr = (uint32_t)(t - trk), n = 0, i, nl = 0;
    uint8_t rec[FB_SLOT], patch[FG_SIZE], par[18], nb[12];
    fmini_x_t x;
    if (k >= FMINI_NSLOT)
        return -1;
    memset(&x, 0, sizeof x);
    x.arp[0] = t->p[P_AMODE] != 0;
    x.arp[1] = (uint8_t)t->p[P_AMODE];
    x.arp[2] = (uint8_t)t->p[P_ARATE];
    x.arp[3] = (uint8_t)t->p[P_AOCT];
    x.arp[4] = (uint8_t)t->p[P_AGATE];
    x.arp[5] = (uint8_t)t->p[P_ASWING];
    x.arp[6] = (uint8_t)t->p[P_APROB];
    x.lfo[0] = (uint8_t)t->p[P_LRATE];
    x.lfo[1] = (uint8_t)t->p[P_LWAVE];
    x.lfo[2] = (uint8_t)t->p[P_LFADE];
    x.lfo[3] = (uint8_t)t->p[P_LSYNC];
    x.level = (uint8_t)t->p[P_LEVEL];
    x.mod[0] = (int8_t)t->p[P_LD_PIT];
    x.mod[1] = (int8_t)t->p[P_LD_AMP];
    x.mod[2] = (int8_t)t->p[P_ED_PIT];
    for (i = 0; i < 12u; i++)
        x.fx[i] = (uint8_t)clamp(song.g[FA_XG[i]], 0, 255);
    x.oct = (int8_t)song.octave;
    for (i = 0; i < 8u; i++)
        par[i] = (uint8_t)(int8_t)t->p[P_E0 + i];
    for (i = 0; i < 4u; i++)
        par[8 + i] = (uint8_t)t->p[ENV[i]];
    par[12] = (uint8_t)(int8_t)t->p[P_ED_FLT];
    par[13] = t->p[P_VOICE] == V_LEGATO ? 1u : 0u;
    for (i = 0; i < 4u; i++)
        par[14 + i] = (uint8_t)(clamp(t->p[P_DIST + i], 0, 127) + 1);
    memcpy(patch, fmini_patch[tr], FG_SIZE);
    while (nl < 12u && name[nl]) {
        nb[nl] = (uint8_t)name[nl];
        nl++;
    }
    n = fr_put(rec, FB_SLOT, n, FR_NAME, nb, nl);
    n = fr_put(rec, FB_SLOT, n, FR_PATCH, patch, FG_SIZE);
    n = fr_put(rec, FB_SLOT, n, FR_EXTRA, (const uint8_t *)&x, sizeof x);
    n = fr_put(rec, FB_SLOT, n, FR_PARAM, par, sizeof par);
    if (!n)
        return -1;
    return fb_write(k, rec, n);                       /* flash (fmini_bank.c), or the host's RAM copy */
}

/* PRESETS: the previous / next patch of the list (factory F1..F8, then the saved ones), round; in the SAVE dialog the slot */
static void fa_preset(int32_t s)
{
    track_t *t = TSEL;
    uint32_t cur, nx;
    if (fa_sv) {
        fa_sv_slot = (uint8_t)((fa_sv_slot + FMINI_NSLOT + (s > 0 ? 1u : FMINI_NSLOT - 1u)) % FMINI_NSLOT);
        fa_sv_fill();
        return;
    }
    /* every slot is a preset now (0..199, the same way): the one shown, or the patch the track came from (OWN) */
    cur = t->p[P_E7] >= 0 && t->p[P_E7] < (int32_t)FMINI_NSLOT ? (uint32_t)t->p[P_E7] : fa_base % FMINI_NSLOT;
    {   /* one preset per detent, however fast they came (s: the detents since the last frame, signed); round */
        int32_t i = ((int32_t)cur + s) % (int32_t)FMINI_NSLOT;
        nx = (uint32_t)(i < 0 ? i + (int32_t)FMINI_NSLOT : i);
    }
    t->user = 0;
    apply_preset_to(t, nx);
    t->p[P_E7] = (int16_t)nx;
    fa_factory_extras(t, nx);                         /* its arpeggiator, LFO, FX settings and octave */
    fmini_poll();
}

/* a page button: the tap opens it */
/* the delay pop-up's TAP TEMPO (the SCL button): the time between two taps is the delay TIME (taps further apart than 1.4 s start again) */
static uint32_t fa_tap_t;
static void fa_tap(void)
{
    uint32_t now = fm1_ms, dt = now - fa_tap_t;
    fa_tap_t = now;
    if (dt >= 100u && dt <= 1400u) {
        int32_t v, best = 0, bd = 1 << 30;
        for (v = 0; v < 128; v++) {                       /* the TIME knob value whose milliseconds are the nearest */
            int32_t d = dly_ms(v) - (int32_t)dt;
            d = d < 0 ? -d : d;
            if (d < bd) {
                bd = d;
                best = v;
            }
        }
        song.g[G_DRREV] = (int16_t)best;
        fmg_gdirty = 1;
    }
}

static void fa_buttons(uint32_t pressed)
{
    static const uint8_t PG_BTN[5] = {B_LFO, B_EDIT, B_SCL, B_FX, B_ENV};   /* (GLO: global, ARP, SEQ, REC, PLAY: their own) */
    uint32_t i;
    if (fa_tp && pressed) {                               /* TAP TEMPO: EDIT taps; any other button closes it, and does nothing else */
        if (pressed == 1u << panel.btn[B_EDIT])
            fa_tp_tap();
        else
            fa_tp = 0;
        return;
    }
    if ((pressed >> panel.btn[B_HOME]) & 1u) {            /* HOME: the first screen, CTRL; the pop-ups close */
        fa_home();
        return;
    }
    if ((pressed >> panel.btn[B_GLO]) & 1u) {             /* GLO: the global pop-up; inside a list, one level back */
        fa_sv = 0;
        fa_ar = 0;
        fa_sc = 0;
        fa_sq = 0;
        fa_sq_rec = 0;
        if (!fa_gl)
            fa_gl = 1;
        else if (fa_gl > 1u)
            fa_gl = 1;
        else
            fa_gl_close();
        return;
    }
    {   /* ARP: a tap turns the arpeggiator on / off (the button lights); held, HOLD on / off. The pop-up shows it.
         * With the pop-up closed and the arpeggiator on, a tap only opens the pop-up: the arpeggio stays on */
        uint32_t down = (fm1_in.buttons >> panel.btn[B_ARP]) & 1u;
        if ((pressed >> panel.btn[B_ARP]) & 1u) {
            fa_arp_down = 1;
            fa_arp_done = 0;
            fa_arp_t0 = fm1_ms;
        }
        if (fa_arp_down) {
            if (!down) {
                fa_arp_down = 0;
                if (!fa_arp_done) {                           /* the tap */
                    track_t *t = TSEL;
                    if (!(t->p[P_AMODE] && !fa_ar)) {         /* (not: on, and off the pop-up: that tap only opens it) */
                        if (t->p[P_AMODE]) {
                            fa_ar_last = (uint8_t)t->p[P_AMODE];
                            t->p[P_AMODE] = 0;
                        } else {
                            t->p[P_AMODE] = fa_ar_last ? fa_ar_last : 1;
                        }
                    }
                    fa_arp_show();
                }
            } else if (!fa_arp_done && (uint32_t)(fm1_ms - fa_arp_t0) >= 450u) {   /* the hold */
                fa_arp_done = 1;
                TSEL->p[P_AHOLD] = (int16_t)!TSEL->p[P_AHOLD];
                fa_arp_show();
            }
        }
    }
    {   /* REC: a tap arms / disarms it (it lights; the next notes go into the SEQ slots). Held (HOLD REC TO CLEAR) empties
         * the sequence, and the release is then no tap */
        uint32_t down = (fm1_in.buttons >> panel.btn[B_REC]) & 1u;
        if ((pressed >> panel.btn[B_REC]) & 1u) {
            fa_rec_down = 1;
            fa_rec_done = 0;
            fa_rec_t0 = fm1_ms;
        }
        if (fa_rec_down) {
            if (!down) {
                fa_rec_down = 0;
                if (!fa_rec_done) {                           /* the tap */
                    fa_sq_rec = (uint8_t)!fa_sq_rec;
                    if (fa_sq_rec) {
                        fa_sq_cur = 0;
                        fa_sq_fresh = 1;
                        fa_sv = 0;
                        fa_ar = 0;
                        fa_sc = 0;
                        fa_gl_close();
                        fa_sq = 1;
                    }
                }
            } else if (!fa_rec_done && (uint32_t)(fm1_ms - fa_rec_t0) >= 700u) {   /* HOLD REC TO CLEAR */
                fa_rec_done = 1;
                fa_seq_clear();
            }
        }
    }
    if ((pressed >> panel.btn[B_PLAY]) & 1u)              /* PLAY / STOP the sequence */
        transport_req = (song.playing || transport_req == 1u) ? 2u : 1u;
    {   /* SEQ. Outside the sequencer, a press (not the release) opens its grid, with the word SEQUENCER for 0.5 s, and
         * holding does nothing more. Inside the grid: a tap (decided on release) closes it, and a hold (450 ms) opens the
         * settings pop-up (MODE, RATE, GATE, TEMPO). On the settings pop-up, a press goes back to the grid */
        uint32_t down = (fm1_in.buttons >> panel.btn[B_SEQ]) & 1u;
        if ((pressed >> panel.btn[B_SEQ]) & 1u) {
            fa_seq_down = 1;
            fa_seq_done = 1;                              /* (set below when a hold or a tap is still to come) */
            fa_seq_t0 = fm1_ms;
            if (fa_sc) {                                  /* the settings pop-up: back to the grid */
                fa_sc = 0;
                fa_sq = 1;
                fa_sq_title_until = 0;
            } else if (!fa_sq) {                          /* outside the sequencer: its grid opens now */
                fa_sv = 0;
                fa_ar = 0;
                fa_gl_close();
                fa_sq = 1;
                fa_sq_title_until = fm1_ms + 500u;
            } else {                                      /* inside the grid: a tap closes it, a hold opens the settings */
                fa_seq_done = 0;
            }
        }
        if (fa_seq_down) {
            if (!down) {
                fa_seq_down = 0;
                if (!fa_seq_done) {                       /* the tap inside the grid: it closes */
                    fa_sq = 0;
                    fa_sq_rec = 0;
                }
            } else if (!fa_seq_done && (uint32_t)(fm1_ms - fa_seq_t0) >= 450u) {   /* the hold inside the grid */
                fa_seq_done = 1;
                fa_sq = 0;
                fa_sq_rec = 0;
                fa_sc = 1;
                fa_sc_row = 0;
            }
        }
    }
    if ((pressed >> panel.btn[B_SAVE]) & 1u) {            /* SAVE: open the dialog; again: write */
        fa_ar = 0;
        fa_sc = 0;
        fa_sq = 0;
        fa_sq_rec = 0;
        fa_gl_close();
        if (fa_sv)
            fa_sv_press();
        else
            fa_sv_open();
        return;
    }
    if (fmg_fx == 2u && ((pressed >> panel.btn[B_SCL]) & 1u)) {   /* the delay pop-up: SCL taps the tempo, it does not leave */
        fa_tap();
        pressed &= ~(1u << panel.btn[B_SCL]);
    }
    for (i = 0; i < 5u; i++)
        if ((pressed >> panel.btn[PG_BTN[i]]) & 1u) {
            if (PG_BTN[i] == B_FX && fa_page == 6u) {     /* FX again: round the four pop-ups, then back to the page */
                fa_sv = 0;
                fa_ar = 0;
                fa_sc = 0;
                fa_sq = 0;
                fa_sq_rec = 0;
                fa_gl_close();
                fmg_fx = (uint8_t)((fmg_fx + 1u) % 5u);
                if (!fmg_fx)
                    fa_cfg_save();
                continue;
            }
            fmg_fx = 0;
            fa_sv = 0;                                    /* a page button cancels the dialog */
            fa_ar = 0;
            fa_sc = 0;
            fa_sq = 0;
            fa_sq_rec = 0;
            fa_gl_close();
            if (PG_BTN[i] == B_ENV) {
                fa_env_last = (uint8_t)(fa_page == 4u ? 5u : 4u);
                fa_goto(fa_env_last);
            } else {
                static const uint8_t TO[5] = {0, 3, 1, 6, 4};      /* LFO CTRL, EDIT FILT, SCL OSC (again: MIX), FX, ENV */
                fa_goto(PG_BTN[i] == B_SCL && fa_page == 1u ? 2u : TO[i]);
            }
        }
    if ((pressed >> panel.btn[B_OCTDN]) & 1u)
        song.octave = song.octave > -3 ? song.octave - 1 : song.octave;
    if ((pressed >> panel.btn[B_OCTUP]) & 1u)
        song.octave = song.octave < 3 ? song.octave + 1 : song.octave;
}

/* the instrument is MONO, like the one it follows (one voice: a new key retriggers the contours and glides when GLIDE
 * is on, the lowest key wins), or POLY (CTRL's VOICES: 12 voices); it is part of the preset (P_VOICE is in its record).
 * Whatever else a patch or a record says (LEGATO, UNISON, a factory patch's own) plays MONO */
static void fa_mono(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        if (trk[i].p[P_VOICE] != V_POLY)
            trk[i].p[P_VOICE] = V_MONO;
        trk[i].p[P_PRIO] = 1;
    }
}

/* a reset (fb_rs_apply) changes the preset's record in flash; the track that shows it keeps the old values in RAM,
 * so it is loaded again here (only the FMINI track on its own preset, or every reset of all of them) */
static void fa_reload_reset(void)
{
    track_t *t = TSEL;
    uint32_t cur;
    if (t->eng_req != ENGI_FMINI)
        return;
    cur = t->p[P_E7] >= 0 && t->p[P_E7] < (int32_t)FMINI_NSLOT ? (uint32_t)t->p[P_E7] : FMINI_NSLOT;
    if (cur >= FMINI_NSLOT || (fb_rs_k != FB_RS_ALL && cur != fb_rs_k))
        return;
    apply_preset_to(t, cur);
    t->p[P_E7] = (int16_t)cur;
    fa_factory_extras(t, cur);
}

static void fa_input(void)
{
    uint32_t pressed = fm1_input_edges(0), k;
    int32_t s;
    (void)fm1_input_note_edges();
    if (fb_rs_state() == FB_RS_ASK || fb_rs == FB_RS_OK) {  /* the restore question (or its progress): nothing else */
        if (fb_rs == FB_RS_ASK) {
            if ((s = panel_enc(EN_ALGO)) != 0)
                fb_rs_yes = s > 0;
            {   static const uint8_t RS_BTN[5] = {B_LFO, B_EDIT, B_SCL, B_FX, B_ENV};   /* the page buttons: NO */
                for (k = 0; k < 5u; k++)
                    if ((pressed >> panel.btn[RS_BTN[k]]) & 1u)
                        fb_rs = FB_RS_NO;
            }
            if ((pressed >> panel.btn[B_SAVE]) & 1u) {
                fb_rs = fb_rs_yes ? FB_RS_OK : FB_RS_NO;
                if (fb_rs == FB_RS_OK && fb_rs_k != FB_NSLOT) {                        /* a reset: done here, at YES */
                    if (fb_rs_apply() != 0)
                        fb_rs = FB_RS_NO;
                    else
                        fa_reload_reset();
                }
            }
            fb_rs_touch();
            ui.force = 1;
        }
        (void)panel_enc(EN_SELECT);
        (void)panel_enc(EN_PRESET);
        for (k = 0; k < 4u; k++)
            (void)panel_enc(EN_K1 + k);
        return;
    }
    midi_take();                                            /* MIDI IN notes */
    midi_pc_poll();                                         /* Program Change: a patch */
    fmini_poll();                                            /* SLOT turned: its patch */
    settings_later_poll();
    if (TSEL->eng_req != ENGI_FMINI) {                       /* an old autosave: the instrument is FMINI */
        set_engine_of(TSEL, ENGI_FMINI);
        apply_preset_to(TSEL, 0);
        fmini_poll();
    }
    song.sel = 0;
    fa_mono();
    fmg_glide_sync();                                       /* the patch's GLIDE switch and time -> the track's */
    fa_clk_apply();                                         /* MIDI CLOCK: the setting is the global CLK */
    fmg_force_page = (int8_t)fa_page;
    if (!fa_ar && !fa_sq && !fa_sc)
        fa_tp = 0;                                          /* the TAP TEMPO pop-up lives over the ARP or SEQ pop-up alone */
    fa_buttons(pressed);
    if ((s = panel_enc(EN_PRESET)) != 0 && !fa_gl)
        fa_preset(s);
    if (fa_track_preset())                                  /* a new preset (the knob, a Program Change, a saved one): HOME */
        fa_home();
    fa_cc_poll();                                         /* the CCs the MIDI inputs brought */
    if (fa_gl || fa_sq || fa_ar || fa_sv || fa_page != 6u)
        fmg_fx = 0;                                       /* the FX pop-up lives on the FX page alone, under no other dialog */
    if (fmg_fx) {                                         /* an FX pop-up: K1..K4 its four knobs, SELECT / ALGO as on a page */
        uint32_t pg = 6u + fmg_fx;
        fmg_force_page = (int8_t)pg;
        if ((s = panel_enc(EN_SELECT)) != 0)
            fmg_cur[pg] = (uint8_t)((fmg_cur[pg] + 4u + (s > 0 ? 1u : 3u)) % 4u);
        if ((s = panel_enc(EN_ALGO)) != 0)
            fmg_turn(pg, fmg_cur[pg] % 4u, s, EN_ALGO);
        for (k = 0; k < 4u; k++)
            if ((s = panel_enc(EN_K1 + k)) != 0) {
                fmg_turn(pg, k, s, EN_K1 + k);
                fmg_cur[pg] = (uint8_t)k;
            }
        return;
    }
    if (fa_gl) {                                          /* the GLO pop-up (the page's knobs wait) */
        for (k = 0; k < 4u; k++)
            (void)panel_enc(EN_K1 + k);
        (void)panel_enc(EN_PRESET);
        if (fa_gl == 1u) {
            if ((s = panel_enc(EN_SELECT)) != 0)
                fa_gl_row = (uint8_t)((fa_gl_row + 6u + (s > 0 ? 1u : 5u)) % 6u);
            if ((s = panel_enc(EN_ALGO)) != 0) {
                if (fa_gl_row == 0u) {
                    static const uint8_t SRC[3] = {0, 1, 3};
                    int32_t d = (fm_midi_src == 3u ? 2 : (int32_t)(fm_midi_src & 1u)) + (s > 0 ? 1 : -1);
                    fm_midi_src = SRC[clamp(d, 0, 2)];
                    panic_req |= 1u;                       /* a note of the source left must not hang */
                    fa_cfg_dirty = 1;
                } else if (fa_gl_row == 1u) {
                    fm_midi_ch = (uint8_t)clamp((int32_t)fm_midi_ch + (s > 0 ? 1 : -1), 0, 16);
                    panic_req |= 1u;
                    fa_cfg_dirty = 1;
                } else if (fa_gl_row == 2u) {
                    fa_gl = 2u;
                } else if (fa_gl_row == 3u) {               /* MIDI CLOCK: OFF, USB, TRS */
                    fa_clk = (uint8_t)((fa_clk + 3u + (s > 0 ? 1u : 2u)) % 3u);
                    fa_clk_apply();
                    panic_req |= 1u;                       /* the clock changes: no note may hang */
                    fa_cfg_dirty = 1;
                } else if (fa_gl_row == 4u) {
                    uint32_t ch;
                    fm_bend_mode = (uint8_t)clamp((int32_t)fm_bend_mode + (s > 0 ? 1 : -1), 0, 3);
                    for (ch = 0; ch < 16u; ch++)           /* the bend held now follows the new range */
                        midi_expression_channel(ch);
                    fa_cfg_dirty = 1;
                } else
                    fa_gl = 3u;                            /* ABOUT */
            }
        } else if (fa_gl == 2u) {
            if ((s = panel_enc(EN_SELECT)) != 0)
                fa_cc_i = (uint8_t)((fa_cc_i + FA_NCC + (s > 0 ? 1u : FA_NCC - 1u)) % FA_NCC);
            if ((s = panel_enc(EN_ALGO)) != 0) {
                int32_t c = fa_cc[fa_cc_i], n;
                for (n = 0; n < 121; n++) {                /* the next valid CC, round through 0 = none */
                    c = (c + 121 + (s > 0 ? 1 : 120)) % 121;
                    if (fa_cc_valid((uint32_t)c))
                        break;
                }
                fa_cc[fa_cc_i] = (uint8_t)c;
                fa_cfg_dirty = 1;
            }
        } else {                                            /* ABOUT: either encoder goes back */
            if (panel_enc(EN_SELECT) != 0 || panel_enc(EN_ALGO) != 0)
                fa_gl = 1;
        }
        return;
    }
    if (fa_tp) {                                          /* TAP TEMPO: ALGORITHM turns the tempo; SELECT or a knob closes it */
        int32_t close = 0;
        if ((s = panel_enc(EN_ALGO)) != 0)
            fa_tp_turn(s);
        if (panel_enc(EN_SELECT) != 0)
            close = 1;
        for (k = 0; k < 4u; k++)
            if (panel_enc(EN_K1 + k) != 0)
                close = 1;
        if (close)
            fa_tp = 0;
        return;
    }
    if (fa_sc) {                                          /* the SEQ settings pop-up (as the ARP's): SELECT picks a line, ALGORITHM turns it */
        if ((s = panel_enc(EN_SELECT)) != 0)
            fa_sc_row = (uint8_t)((fa_sc_row + FA_SC_N + (s > 0 ? 1u : FA_SC_N - 1u)) % FA_SC_N);
        if ((s = panel_enc(EN_ALGO)) != 0) {
            fa_sc_turn(fa_sc_row, s);
            if (fa_sc_row == 3u)                          /* TEMPO: the TAP TEMPO pop-up opens */
                fa_tp_begin();
        }
        for (k = 0; k < 3u; k++)
            if ((s = panel_enc(EN_K1 + k)) != 0) {
                fa_sc_turn(k, s);
                fa_sc_row = (uint8_t)k;
            }
        return;
    }
    if (fa_sq) {                                          /* the SEQ pop-up: ALGORITHM is the tempo (its own pop-up opens with it) */
        if ((s = panel_enc(EN_ALGO)) != 0) {
            fa_tp_turn(s);
            fa_tp_begin();
        }
        (void)panel_enc(EN_SELECT);
        for (k = 0; k < 4u; k++)
            (void)panel_enc(EN_K1 + k);
        return;
    }
    if (fa_ar) {                                          /* the ARP pop-up (it stays open until closed) */
        if ((s = panel_enc(EN_SELECT)) != 0)
            fa_ar_row = (uint8_t)((fa_ar_row + FA_AR_N + (s > 0 ? 1u : FA_AR_N - 1u)) % FA_AR_N);
        if ((s = panel_enc(EN_ALGO)) != 0) {
            fa_ar_turn(fa_ar_row, s);
            if (fa_ar_row == 5u)                          /* TEMPO: the TAP TEMPO pop-up opens */
                fa_tp_begin();
        }
        for (k = 0; k < 4u; k++)
            if ((s = panel_enc(EN_K1 + k)) != 0) {
                fa_ar_turn(k, s);
                fa_ar_row = (uint8_t)k;
            }
        return;
    }
    if (fa_sv && fa_sv_conf) {                            /* the question: ALGORITHM is YES / NO, the rest waits */
        (void)panel_enc(EN_SELECT);
        if ((s = panel_enc(EN_ALGO)) != 0)
            fa_sv_yes = s > 0;
        for (k = 0; k < 4u; k++)
            (void)panel_enc(EN_K1 + k);
        return;
    }
    if (fa_sv) {                                          /* the SAVE dialog: name editing; the knobs wait */
        if ((s = panel_enc(EN_SELECT)) != 0)
            fa_sv_pos = (uint8_t)((fa_sv_pos + 12u + (s > 0 ? 1u : 11u)) % 12u);
        if ((s = panel_enc(EN_ALGO)) != 0) {
            char *c = &fa_sv_name[fa_sv_pos];
            uint32_t i, n = (uint32_t)sizeof FA_CH - 1u, at = 0;
            for (i = 0; i < fa_sv_pos; i++)               /* a short name: pad with blanks up to the cursor */
                if (!fa_sv_name[i])
                    fa_sv_name[i] = ' ';
            if (!*c)
                *c = ' ';
            for (i = 0; i < n; i++)
                if (FA_CH[i] == *c)
                    at = i;
            *c = FA_CH[(at + n + (s > 0 ? 1u : n - 1u)) % n];
            if (!fa_sv_name[fa_sv_pos + 1u] && fa_sv_pos < 11u)
                fa_sv_name[fa_sv_pos + 1u] = 0;
        }
        for (k = 0; k < 4u; k++)
            (void)panel_enc(EN_K1 + k);
        return;
    }
    if ((s = panel_enc(EN_SELECT)) != 0)
        fa_select(s);
    fmg_force_page = (int8_t)fa_page;
    if ((s = panel_enc(EN_ALGO)) != 0)
        fmg_algo(s);
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0) {
            fmg_force_page = (int8_t)fa_page;
            fmg_knob(k, s);
            fmg_cur[fa_page] = (uint8_t)k;                  /* the highlight follows the knob last turned */
        }
}

static void fa_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0}, nb[FM1_NCOL] = {0}, nm[FM1_NCOL] = {0};
    uint32_t k, c;
    static uint8_t ready;
    if (idle_leds() || fm1_led_anim_on())
        return;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);                       /* every button's glow */
    for (k = 0; k < 27u; k++)
        led_put(nd, 14u + k, 1);                            /* every key's glow too: the keys lit when held or sounding stand over it */
    {
        static const uint8_t PG_LED[FA_NPAGE] = {B_LFO, B_SCL, B_SCL, B_EDIT, B_ENV, B_ENV, B_FX};
        led_put(nl, panel.btn[PG_LED[fa_page]], 1);          /* the page's button */
        led_put(nl, panel.btn[B_HOME], fa_page == 0u);        /* CTRL is HOME's: both light */
    }
    led_put(nl, panel.btn[B_ARP], TSEL->p[P_AMODE] != 0);
    led_put(nl, panel.btn[B_SEQ], fa_sq != 0u || fa_sc != 0u);
    led_put(nl, panel.btn[B_REC], fa_sq_rec != 0u);
    if (fa_tp)                                              /* TAP TEMPO: EDIT blinks at the tempo */
        led_put(nl, panel.btn[B_EDIT], (fm1_ms % (60000u / (uint32_t)song.g[G_BPM])) < (60000u / (uint32_t)song.g[G_BPM]) / 3u);
    led_put(nl, panel.btn[B_PLAY], song.playing != 0u);
    led_put(nl, panel.btn[B_SAVE], fa_sv);
    led_put(nl, panel.btn[B_GLO], fa_gl != 0u);
    led_put(nl, panel.btn[B_OCTDN], song.octave < 0);
    led_put(nl, panel.btn[B_OCTUP], song.octave > 0);
    c = fm1_in.notes | fa_sound_keys();                     /* the keys held, and the notes sounding: arpeggio, sequence, MIDI */
    for (k = 0; k < 27u; k++)
        led_put(nl, 14u + k, (int)((c >> k) & 1u));
    for (k = 0; k < FM1_NCOL; k++) {
        fm1_led_dim[k] = nd[k];
        fm1_led_breath[k] = nb[k];
        fm1_led_mid[k] = nm[k];
        fm1_led[k] = nl[k];
    }
}

/* power-on: track 1 is the instrument (the first factory patch), the others are muted FMINI parts */
static void fa_init(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if (t->eng_req != ENGI_FMINI || i) {              /* track 1 keeps its autosaved sound; the rest start fresh and muted */
            set_engine_of(t, ENGI_FMINI);
            apply_preset_to(t, 0);
            t->eng_req = ENGI_FMINI;
            t->engine = t->eng_req;
            fmini_load_slot(i, 0);
            fmini_adopt(i);
            if (!i)
                fa_factory_extras(t, 0);                 /* the first patch's arpeggiator, LFO, FX settings and octave */
        }
        else if (t->p[P_E7] >= 0 && t->p[P_E7] < (int32_t)FMINI_NSLOT) {
            fmini_x_t x;                                   /* the autosaved slot (the octave is not autosaved): its own octave, as a PRESETS turn gives */
            fb_extra((uint32_t)t->p[P_E7], &x);
            song.octave = x.oct;
        }
        t->p[P_MUTE] = i ? 1 : 0;
    }
    song.sel = 0;
    fa_cfg_load();
    ui_idle |= 1u;                                           /* the idle light show (ui_input.c idle_leds): on, after a minute with no input */
    fa_primed = 0;
    fa_last_sl = -2;
    fa_last_user = 0;
    fa_pp_until = 0;
    fa_mono();
    fa_page = 0;
    fa_track_preset();                                      /* the patch loaded at power-on: shown, no pop-up */
    fa_primed = 1;
    ui.force = 1;
}
#endif /* FELUCCA_FMINI_ONLY */
