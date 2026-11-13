/* SPDX-License-Identifier: GPL-3.0-only
 * OBERV engine parameters: OberVave's (zvenson, GPL-3.0-only, firmware/src/obv_params.h), every name prefixed OB_ so it cannot
 * meet the firmware's own P_*, F_* ... Made by tools/gen_oberv.py, not edited by hand. */
/* SPDX-License-Identifier: GPL-3.0-only */
/* The parameter table: a patch is a byte per parameter plus a 10-character name. The UI, MIDI CC, the
 * randomizer and the storage all walk this table. */
enum {
    OB_P_O1_WAVE, OB_P_O1_PW, OB_P_O1_LVL, OB_P_NOISE,
    OB_P_O2_WAVE, OB_P_O2_PW, OB_P_O2_LVL, OB_P_O2_SEMI,
    OB_P_SYNC, OB_P_XMOD, OB_P_ENV_O2, OB_P_O2_FINE,
    OB_P_CUTOFF, OB_P_RESO, OB_P_FENV_AMT, OB_P_KEYTRK, OB_P_FTYPE,
    OB_P_DRIVE, OB_P_VEL_FLT, OB_P_VEL_AMP,
    OB_P_FA, OB_P_FD, OB_P_FS, OB_P_FR,
    OB_P_AA, OB_P_AD, OB_P_AS, OB_P_AR,
    OB_P_LFO_SHAPE, OB_P_LFO_RATE, OB_P_LFO_DELAY, OB_P_LFO_PHASE,
    OB_P_LFO_PITCH, OB_P_LFO_PWM, OB_P_LFO_FLT, OB_P_LFO_AMP, OB_P_LFO_PITCH_TO, OB_P_LFO_PWM_TO,
    OB_P_MODE, OB_P_UNI_N, OB_P_UNI_DET, OB_P_GLIDE,
    OB_P_VINTAGE, OB_P_SPREAD, OB_P_BEND, OB_P_VOLUME,
    OB_P_MOD_DEPTH,
    OB_P_ARP_MODE, OB_P_ARP_DIV, OB_P_ARP_OCT, OB_P_ARP_GATE,
    OB_P_FMORPH,                                   /* (appended: the MIDI CC numbers of the above stay) */
    OB_P_CHO_RATE, OB_P_CHO_DEPTH, OB_P_CHO_MIX,
    OB_P_DLY_TIME, OB_P_DLY_FB, OB_P_DLY_TONE, OB_P_DLY_MIX,
    OB_P_REV_SIZE, OB_P_REV_DAMP, OB_P_REV_PRE, OB_P_REV_MIX,
    OB_NPARAM
};
#define OB_PATCH_NAME 10u
#define OB_PATCH_BYTES 80u                         /* OB_NPARAM values, the name at OB_NPARAM, the rest spare */
#if OB_NPARAM + OB_PATCH_NAME > OB_PATCH_BYTES
#error "the patch outgrew OB_PATCH_BYTES"
#endif
#define OB_NPROG 99u                               /* 1-64 factory sounds, 65-99 INIT; the user's saves over them */
#define OB_NFACT 64u

enum { OB_K_NUM, OB_K_BIP, OB_K_ONOFF, OB_K_WAVE, OB_K_FTYPE, OB_K_MODE, OB_K_LSHAPE, OB_K_LPHASE, OB_K_TO, OB_K_ARP, OB_K_DIV, OB_K_SEMI, OB_K_MS, OB_K_HZ,
       OB_K_PCT, OB_K_CUT, OB_K_OCT, OB_K_MORPH, OB_K_DDIV };
enum { OB_W_OFF, OB_W_SAW, OB_W_PLS, OB_W_BOTH };
enum { OB_F_LP12, OB_F_HP12, OB_F_BP12, OB_F_NOTCH, OB_F_LP24, OB_F_SEM, OB_NFTYPE };   /* SEM: LP .. notch .. HP by MORPH */
enum { OB_M_POLY, OB_M_DUAL, OB_M_UNISON, OB_M_MONO };
enum { OB_L_TRI, OB_L_SAW, OB_L_RAMP, OB_L_SQR, OB_L_SH, OB_L_RND };
enum { OB_LP_SYNC, OB_LP_FREE, OB_LP_SPREAD };
enum { OB_TO_O1, OB_TO_O2, OB_TO_BOTH };
enum { OB_A_OFF, OB_A_UP, OB_A_DOWN, OB_A_UPDN, OB_A_RND, OB_A_PLAY, OB_NARPMODE };
enum { OB_D_4, OB_D_8, OB_D_8T, OB_D_16, OB_D_16T, OB_D_32, OB_D_32T, OB_NDIV };
enum { OB_DD_16, OB_DD_8T, OB_DD_8, OB_DD_8D, OB_DD_4, OB_DD_4D, OB_DD_2, OB_NDDIV };   /* the delay's note values */

typedef struct {
    const char *name;                           /* 8 characters at most (the knob label) */
    uint8_t max, def, kind, cc;                 /* cc: a fixed MIDI CC, 0 = taken from 20 up in table order */
} OB_pdef_t;
static const OB_pdef_t OB_PDEF[OB_NPARAM] = {
    [OB_P_O1_WAVE] = {"WAVE", 3, OB_W_SAW, OB_K_WAVE, 0},
    [OB_P_O1_PW] = {"PW", 127, 64, OB_K_PCT, 0},
    [OB_P_O1_LVL] = {"LEVEL", 127, 110, OB_K_NUM, 0},
    [OB_P_NOISE] = {"NOISE", 127, 0, OB_K_NUM, 0},
    [OB_P_O2_WAVE] = {"WAVE", 3, OB_W_SAW, OB_K_WAVE, 0},
    [OB_P_O2_PW] = {"PW", 127, 64, OB_K_PCT, 0},
    [OB_P_O2_LVL] = {"LEVEL", 127, 100, OB_K_NUM, 0},
    [OB_P_O2_SEMI] = {"SEMI", 48, 24, OB_K_SEMI, 0},
    [OB_P_SYNC] = {"SYNC", 1, 0, OB_K_ONOFF, 0},
    [OB_P_XMOD] = {"X-MOD", 127, 0, OB_K_NUM, 0},
    [OB_P_ENV_O2] = {"ENV>O2", 127, 64, OB_K_BIP, 0},
    [OB_P_O2_FINE] = {"FINE", 127, 70, OB_K_BIP, 0},
    [OB_P_CUTOFF] = {"CUTOFF", 127, 80, OB_K_CUT, 74},
    [OB_P_RESO] = {"RESO", 127, 20, OB_K_NUM, 71},
    [OB_P_FENV_AMT] = {"ENV AMT", 127, 90, OB_K_BIP, 0},
    [OB_P_KEYTRK] = {"KEY TRK", 127, 64, OB_K_PCT, 0},
    [OB_P_FTYPE] = {"TYPE", OB_NFTYPE - 1, OB_F_LP24, OB_K_FTYPE, 0},
    [OB_P_DRIVE] = {"DRIVE", 127, 0, OB_K_NUM, 0},
    [OB_P_VEL_FLT] = {"VEL>FLT", 127, 40, OB_K_NUM, 0},
    [OB_P_VEL_AMP] = {"VEL>AMP", 127, 60, OB_K_NUM, 0},
    [OB_P_FA] = {"ATTACK", 127, 20, OB_K_MS, 0},
    [OB_P_FD] = {"DECAY", 127, 70, OB_K_MS, 0},
    [OB_P_FS] = {"SUSTAIN", 127, 40, OB_K_NUM, 0},
    [OB_P_FR] = {"RELEASE", 127, 60, OB_K_MS, 0},
    [OB_P_AA] = {"ATTACK", 127, 10, OB_K_MS, 73},
    [OB_P_AD] = {"DECAY", 127, 70, OB_K_MS, 0},
    [OB_P_AS] = {"SUSTAIN", 127, 100, OB_K_NUM, 0},
    [OB_P_AR] = {"RELEASE", 127, 60, OB_K_MS, 72},
    [OB_P_LFO_SHAPE] = {"SHAPE", 5, OB_L_TRI, OB_K_LSHAPE, 0},
    [OB_P_LFO_RATE] = {"RATE", 127, 70, OB_K_HZ, 0},
    [OB_P_LFO_DELAY] = {"DELAY", 127, 0, OB_K_NUM, 0},
    [OB_P_LFO_PHASE] = {"PHASE", 2, OB_LP_FREE, OB_K_LPHASE, 0},
    [OB_P_LFO_PITCH] = {"PITCH", 127, 0, OB_K_NUM, 0},
    [OB_P_LFO_PWM] = {"PWM", 127, 0, OB_K_NUM, 0},
    [OB_P_LFO_FLT] = {"FILTER", 127, 0, OB_K_NUM, 0},
    [OB_P_LFO_AMP] = {"AMP", 127, 0, OB_K_NUM, 0},
    [OB_P_LFO_PITCH_TO] = {"PITCH>", 2, OB_TO_BOTH, OB_K_TO, 0},
    [OB_P_LFO_PWM_TO] = {"PWM>", 2, OB_TO_BOTH, OB_K_TO, 0},
    [OB_P_MODE] = {"MODE", 3, OB_M_POLY, OB_K_MODE, 0},
    [OB_P_UNI_N] = {"VOICES", 8, 4, OB_K_NUM, 0},
    [OB_P_UNI_DET] = {"DETUNE", 127, 30, OB_K_NUM, 0},
    [OB_P_GLIDE] = {"GLIDE", 127, 0, OB_K_MS, 5},
    [OB_P_VINTAGE] = {"VINTAGE", 127, 30, OB_K_NUM, 0},
    [OB_P_SPREAD] = {"SPREAD", 127, 60, OB_K_NUM, 0},
    [OB_P_BEND] = {"BEND", 24, 2, OB_K_NUM, 0},
    [OB_P_VOLUME] = {"VOLUME", 127, 100, OB_K_NUM, 0},
    [OB_P_MOD_DEPTH] = {"MOD DPT", 127, 0, OB_K_NUM, 1},
    [OB_P_ARP_MODE] = {"ARP", 5, OB_A_OFF, OB_K_ARP, 0},
    [OB_P_ARP_DIV] = {"DIV", 6, OB_D_16, OB_K_DIV, 0},
    [OB_P_ARP_OCT] = {"OCTAVES", 4, 1, OB_K_OCT, 0},
    [OB_P_ARP_GATE] = {"GATE", 127, 64, OB_K_PCT, 0},
    [OB_P_FMORPH] = {"MORPH", 127, 0, OB_K_MORPH, 0},
    [OB_P_CHO_RATE] = {"RATE", 127, 40, OB_K_NUM, 0},
    [OB_P_CHO_DEPTH] = {"DEPTH", 127, 50, OB_K_NUM, 0},
    [OB_P_CHO_MIX] = {"MIX", 127, 0, OB_K_NUM, 0},
    [OB_P_DLY_TIME] = {"TIME", OB_NDDIV - 1, OB_DD_8D, OB_K_DDIV, 0},
    [OB_P_DLY_FB] = {"FEEDBK", 127, 50, OB_K_NUM, 0},
    [OB_P_DLY_TONE] = {"TONE", 127, 70, OB_K_NUM, 0},
    [OB_P_DLY_MIX] = {"MIX", 127, 0, OB_K_NUM, 0},
    [OB_P_REV_SIZE] = {"SIZE", 127, 80, OB_K_NUM, 0},
    [OB_P_REV_DAMP] = {"DAMP", 127, 60, OB_K_NUM, 0},
    [OB_P_REV_PRE] = {"PRE-DLY", 127, 20, OB_K_NUM, 0},
    [OB_P_REV_MIX] = {"MIX", 127, 0, OB_K_NUM, 0},
};

static const char *const OB_WAVE_NAME[4] = {"OFF", "SAW", "PULSE", "SAW+PLS"};
static const char *const OB_FTYPE_NAME[OB_NFTYPE] = {"LP 12", "HP 12", "BP 12", "NOTCH", "LP 24", "SEM"};
static const char *const OB_MODE_NAME[4] = {"POLY", "DUAL", "UNISON", "MONO"};
static const char *const OB_LSHAPE_NAME[6] = {"TRI", "SAW", "RAMP", "SQUARE", "S&H", "RANDOM"};
static const char *const OB_LPHASE_NAME[3] = {"SYNC", "FREE", "SPREAD"};
static const char *const OB_TO_NAME[3] = {"OSC1", "OSC2", "BOTH"};
static const char *const OB_ARP_NAME[OB_NARPMODE] = {"OFF", "UP", "DOWN", "UP/DN", "RANDOM", "PLAYED"};
static const char *const OB_DIV_NAME[OB_NDIV] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T"};
static const uint8_t OB_DIV_PER_QUARTER[OB_NDIV] = {1, 2, 3, 4, 6, 8, 12};
static const char *const OB_DDIV_NAME[OB_NDDIV] = {"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"};
static const uint8_t OB_DDIV_NUM[OB_NDDIV] = {1, 1, 1, 3, 1, 3, 2}, OB_DDIV_DEN[OB_NDDIV] = {4, 3, 2, 4, 1, 2, 1};   /* of a quarter */

/* the pages: four knobs each (-1 = no knob) */
typedef struct { const char *title; int8_t p[4]; } OB_page_t;
enum { OB_PG_PERFORM, OB_PG_OSC1, OB_PG_OSC2, OB_PG_VOICE, OB_PG_OSCMOD, OB_PG_FILTER, OB_PG_FILTER2, OB_PG_FENV, OB_PG_AENV, OB_PG_LFO, OB_PG_LFODEST,
       OB_PG_LFO2, OB_PG_ARP, OB_PG_CHORUS, OB_PG_DELAY, OB_PG_REVERB, OB_PG_DRIVE, OB_PG_OUTPUT, OB_PG_GLOBAL,   /* (3.3, as native 3.2) VOICE under EDIT, after OSC 2 */
       OB_PG_MIXER, OB_PG_SEQ, OB_NPAGE };              /* the mixer (VIBE), the sequencer's step view: drawn by hand */
static const OB_page_t OB_PAGE[OB_NPAGE] = {
    [OB_PG_PERFORM] = {"PERFORM", {OB_P_CUTOFF, OB_P_RESO, OB_P_FENV_AMT, OB_P_AR}},   /* (ALGORITHM picks the track now, as sloopDX) */
    [OB_PG_OSC1] = {"OSC 1", {OB_P_O1_WAVE, OB_P_O1_PW, OB_P_O1_LVL, OB_P_NOISE}},
    [OB_PG_OSC2] = {"OSC 2", {OB_P_O2_WAVE, OB_P_O2_PW, OB_P_O2_LVL, OB_P_O2_SEMI}},
    [OB_PG_OSCMOD] = {"OSC MOD", {OB_P_SYNC, OB_P_XMOD, OB_P_ENV_O2, OB_P_O2_FINE}},
    [OB_PG_FILTER] = {"FILTER", {OB_P_RESO, OB_P_FENV_AMT, OB_P_KEYTRK, OB_P_FTYPE}},
    [OB_PG_FILTER2] = {"FILTER +", {OB_P_DRIVE, OB_P_VEL_FLT, OB_P_VEL_AMP, OB_P_FMORPH}},
    [OB_PG_FENV] = {"FILTER ENV", {OB_P_FA, OB_P_FD, OB_P_FS, OB_P_FR}},
    [OB_PG_AENV] = {"AMP ENV", {OB_P_AA, OB_P_AD, OB_P_AS, OB_P_AR}},
    [OB_PG_LFO] = {"LFO", {OB_P_LFO_SHAPE, OB_P_LFO_RATE, OB_P_LFO_DELAY, OB_P_LFO_PHASE}},
    [OB_PG_LFODEST] = {"LFO DEST", {OB_P_LFO_PITCH, OB_P_LFO_PWM, OB_P_LFO_FLT, OB_P_LFO_AMP}},
    [OB_PG_LFO2] = {"LFO +", {OB_P_LFO_PITCH_TO, OB_P_LFO_PWM_TO, -1, -1}},
    [OB_PG_VOICE] = {"VOICE", {OB_P_MODE, OB_P_UNI_N, OB_P_UNI_DET, OB_P_GLIDE}},
    [OB_PG_OUTPUT] = {"OUTPUT", {OB_P_VINTAGE, OB_P_SPREAD, OB_P_BEND, OB_P_VOLUME}},
    [OB_PG_ARP] = {"ARP", {OB_P_ARP_MODE, OB_P_ARP_DIV, OB_P_ARP_OCT, OB_P_ARP_GATE}},
    [OB_PG_CHORUS] = {"CHORUS", {OB_P_CHO_RATE, OB_P_CHO_DEPTH, OB_P_CHO_MIX, -1}},
    [OB_PG_DELAY] = {"DELAY", {OB_P_DLY_TIME, OB_P_DLY_FB, OB_P_DLY_TONE, OB_P_DLY_MIX}},
    [OB_PG_REVERB] = {"REVERB", {OB_P_REV_SIZE, OB_P_REV_DAMP, OB_P_REV_PRE, OB_P_REV_MIX}},
    [OB_PG_DRIVE] = {"DRIVE", {-1, -1, -1, -1}},   /* (VIBE) the drive send: TYPE, DRIVE, TONE, MIX – the settings', drawn by hand */
    [OB_PG_GLOBAL] = {"GLOBAL", {-1, -1, -1, -1}},   /* tempo, MIDI channel, tune, CPU: globals, drawn by hand */
    [OB_PG_MIXER] = {"MIXER", {-1, -1, -1, -1}},   /* level, pan, send per track (VIBE): drawn by hand */
    [OB_PG_SEQ] = {"SEQUENCER", {-1, -1, -1, -1}},   /* length, note value, swing, transpose; a held step: its own four */
};

