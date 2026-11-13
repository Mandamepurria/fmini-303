/* SPDX-License-Identifier: GPL-3.0-only
 * OBERV: OberVave's synth voice as an engine (OberVave, zvenson, GPL-3.0-only, https://github.com/zvenson/obervave; its
 * firmware/src/obv_engine.h, obv_dsp.h, obv_params.h and obv_patches.h, through tools/gen_oberv.py -> eng_oberv_*.h).
 *
 * A voice: two oscillators (saw, pulse, both, with PWM, hard sync, X-MOD, the filter envelope on OSC 2) and noise -> a mixer
 * with an input stage (DRIVE) -> a 12 dB state-variable filter (LP HP BP NOTCH, or SEM: LP .. notch .. HP by MORPH) or a 24 dB
 * four-pole ladder -> its amplifier. Two envelopes (analog RC curves), an LFO (per voice phase in SPREAD, delay / fade-in),
 * glide, VINTAGE (each of the eight "voice cards" has its own small tune, cutoff and envelope offsets, and a slow random
 * walk). Integers only, as the rest of the firmware.
 *
 * What changed in the port (OberVave's engine is a whole instrument: four tracks, eight shared voices, an arpeggiator, a
 * sequencer, a mixer, effects):
 *   - the host does what OberVave's engine did around the voice: POLY / MONO / LEGATO / UNISON (P_VOICE), the voice
 *     allocation, the unison detune and the spread, the pitch bend, the tuning and the glide of the key (the part's pitch is
 *     vmod_t.pitch16), the mod wheel, the arpeggiator and the sequencer of the track, the mixer, the sends. The patch's MODE
 *     sets P_VOICE when a program loads (POLY, DUAL: POLY; UNISON; MONO: LEGATO); its CHORUS, DELAY and REVERB mixes set
 *     the track's sends; its other effect parameters (and ARP, SPREAD, BEND, MOD DEPTH, the unison number and detune) are kept
 *     in the patch and not used: the host's own FX settings and controls are the track's.
 *   - GLIDE is OberVave's (a patch parameter: the pitch moves from the last note to the new one, the table's time).
 *   - the control block is 16 frames (OberVave's CTL); the host's is 32: a voice runs two of them a block, the part's LFO and
 *     smoothed controls are made twice (obv_block).
 *   - the amplitude is the voice's own (engine_t.ownenv): VEL > AMP, the amp envelope, the LFO tremolo and VOLUME.
 *
 * The patch (63 parameters + a 10 character name, eng_oberv_params.h) is the track's: obv_patch[track], kept in projects and
 * user presets in the slot of the FM6 patch (the track plays one engine at a time: project.c, up_fm6.c), as FMINI's is.
 * Seven of its parameters are the EDIT values, which are the live ones: E0 CUTOFF, E1 RESO, E2 ENV AMT, E3 F.DECAY,
 * E4 A.ATTACK, E5 A.RELEASE, E6 DRIVE; E7 PROGRAM (0 = OWN, 1..70 the factory programs: choosing one loads the whole patch
 * and sets the seven; turning one of the seven makes the track's sound its OWN). The patch bytes of those seven are
 * refreshed from the EDIT values when the patch is saved or read (obv_capture, obv_patch_get). State per part: engine memory
 * (obv_mem_t), a voice's slot of it by the voice's index. */
#include "eng_oberv_tables.h"
#include "eng_oberv_dsp.h"
#include "eng_oberv_params.h"
#include "eng_oberv_presets.h"

#define ENGI_OBERV 16u          /* engines.c ENGINES[] (append-only) */
#define OBV_CTL 16u             /* OberVave's control block, in frames */
#define OBV_LEVEL_FLOOR 2048
#define OBV_ENV_ONE (1 << 24)
#define OBV_PB OB_PATCH_BYTES   /* the patch's size (63 + name + spare): fits the FM6 patch slot of a project (128) */
enum { OBV_E_IDLE, OBV_E_ATT, OBV_E_DEC, OBV_E_SUS, OBV_E_REL };

/* the seven parameters the EDIT values are, in E0..E6 order */
static const uint8_t OBV_EMAP[7] = {OB_P_CUTOFF, OB_P_RESO, OB_P_FENV_AMT, OB_P_FD, OB_P_AA, OB_P_AR, OB_P_DRIVE};

static uint8_t obv_patch[NTRK][OBV_PB];           /* the tracks' patches */
static uint8_t obv_ready[NTRK];                   /* 1 once a track's patch has been set (else the defaults are made) */
static uint8_t obv_slot_seen[NTRK];               /* the E7 value last acted on (255 = none yet) */
static uint8_t obv_e_seen[NTRK][7];               /* E0..E6 as the program / patch last set them */

static const char OBV_NAMEPAD = ' ';

static void obv_defaults(uint8_t *p)
{
    uint32_t i;
    for (i = 0; i < OB_NPARAM; i++)
        p[i] = OB_PDEF[i].def;
    for (; i < OBV_PB; i++)
        p[i] = (uint8_t)OBV_NAMEPAD;
}
static void obv_sanitize(uint8_t *p)               /* every byte inside its range, the name printable */
{
    uint32_t i;
    for (i = 0; i < OB_NPARAM; i++)
        if (p[i] > OB_PDEF[i].max)
            p[i] = OB_PDEF[i].max;
    for (i = OB_NPARAM; i < OB_NPARAM + OB_PATCH_NAME; i++)
        if (p[i] < 32u || p[i] > 126u)
            p[i] = (uint8_t)OBV_NAMEPAD;
}
static void obv_touch(uint32_t tr)                 /* a patch exists for the track */
{
    if (!obv_ready[tr]) {
        obv_defaults(obv_patch[tr]);
        obv_ready[tr] = 1;
    }
}

/* ---- the host's side: the patch to and from the track's EDIT values, the program, the voice mode ---- */
static void obv_to_edit(track_t *t, uint32_t tr)   /* the seven EDIT values from the patch */
{
    uint32_t k;
    for (k = 0; k < 7u; k++) {
        t->p[P_E0 + k] = obv_patch[tr][OBV_EMAP[k]];
        obv_e_seen[tr][k] = obv_patch[tr][OBV_EMAP[k]];
    }
}
static void obv_from_edit(const track_t *t, uint32_t tr, uint8_t *dst)   /* the patch with the EDIT values written into it */
{
    uint32_t k;
    memcpy(dst, obv_patch[tr], OBV_PB);
    for (k = 0; k < 7u; k++)
        dst[OBV_EMAP[k]] = (uint8_t)clamp(t->p[P_E0 + k], 0, OB_PDEF[OBV_EMAP[k]].max);
}
/* the voice mode and the sends a patch asks for (the host's P_VOICE and the track's sends) */
static void obv_host_from_patch(track_t *t, uint32_t tr)
{
    const uint8_t *p = obv_patch[tr];
    t->p[P_VOICE] = p[OB_P_MODE] == OB_M_UNISON ? V_UNISON : p[OB_P_MODE] == OB_M_MONO ? V_LEGATO : V_POLY;
    t->p[P_CHOR] = p[OB_P_CHO_MIX];
    t->p[P_DLY] = p[OB_P_DLY_MIX];
    t->p[P_REV] = p[OB_P_REV_MIX];
}
/* the patch as saved (projects, user presets): the EDIT values in, 128 bytes = the patch and zeros */
static void obv_capture(const track_t *t, uint32_t tr, uint8_t *dst128)
{
    obv_touch(tr);
    memset(dst128, 0, 128);
    obv_from_edit(t, tr, dst128);
}
static void obv_set_patch(uint32_t tr, const uint8_t *src)    /* a patch from a project / a user preset / the editor */
{
    uint8_t s[OBV_PB];
    tr %= NTRK;
    memcpy(s, src, OBV_PB);
    obv_sanitize(s);
    memcpy(obv_patch[tr], s, OBV_PB);
    obv_ready[tr] = 1;
}
static void obv_load_program(track_t *t, uint32_t tr, uint32_t i)   /* factory program i (0..69) */
{
    uint32_t j;
    uint8_t *p = obv_patch[tr];
    for (j = 0; j < OB_NPARAM; j++)
        p[j] = OBV_FACTORY[i].v[j];
    for (j = 0; j < OBV_PB - OB_NPARAM; j++)           /* the name, then spaces */
        p[OB_NPARAM + j] = j < OB_PATCH_NAME && OBV_FACTORY[i].name[j] ? (uint8_t)OBV_FACTORY[i].name[j] : (uint8_t)OBV_NAMEPAD;
    obv_ready[tr] = 1;
    obv_to_edit(t, tr);
    obv_host_from_patch(t, tr);
}

/* ---- MIDI CC: OberVave's numbers (CUTOFF 74, RESO 71, A.ATTACK 73, A.RELEASE 72, GLIDE 5; the rest from 20 up in the order
 * of the parameter table, skipping 64), every parameter of the patch. The host keeps 1 (the mod wheel), 7, 10, 64 and the
 * channel messages. A parameter that is one of the seven EDIT values sets it; any other sets the patch and the sound is
 * OWN. MODE and the three effect mixes also set the host's voice mode and sends. Returns 1 when the CC was the engine's. */
static uint8_t obv_cc_map[128];                   /* parameter + 1, 0 = none */
static uint8_t obv_cc_ready;
static uint8_t obv_cc_fixed(uint32_t p)
{
    switch (p) {
    case OB_P_CUTOFF: return 74;
    case OB_P_RESO: return 71;
    case OB_P_AA: return 73;
    case OB_P_AR: return 72;
    case OB_P_GLIDE: return 5;
    case OB_P_MOD_DEPTH: return 1;
    default: return 0;
    }
}
static void obv_cc_init(void)
{
    uint32_t i, next = 20;
    for (i = 0; i < OB_NPARAM; i++)
        if (obv_cc_fixed(i)) obv_cc_map[obv_cc_fixed(i)] = (uint8_t)(i + 1u);
    for (i = 0; i < OB_NPARAM; i++) {
        if (obv_cc_fixed(i)) continue;
        while (next < 120u && (obv_cc_map[next] || next == 64u)) next++;
        if (next < 120u) obv_cc_map[next++] = (uint8_t)(i + 1u);
    }
    obv_cc_ready = 1;
}
/* a parameter of the track's patch: its value, the EDIT value when it is one of the seven */
static uint32_t obv_get(const track_t *t, uint32_t tr, uint32_t p)
{
    uint32_t k;
    obv_touch(tr);
    for (k = 0; k < 7u; k++)
        if (OBV_EMAP[k] == p)
            return (uint32_t)clamp(t->p[P_E0 + k], 0, OB_PDEF[p].max);
    return obv_patch[tr][p];
}
static void obv_set(track_t *t, uint32_t tr, uint32_t p, uint32_t v)   /* the CC and the panel: the sound is OWN after it */
{
    uint32_t k;
    obv_touch(tr);
    v = v > OB_PDEF[p].max ? OB_PDEF[p].max : v;
    for (k = 0; k < 7u; k++)
        if (OBV_EMAP[k] == p) {
            t->p[P_E0 + k] = (int16_t)v;
            t->p[P_E7] = 0;                           /* OWN at once (the block would see it a block later) */
            obv_slot_seen[tr] = 0;
            obv_e_seen[tr][k] = (uint8_t)v;
            return;
        }
    obv_patch[tr][p] = (uint8_t)v;
    t->p[P_E7] = 0;
    obv_slot_seen[tr] = 0;
    if (p == OB_P_MODE || p == OB_P_CHO_MIX || p == OB_P_DLY_MIX || p == OB_P_REV_MIX)
        obv_host_from_patch(t, tr);
}
static int obv_cc(track_t *t, uint32_t cc, uint32_t value)
{
    uint32_t tr = (uint32_t)(t - trk), p, k, v;
    if (tr >= NTRK || cc >= 120u || cc == 1u || cc == 7u || cc == 10u || cc == 64u)
        return 0;
    if (!obv_cc_ready)
        obv_cc_init();
    if (!obv_cc_map[cc])
        return 0;
    p = (uint32_t)obv_cc_map[cc] - 1u;
    v = OB_PDEF[p].max == 127u ? (value & 127u) : ((value & 127u) * OB_PDEF[p].max + 63u) / 127u;
    (void)k;
    obv_set(t, tr, p, v);
    return 1;
}

/* ---- the voices ---- */
typedef struct { uint32_t stage; int32_t level; } obv_env_t;   /* level Q24 */
typedef struct {
    obv_osc_t o1, o2;
    uint32_t noise;
    obv_env_t fe, ae;
    obv_svf_state_t f1;
    obv_svf_coef_t c1;
    obv_lad_state_t lad;
    obv_lad_coef_t lc;
    uint32_t inc1, inc2, pw1, pw2;
    int32_t amp0, amp1;
    int32_t dcq;                  /* the DC blocker's state (Q10) */
    int32_t pitch16, target16;
    int32_t drift;
    uint32_t lfo_blk;
    int32_t lfo_fade;
    uint8_t note, vel, gate, live;
} obv_v_t;
typedef struct {                  /* the part's numbers for one 16 frame block */
    int32_t lfo_val;
    int32_t fenv_amt, keytrk, velflt, lfo_pd, lfo_fd, lfo_pwd, lfo_ad, wheel_q, env_o2, det_fine8;
    int32_t lvl1, lvl2, lvln, drive_q12, reso_k, lad_k, vint, sus_f, sus_a, gain_q12, morph_q;
    uint32_t pw1_base, pw2_base, w1, w2, ftype, sync_on, xmod_q, glide_k, lfo_ph;
} obv_g_t;
typedef struct {                  /* a part's engine memory */
    obv_g_t g[2];                 /* the two blocks of a host block */
    uint32_t lfo_ph, lfo_rnd_a, lfo_rnd_b, rnd;
    int32_t cut_sm, reso_sm;      /* CUTOFF and RESO slewed (Q8) */
    int32_t last_pitch16;         /* where a glide starts from */
    uint8_t pv[OB_NPARAM];        /* the parameters as this block sees them: the patch with the EDIT values in */
    uint8_t started;
    obv_v_t v[NVOICE];
} obv_mem_t;

static obv_mem_t *obv_of(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk);
    return p < NPART ? (obv_mem_t *)ENG_MEM(p) : 0;
}

static const int16_t OBV_VINT_TUNE8[8] = {-20, 12, 7, -15, 18, -9, 3, -17};            /* +-8 cents */
static const int16_t OBV_VINT_CUT8[8] = {-300, 250, 120, -380, 60, 330, -180, 220};     /* +-1.5 semitones */
static const int8_t OBV_VINT_ENV[8] = {10, -6, 14, -12, 3, -16, 8, -4};                 /* /128: +-12 % on the times */

static inline void obv_env_tick(obv_env_t *e, uint32_t a, uint32_t d, int32_t sus, uint32_t r, int32_t gate, int32_t vint)
{
    int32_t k, target;
    if (gate && e->stage >= OBV_E_REL) e->stage = OBV_E_ATT;
    if (!gate && e->stage && e->stage < OBV_E_REL) e->stage = OBV_E_REL;
    switch (e->stage) {
    case OBV_E_ATT:
        k = (int32_t)OBV_ENV_K_ATT[a];
        k += (int32_t)(((int64_t)k * vint) >> 7);
        target = OBV_ENV_ONE + (OBV_ENV_ONE * 3 >> 3);   /* aims at 1.3, stops at 1.0 */
        e->level += (int32_t)(((int64_t)(target - e->level) * k) >> 24);
        if (e->level >= OBV_ENV_ONE) { e->level = OBV_ENV_ONE; e->stage = OBV_E_DEC; }
        break;
    case OBV_E_DEC:
        k = (int32_t)OBV_ENV_K_DEC[d];
        k += (int32_t)(((int64_t)k * vint) >> 7);
        e->level += (int32_t)(((int64_t)(sus - e->level) * k) >> 24) - 16;
        if (e->level <= sus + 64) { e->level = sus; e->stage = OBV_E_SUS; }
        break;
    case OBV_E_SUS:
        e->level = sus;
        break;
    case OBV_E_REL:
        k = (int32_t)OBV_ENV_K_DEC[r];
        k += (int32_t)(((int64_t)k * vint) >> 7);
        e->level += (int32_t)(((int64_t)(0 - e->level) * k) >> 24) - 32;
        if (e->level < OBV_LEVEL_FLOOR) { e->level = 0; e->stage = OBV_E_IDLE; }
        break;
    default:
        e->level = 0;
    }
}

static int32_t obv_lfo_shape(const obv_mem_t *m, uint32_t ph, uint32_t shape)
{
    int32_t p;
    switch (shape) {
    case OB_L_TRI:
        p = (int32_t)((ph + 0x40000000u) >> 15);
        return p < 65536 ? p - 32768 : 98303 - p;
    case OB_L_SAW: return 32767 - (int32_t)(ph >> 16);
    case OB_L_RAMP: return (int32_t)(ph >> 16) - 32768;
    case OB_L_SQR: return ph < 0x80000000u ? 32767 : -32767;
    case OB_L_SH: return (int32_t)(m->lfo_rnd_b >> 16) - 32768;
    default: {                                   /* smooth random: a line from one random value to the next */
        int32_t a = (int32_t)(m->lfo_rnd_a >> 16) - 32768, b = (int32_t)(m->lfo_rnd_b >> 16) - 32768;
        return a + (int32_t)(((int64_t)(b - a) * (ph >> 8)) >> 24);
    }
    }
}

/* the part's numbers for one 16 frame block (OberVave's lfo_tick and block_globals) */
static void obv_part_block(obv_mem_t *m, obv_g_t *g)
{
    const uint8_t *pv = m->pv;
    uint32_t inc = OBV_LFO_INC[pv[OB_P_LFO_RATE]], old = m->lfo_ph;
    int32_t d;
    m->lfo_ph += inc;
    if (m->lfo_ph < old) { m->lfo_rnd_a = m->lfo_rnd_b; m->lfo_rnd_b = obv_xorshift(&m->rnd); }
    g->lfo_ph = m->lfo_ph;
    g->lfo_val = obv_lfo_shape(m, m->lfo_ph, pv[OB_P_LFO_SHAPE]);
    g->fenv_amt = (int32_t)pv[OB_P_FENV_AMT] - 64;
    g->keytrk = pv[OB_P_KEYTRK];
    g->velflt = pv[OB_P_VEL_FLT];
    d = pv[OB_P_LFO_PITCH]; g->lfo_pd = (d * d) >> 7;
    d = pv[OB_P_LFO_FLT]; g->lfo_fd = (d * d) >> 7;
    d = pv[OB_P_LFO_PWM]; g->lfo_pwd = (d * d) >> 7;
    g->lfo_ad = pv[OB_P_LFO_AMP];
    g->wheel_q = 0;                              /* (the mod wheel is the host's) */
    g->env_o2 = (int32_t)pv[OB_P_ENV_O2] - 64;
    g->det_fine8 = ((int32_t)pv[OB_P_O2_FINE] - 64) * 2;
    d = pv[OB_P_O1_LVL]; g->lvl1 = (d * d) >> 7;
    d = pv[OB_P_O2_LVL]; g->lvl2 = (d * d) >> 7;
    d = pv[OB_P_NOISE]; g->lvln = (d * d) >> 7;
    d = pv[OB_P_DRIVE]; g->drive_q12 = 4096 + (d * d * 15 * 4096 / 16129);
    m->cut_sm += ((pv[OB_P_CUTOFF] << 8) - m->cut_sm) >> 6;
    m->reso_sm += ((pv[OB_P_RESO] << 8) - m->reso_sm) >> 6;
    g->reso_k = 8192 - m->reso_sm * 8400 / (127 * 256);
    g->lad_k = (8192 - g->reso_k) >> 1;
    g->vint = pv[OB_P_VINTAGE];
    g->sus_f = pv[OB_P_FS] * (OBV_ENV_ONE / 127);
    g->sus_a = pv[OB_P_AS] * (OBV_ENV_ONE / 127);
    d = pv[OB_P_VOLUME];
    g->gain_q12 = (d * d) >> 2;
    g->pw1_base = ((uint32_t)pv[OB_P_O1_PW] << 25) + (1u << 24);
    g->pw2_base = ((uint32_t)pv[OB_P_O2_PW] << 25) + (1u << 24);
    g->w1 = pv[OB_P_O1_WAVE];
    g->w2 = pv[OB_P_O2_WAVE];
    g->ftype = pv[OB_P_FTYPE];
    g->morph_q = pv[OB_P_FMORPH];
    g->sync_on = pv[OB_P_SYNC];
    g->xmod_q = (uint32_t)(((int32_t)pv[OB_P_XMOD] * pv[OB_P_XMOD]) >> 7);
    g->glide_k = OBV_GLIDE_K[pv[OB_P_GLIDE]];
}

static inline uint32_t obv_pw_mod(uint32_t base, int32_t mod)
{
    int64_t p = (int64_t)base + ((int64_t)mod << 8);
    if (p < (1 << 26)) p = 1 << 26;
    if (p > (int64_t)0xFFFFFFFFu - (1 << 26)) p = (int64_t)0xFFFFFFFFu - (1 << 26);
    return (uint32_t)p;
}

/* one 16 frame block of a voice: pitch, cutoff, envelopes, amplitude (OberVave's voice_block). hostp: the part's pitch,
 * Q16 semitones, with the host's bend / tune / vibrato / unison detune (fine8: the residual, Q8 semitones) */
static void obv_voice_block(obv_mem_t *m, obv_v_t *v, const obv_g_t *g, uint32_t card, int32_t hostp, int32_t fine8)
{
    const uint8_t *pv = m->pv;
    int32_t lfo, p8, p1, p2, cut, fenv, tmp, velg, trem, vint = g->vint, vt = (vint * OBV_VINT_ENV[card & 7u]) >> 7, lfo_to, a;
    uint32_t ph, glide_k = g->glide_k;
    v->target16 = hostp;                           /* the glide: OberVave's, from the last pitch to the part's */
    if (v->pitch16 != v->target16) {
        if (!glide_k) v->pitch16 = v->target16;
        else {
            int32_t diff = v->target16 - v->pitch16;
            v->pitch16 += (int32_t)(((int64_t)diff * glide_k) >> 16);
            if (diff > -64 && diff < 64) v->pitch16 = v->target16;
        }
    }
    obv_env_tick(&v->fe, pv[OB_P_FA], pv[OB_P_FD], g->sus_f, pv[OB_P_FR], v->gate, vt);
    obv_env_tick(&v->ae, pv[OB_P_AA], pv[OB_P_AD], g->sus_a, pv[OB_P_AR], v->gate, vt);
    fenv = v->fe.level >> 9;                       /* Q15 */
    ph = g->lfo_ph;
    if (pv[OB_P_LFO_PHASE] == OB_LP_SPREAD) ph += card << 29;
    lfo = pv[OB_P_LFO_PHASE] == OB_LP_SPREAD ? obv_lfo_shape(m, ph, pv[OB_P_LFO_SHAPE]) : g->lfo_val;
    if (v->lfo_fade < 32767) {
        uint32_t dl = OBV_LFO_DELAY_BLK[pv[OB_P_LFO_DELAY]];
        if (v->lfo_blk >= dl) { v->lfo_fade += 32767 / (int32_t)(dl / 2u + 1u) + 1; if (v->lfo_fade > 32767) v->lfo_fade = 32767; }
        v->lfo_blk++;
    }
    lfo = (lfo * v->lfo_fade) >> 15;
    v->drift += (int32_t)(obv_xorshift(&v->noise) & 31u) - 16;
    v->drift -= v->drift >> 11;
    p8 = (v->pitch16 >> 8) + fine8 + ((OBV_VINT_TUNE8[card & 7u] * vint) >> 7) + ((v->drift * vint) >> 10);
    lfo_to = (lfo * g->lfo_pd * 24) >> 15;
    p1 = p8 + (pv[OB_P_LFO_PITCH_TO] != OB_TO_O2 ? lfo_to : 0);
    p2 = p8 + (pv[OB_P_LFO_PITCH_TO] != OB_TO_O1 ? lfo_to : 0) + ((int32_t)pv[OB_P_O2_SEMI] - 24) * 256 + g->det_fine8
       + ((fenv * g->env_o2 * 97) >> 15);
    v->inc1 = obv_pitch_inc(p1);
    v->inc2 = obv_pitch_inc(p2);
    tmp = (lfo * g->lfo_pwd);
    v->pw1 = obv_pw_mod(g->pw1_base, pv[OB_P_LFO_PWM_TO] != OB_TO_O2 ? tmp : 0);
    v->pw2 = obv_pw_mod(g->pw2_base, pv[OB_P_LFO_PWM_TO] != OB_TO_O1 ? tmp : 0);
    cut = m->cut_sm + 8 * 256 + ((fenv * g->fenv_amt * 390) >> 15)
        + ((int32_t)v->note - 60) * g->keytrk * 2 + (((int32_t)v->vel - 64) * g->velflt >> 1)
        + ((lfo * g->lfo_fd * 97) >> 15) + ((OBV_VINT_CUT8[card & 7u] * vint) >> 7) + ((v->drift * vint) >> 7);
    if (g->ftype == OB_F_LP24) obv_lad_coef(&v->lc, cut, g->lad_k);
    else obv_svf_coef(&v->c1, cut, g->reso_k);
    velg = 32767 - ((((127 - (int32_t)v->vel) * (int32_t)pv[OB_P_VEL_AMP]) >> 7) * 258);
    trem = 32767 - (((32767 - lfo) * g->lfo_ad) >> 8);
    v->amp0 = v->amp1;
    a = (int32_t)(((int64_t)(v->ae.level >> 9) * velg >> 15) * trem >> 15);
    v->amp1 = (a * g->gain_q12) >> 12;
}

/* the samples of one 16 frame block: oscillators -> mix -> input stage (drive) -> filter -> amplifier, MONO into out */
static void obv_voice_render(obv_v_t *v, const obv_g_t *g, int32_t *out, uint32_t n, const vmod_t *hm, uint32_t first)
{
    uint32_t i, inc1 = v->inc1, inc2 = v->inc2, pw1 = v->pw1, pw2 = v->pw2, d1 = inc1 >> 15, d2 = inc2 >> 15;
    int32_t amp0 = v->amp0, da = v->amp1 - v->amp0, o2n = v->o2.prev;
    int32_t l1 = g->lvl1, l2 = g->lvl2, ln = g->lvln, drv = g->drive_q12, morph_q = g->morph_q;
    uint32_t ft = g->ftype, xq = g->xmod_q, sy = g->sync_on, ww1 = g->w1, ww2 = g->w2;
    int32_t hw1 = (ww1 & OBV_OSC_SAW ? -65536 : 0) + (ww1 & OBV_OSC_PLS ? -65536 : 0);
    int32_t hw2 = (ww2 & OBV_OSC_SAW ? -65536 : 0) + (ww2 & OBV_OSC_PLS ? -65536 : 0);
    for (i = 0; i < n; i++) {
        int32_t s1, s2, mix, lp, bp, hp, y, amp, rt = -1;
        uint32_t ie = inc1;
        if (xq) {                               /* OSC2 -> OSC1 linear FM, the step never below inc/16 */
            int32_t mo = (int32_t)(inc1 >> 8 > 0x200000u ? 0x200000u : inc1 >> 8) * (((o2n >> 8) * (int32_t)xq) >> 6);
            mo += (int32_t)inc1;
            ie = mo < (int32_t)(inc1 >> 4) ? inc1 >> 4 : (uint32_t)mo;
            d1 = ie >> 15;
        }
        s1 = obv_osc_step(&v->o1, ie, d1, pw1, ww1, hw1, -1);
        if (sy && v->o1.ph < ie && d1)
            rt = (int32_t)(v->o1.ph / d1);
        s2 = obv_osc_step(&v->o2, inc2, d2, pw2, ww2, hw2, rt);
        o2n = v->o2.prev;
        mix = s1 * l1 + s2 * l2;
        if (ln) mix += ((int32_t)(obv_xorshift(&v->noise) >> 16) - 32768) * ln;
        mix >>= 7;
        mix = obv_sat_in(((mix >> 3) * drv) >> 9);
        if (ft == OB_F_LP24)
            y = obv_lad_step(&v->lc, &v->lad, mix);
        else {
            obv_svf_step(&v->c1, &v->f1, mix, &lp, &bp, &hp);
            switch (ft) {
            case OB_F_HP12: y = hp; break;
            case OB_F_BP12: y = bp; break;
            case OB_F_NOTCH: y = hp + lp; break;
            case OB_F_SEM: y = lp + (((hp - lp) * morph_q) >> 7); break;
            default: y = lp; break;
            }
        }
        y = obv_sat_knee(y);
        amp = amp0 + ((da * (int32_t)i) >> 4);
        y = (y * amp) >> 16;
        v->dcq += y - (v->dcq >> 10);           /* a DC blocker, 7 Hz: the saturation of a hard-driven voice leaves an offset that moves with its envelope */
        y -= v->dcq >> 10;                      /* (the state keeps ten more bits: with nothing in, it settles on exactly 0 out) */
        out[i] += y;                            /* (the pan is the host's: PAN, SPREAD) */
    }
    (void)hm; (void)first;
}

/* ---- the engine ---- */
static void obv_block(track_t *t)                /* once a block and part, before its voices */
{
    uint32_t tr = (uint32_t)(t - trk), k;
    obv_mem_t *m = obv_of(t);
    uint8_t slot;
    if (tr >= NTRK || !m)
        return;
    obv_touch(tr);
    /* PROGRAM (E7): a new one loads the patch; one of the seven edited makes the sound the track's own (OWN, 0) */
    slot = (uint8_t)clamp(t->p[P_E7], 0, (int)OBV_NFACTORY);
    if (slot != obv_slot_seen[tr]) {
        obv_slot_seen[tr] = slot;
        if (slot)
            obv_load_program(t, tr, (uint32_t)slot - 1u);
        else
            for (k = 0; k < 7u; k++)
                obv_e_seen[tr][k] = (uint8_t)clamp(t->p[P_E0 + k], 0, 127);
    } else if (slot) {
        for (k = 0; k < 7u; k++)
            if (clamp(t->p[P_E0 + k], 0, 127) != obv_e_seen[tr][k]) {
                t->p[P_E7] = 0;                  /* edited: OWN */
                obv_slot_seen[tr] = 0;
                break;
            }
        for (k = 0; k < 7u; k++)
            obv_e_seen[tr][k] = (uint8_t)clamp(t->p[P_E0 + k], 0, 127);
    }
    /* this block's parameters: the patch with the EDIT values in */
    memcpy(m->pv, obv_patch[tr], OB_NPARAM);
    for (k = 0; k < 7u; k++)
        m->pv[OBV_EMAP[k]] = (uint8_t)clamp(t->p[P_E0 + k], 0, OB_PDEF[OBV_EMAP[k]].max);
    if (!m->started) {
        m->started = 1;
        m->rnd = 0x9E3779B9u + tr;
        m->cut_sm = (int32_t)m->pv[OB_P_CUTOFF] << 8;
        m->reso_sm = (int32_t)m->pv[OB_P_RESO] << 8;
        m->last_pitch16 = 60 << 16;
    }
    obv_part_block(m, &m->g[0]);
    obv_part_block(m, &m->g[1]);
}

static void obv_note_on(track_t *t, voice_t *v)
{
    obv_mem_t *m = obv_of(t);
    uint32_t idx = (uint32_t)(v - t->v), j;
    obv_v_t *o;
    const uint8_t *pv;
    if (!m || idx >= NVOICE)
        return;
    if (!m->started)
        obv_block(t);
    o = &m->v[idx];
    pv = m->pv;
    o->note = v->note;
    o->vel = v->vel;
    o->gate = 1;
    o->live = 1;
    o->target16 = (int32_t)v->pitch_cur * 4096;            /* 1/16 semitone -> Q16 */
    o->pitch16 = pv[OB_P_GLIDE] ? m->last_pitch16 : o->target16;
    o->fe.stage = OBV_E_ATT;
    o->ae.stage = OBV_E_ATT;
    o->lfo_blk = 0;
    o->lfo_fade = pv[OB_P_LFO_DELAY] ? 0 : 32767;
    if (o->noise == 0u)
        o->noise = 0x2545F491u * (idx + 1u + (uint32_t)(t - trk) * 16u);
    if (pv[OB_P_LFO_PHASE] == OB_LP_SYNC) {                 /* the first key of a chord restarts the LFO */
        for (j = 0; j < NVOICE; j++)
            if (j != idx && t->v[j].active && t->v[j].gate)
                break;
        if (j == NVOICE)
            m->lfo_ph = 0;
    }
    m->last_pitch16 = o->target16;
}

static int obv_done(track_t *t, voice_t *v)
{
    obv_mem_t *m = obv_of(t);
    uint32_t idx = (uint32_t)(v - t->v);
    obv_v_t *o;
    if (!m || idx >= NVOICE)
        return 1;
    o = &m->v[idx];
    if (!o->live)
        return 1;
    if (v->gate || o->gate)
        return 0;
    if (o->ae.stage != OBV_E_IDLE)
        return 0;
    o->live = 0;
    return 1;
}

static void obv_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *hm)
{
    obv_mem_t *m = obv_of(t);
    uint32_t idx = (uint32_t)(v - t->v), off, sl = 0;
    obv_v_t *o;
    int32_t hostp, fine8;
    int32_t buf[OBV_CTL];
    if (!m || idx >= NVOICE)
        return;
    o = &m->v[idx];
    if (!o->live)
        return;
    o->gate = v->gate;
    hostp = hm->pitch16 * 4096;                              /* Q16 semitones: the part's pitch with the host's bend, tune, vibrato */
    fine8 = (hm->fine * 277) >> 8;                           /* the residual (1/4096 of an increment) as Q8 semitones */
    for (off = 0; off < n; off += OBV_CTL, sl++) {
        uint32_t c = n - off < OBV_CTL ? n - off : OBV_CTL, i;
        const obv_g_t *g = &m->g[sl & 1u];
        obv_voice_block(m, o, g, idx, hostp, fine8);
        memset(buf, 0, sizeof buf);
        obv_voice_render(o, g, buf, c, hm, off);
        for (i = 0; i < c; i++)
            out[off + i] += voice_amp(buf[i], hm, off + i) * 2;  /* the voice level (OBV_OUT) and the part's fades */
    }
}

static const engine_t ENG_OBERV = {
    .name = "OBERV",
    .page_title = {"OBERV", "VOICE"},
    .edit = {
        {"CUTOFF", F_INT, 0, 127, 80, 0, 0},
        {"RESO", F_INT, 0, 127, 20, 0, 0},
        {"ENV AMT", F_INT, 0, 127, 90, 0, 0},
        {"F.DECAY", F_INT, 0, 127, 70, 0, 0},
        {"A.ATTACK", F_INT, 0, 127, 10, 0, 0},
        {"A.RELEAS", F_INT, 0, 127, 60, 0, 0},
        {"DRIVE", F_INT, 0, 127, 0, 0, 0},
        {"PROGRAM", F_ENUM, 0, OBV_NFACTORY, 1, OBV_NAMES, 0},
    },
    .presets = OBV_PRESETS,
    .npresets = OBV_NFACTORY,
    .note_on = obv_note_on,
    .render = obv_render,
    .knob = {P_E0, P_E1, P_E2, P_E7},
    .poly = 0,
    .block = obv_block,
    .ownenv = 1,
    .done = obv_done,
    .mem = sizeof(obv_mem_t),
};
