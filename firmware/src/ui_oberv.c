/* SPDX-License-Identifier: GPL-3.0-only
 * OBERV's panel (EDIT family, an OBERV track only, after EDIT 2): twelve pages of four knobs, in the order of OberVave's
 * own (obv_params.h PAGE), the 63 parameters of the patch but the seventeen the host does for the voice (the unison number
 * and detune, SPREAD, BEND, MOD DEPTH, the arpeggiator, the effects' own settings (REV PRE among them)). KNOB 1..4 edit the page's controls
 * directly; a parameter that is one of the EDIT values is that value, the others are bytes of the track's patch, and
 * editing one makes the sound OWN (PROGRAM 0). Included by ui_graph.c after ui_fmini.c. */
#define OBP_NPG 12u
static const struct { const char *name; int8_t p[4]; } OBP_PAGE[OBP_NPG] = {
    {"OSC 1", {OB_P_O1_WAVE, OB_P_O1_PW, OB_P_O1_LVL, OB_P_NOISE}},
    {"OSC 2", {OB_P_O2_WAVE, OB_P_O2_PW, OB_P_O2_LVL, OB_P_O2_SEMI}},
    {"OSC MOD", {OB_P_SYNC, OB_P_XMOD, OB_P_ENV_O2, OB_P_O2_FINE}},
    {"FILTER", {OB_P_CUTOFF, OB_P_RESO, OB_P_FENV_AMT, OB_P_KEYTRK}},
    {"FILTER 2", {OB_P_FTYPE, OB_P_FMORPH, OB_P_DRIVE, OB_P_VEL_FLT}},
    {"F.ENV", {OB_P_FA, OB_P_FD, OB_P_FS, OB_P_FR}},
    {"A.ENV", {OB_P_AA, OB_P_AD, OB_P_AS, OB_P_AR}},
    {"LFO", {OB_P_LFO_SHAPE, OB_P_LFO_RATE, OB_P_LFO_DELAY, OB_P_LFO_PHASE}},
    {"LFO DEST", {OB_P_LFO_PITCH, OB_P_LFO_PWM, OB_P_LFO_FLT, OB_P_LFO_AMP}},
    {"LFO 2", {OB_P_LFO_PITCH_TO, OB_P_LFO_PWM_TO, OB_P_VEL_AMP, OB_P_VINTAGE}},
    {"VOICE", {OB_P_MODE, OB_P_GLIDE, OB_P_VOLUME, -1}},
    {"SENDS", {OB_P_CHO_MIX, OB_P_DLY_MIX, OB_P_REV_MIX, -1}},
};
static int obp_page(uint32_t g) { return g >= GR_OBV0 && g <= GR_OBV11; }
static int obp_ok(void) { return TSEL->eng_req % NENGINES == ENGI_OBERV; }
static uint32_t obp_pg(void) { return (cur_page()->graph - GR_OBV0) % OBP_NPG; }
static int obp_param(uint32_t pg, uint32_t c) { return OBP_PAGE[pg % OBP_NPG].p[c & 3u]; }

/* the value of control c of page pg as text; unit; position 0..1000 */
static const char *obp_names(uint32_t kind, uint32_t v)
{
    switch (kind) {
    case OB_K_WAVE: return OB_WAVE_NAME[v & 3u];
    case OB_K_FTYPE: return OB_FTYPE_NAME[v % OB_NFTYPE];
    case OB_K_MODE: return OB_MODE_NAME[v & 3u];
    case OB_K_LSHAPE: return OB_LSHAPE_NAME[v % 6u];
    case OB_K_LPHASE: return OB_LPHASE_NAME[v % 3u];
    case OB_K_TO: return OB_TO_NAME[v % 3u];
    case OB_K_ONOFF: return v ? "ON" : "OFF";
    default: return 0;
    }
}
static void obp_card_at(uint32_t pg, uint32_t c, int32_t v, const char **label, char *val, const char **unit, int32_t *ratio, uint32_t *icon)
{
    int p = obp_param(pg, c);
    *icon = ICON_NONE;
    *unit = "";
    if (p < 0) {
        *label = "";
        val[0] = 0;
        *ratio = -1;
        return;
    }
    *label = OB_PDEF[p].name;
    {
        const char *nm = obp_names(OB_PDEF[p].kind, (uint32_t)v);
        if (nm) {
            uint32_t i;
            for (i = 0; nm[i] && i < 7u; i++)
                val[i] = nm[i];
            val[i] = 0;
        } else if (OB_PDEF[p].kind == OB_K_BIP || OB_PDEF[p].kind == OB_K_SEMI) {
            fmt_int(val, v - (OB_PDEF[p].kind == OB_K_BIP ? 64 : 24));
        } else {
            fmt_int(val, v);
        }
    }
    *ratio = OB_PDEF[p].max ? v * 1000 / (int32_t)OB_PDEF[p].max : 0;
}
static void obp_card(uint32_t c, const char **label, char *val, const char **unit, int32_t *ratio, uint32_t *icon)
{
    uint32_t pg = obp_pg();
    int p = obp_param(pg, c);
    obp_card_at(pg, c, p < 0 ? 0 : (int32_t)obv_get(TSEL, (uint32_t)(TSEL - trk) % NTRK, (uint32_t)p), label, val, unit, ratio, icon);
}
static void obp_range(uint32_t pg, uint32_t c, int32_t *lo, int32_t *hi)
{
    int p = obp_param(pg, c);
    *lo = 0;
    *hi = p < 0 ? 0 : (int32_t)OB_PDEF[p].max;
}
static void obp_knob(uint32_t slot, int32_t steps)
{
    uint32_t pg = obp_pg();
    int p = obp_param(pg, slot);
    track_t *t = TSEL;
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    int32_t v, max;
    if (!obp_ok() || p < 0 || !steps)
        return;
    max = OB_PDEF[p].max;
    v = (int32_t)obv_get(t, tr, (uint32_t)p) + steps;
    obv_set(t, tr, (uint32_t)p, (uint32_t)clamp(v, 0, max));
}
