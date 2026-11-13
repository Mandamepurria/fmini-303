/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
#define FELUCCA_FAVORITES 1
/* Stable engine/preset references; fixed capacity independent of optional engines. */
typedef struct {
    uint8_t factory[16][32]; /* engine 0..15, preset 0..255 */
    uint32_t user, filter;
} favorites_t;
static favorites_t favorites __attribute__((section(".pool")));
/* rows 14 and 15 of factory[] are the MIDI LEARN map and the settings (midi_learn.c, below): the engines 0..13 keep their own row.
 * ACID build: FMINI (14), ACID (15), OBERV (16) and SYNTHEX (17) have favourites too, in the two rows of the engines retired in 1.13
 * (GRAIN, 8, and SLICE, 13), which nothing reads: FMINI's 200 slots take 25 bytes of row 13; ACID (6 presets) row 8 byte 0, OBERV (70) bytes 1..9,
 * SYNTHEX (80) bytes 10..19. A star a retired engine had left there (a setting kept from before 1.13) would show on those; the bytes
 * hold exactly the presets (ACID 6 in one byte: 8 bits, OBERV 70 in 9: 72, SYNTHEX 80 in 10); the editor and the UI bound a star by the engine's npresets. The rest of the table, the editor and the UI only meet favorite_has / favorite_set */
#define FAV_ROWS 14u
static const struct { uint8_t row, off, n; } FAV_EXT[NENGINES > 14 ? NENGINES - 14 : 1] = {
    {13, 0, 25},    /* 14 FMINI: 200 slots */
    {8, 0, 1},      /* 15 ACID */
    {8, 1, 9},      /* 16 OBERV */
    {8, 10, 10},    /* 17 SYNTHEX */
};
_Static_assert(NENGINES <= 18, "favorites.c FAV_EXT: a row of bytes for each engine past 14");
static uint8_t *fav_byte(uint32_t engine, uint32_t preset)      /* the byte that holds engine's preset's star, 0 = it has none */
{
    if (engine < FAV_ROWS)
        return preset < 256u ? &favorites.factory[engine][preset / 8u] : 0;
    if (engine < NENGINES && engine - 14u < sizeof FAV_EXT / sizeof FAV_EXT[0] && preset / 8u < FAV_EXT[engine - 14u].n)
        return &favorites.factory[FAV_EXT[engine - 14u].row][FAV_EXT[engine - 14u].off + preset / 8u];
    return 0;
}
static int favorite_has(uint32_t engine, uint32_t preset)
{
    uint8_t *b;
    if (engine == NENGINES)
        return preset < UP_SLOTS && ((favorites.user >> preset) & 1u);
    b = fav_byte(engine, preset);
    return b && ((*b >> (preset % 8u)) & 1u);
}
static int favorite_set(uint32_t engine, uint32_t preset, int on)
{
    uint8_t *b;
    if (engine > NENGINES || (engine == NENGINES && preset >= UP_SLOTS))
        return 0;
    if (engine != NENGINES && !fav_byte(engine, preset))
        return 0;
    if (favorite_has(engine, preset) == !!on) return 0;
    if (engine == NENGINES) {
        if (on) favorites.user |= 1u << preset;
        else favorites.user &= ~(1u << preset);
    } else {
        b = fav_byte(engine, preset);
        if (on) *b |= (uint8_t)(1u << (preset % 8u));
        else *b &= (uint8_t)~(1u << (preset % 8u));
    }
    return 1;
}

/* MENU > DISPLAY > COLOR's 11th value, CUSTOM (1.6, #244, Discussion #239): a palette of the user's, five RGB565 colours (BG
 * SURF TEXT THEME ACCENT, 16 bits little endian) in favorites.factory[15][0..9], flags in [12] (bit 0: one is stored, bit 1:
 * it is the palette in use), set from the editor (PALETTE_SET, editor_palette.c) and saved with the settings. settings.palette
 * stays a built-in palette (the one in use before, or after): firmware before 1.6 shows that and keeps the bytes. The value
 * is in COLOR only while one is stored. Zero (every older setting) = none */
#define ui_cpal (&favorites.factory[15][0])
#define ui_cpal_flags (favorites.factory[15][12])
#define CPAL_STORED 1u
#define CPAL_ACTIVE 2u
static uint32_t cpal_on(void) { return (ui_cpal_flags & (CPAL_STORED | CPAL_ACTIVE)) == (CPAL_STORED | CPAL_ACTIVE); }
static uint32_t palette_n(void) { return NPALETTES + ((ui_cpal_flags & CPAL_STORED) ? 1u : 0u); }     /* COLOR's values */
static uint32_t palette_cur(void) { return cpal_on() ? NPALETTES : settings.palette % NPALETTES; }
static const char *palette_name(uint32_t v) { return v == NPALETTES ? "CUSTOM" : UI_PALETTES[v % NPALETTES].name; }
/* the palette in use into gfx.c (the settings changed under it: a start, an import, a restore, a STYLE change) */
static void palette_apply(void)
{
    if (cpal_on()) {
        palette_custom_load(ui_cpal);
        palette_set(NPALETTES);
    } else {
        palette_set(settings.palette % NPALETTES);
    }
}

/* the five colours into the settings' bytes (stored; not yet the palette in use: palette_choose / palette_apply) */
static void cpal_store(const uint16_t *c)
{
    uint32_t i;
    for (i = 0; i < 5u; i++) {
        ui_cpal[2u * i] = (uint8_t)c[i];
        ui_cpal[2u * i + 1u] = (uint8_t)(c[i] >> 8);
    }
    ui_cpal_flags |= CPAL_STORED;
}
