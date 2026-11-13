/* SPDX-License-Identifier: GPL-3.0-only
 * SYNTHEX: Scintilla's Elka Synthex voice as an engine (Scintilla, Joakim Eriksson, GPL-3.0-only,
 * https://github.com/joakimeriksson/scintilla-fm1; the Synthex of synthex-web in integers, checked sample by sample against
 * it; engine/sc_voice.c, sc_patch.h, sc_presets.h through tools/gen_synthex.py -> scx_*.{h,c}, eng_synthex_presets.h).
 *
 * A voice: two PolyBLEP oscillators (sync, ring modulation, cross modulation, PWM, glide), noise, a zero-delay-feedback
 * filter in five modes, two envelopes, two LFOs, the performance paths Scintilla chooses per patch. A Synthex patch has two
 * LAYERS (upper, lower): SINGLE plays the upper one, DOUBLE both, SPLIT one or the other by the key. Its 80 programs are the
 * Synthex's own: the 40 PRESETS (decoded from its EPROMs) and 40 MEMORIES (Paul Wiffen's Laser Harp "Ring mod." among them).
 *
 * What changed in the port (Scintilla's synth is a whole instrument: layers of voices, the voice allocation, a chorus a layer,
 * a reverb, a limiter, a joystick):
 *   - the host allocates the voices (POLY / MONO / LEGATO / UNISON, the unison detune, the glide of the key, the bend, the
 *     vibrato): a host voice is one Synthex voice, or two in a DOUBLE patch. The patch's key assignment sets the host's voice
 *     mode when a program is chosen. The part's pitch is the host's (the voice's bend).
 *   - the chorus, delay and reverb are the track's sends (the patch's mixes set them when a program is chosen): Scintilla's
 *     own effects are not here. The joystick stays at its centre.
 *   - the amplitude is the voice's own (engine_t.ownenv).
 *   - the program (E7, 1..80) is decoded into the part's engine memory; the six EDIT values over it are what is kept in a
 *     project or a user preset (the rest of a patch is its program's): see eng_synthex_map.h. Choosing a program sets the
 *     EDIT values to its own, turning one is a step from them on both layers.
 * Scintilla's engine is the heaviest of this firmware: its README measures 22..70 % of a core for eight voices, a DOUBLE patch
 * twice that. The host voices are capped (.poly). */
#include "eng_synthex_map.h"
#include "scx_voice.c"
#include "scx_presets.h"
#include "eng_synthex_presets.h"

#define ENGI_SYNTHEX 17u        /* engines.c ENGINES[] (append-only) */
#define SYNX_POLY 6u            /* host voices in POLY / UNISON: each plays one or two Synthex voices */
#define SYNX_OUT_SHIFT 5        /* Scintilla's voices are Q20 (the instrument's bus); the host's 24000 full scale is Q15 */

typedef struct {
    scx_voice_t v[2];
    uint8_t note, vel, gate[2], on[2], init;
} synx_v_t;
typedef struct {
    scx_patch_t patch;            /* the program as decoded, the EDIT values laid over it */
    scx_prep_t prep[2];
    scx_perf_t perf;
    uint8_t base[7];              /* the program's own EDIT values */
    uint8_t cur[7];               /* the EDIT values the patch has now */
    uint8_t prog;                 /* the program decoded (0 = none yet) */
    uint8_t e_seen[7];            /* E0..E6 as of the last block */
    uint8_t started;
    synx_v_t vs[NVOICE];
} synx_mem_t;
static uint8_t synx_slot_seen[NTRK] = {255, 255, 255, 255};   /* the E7 value last acted on (255: none yet) */

static synx_mem_t *synx_of(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk);
    return p < NPART ? (synx_mem_t *)ENG_MEM(p) : 0;
}

static void synx_prepare(synx_mem_t *m)
{
    scx_prep(&m->prep[0], &m->patch.layer[0], &m->perf);
    scx_prep(&m->prep[1], &m->patch.layer[1], &m->perf);
}
/* the program p (1..80) decoded, the EDIT values e laid over it */
static void synx_build(synx_mem_t *m, uint32_t prog, const uint8_t e[7])
{
    scx_preset_load(prog - 1u, &m->patch);
    synx_e_from_patch(&m->patch, m->base);
    memcpy(m->cur, e, 7);
    synx_apply(&m->patch, m->base, e);
    m->prog = (uint8_t)prog;
    synx_prepare(m);
}

static void synx_block(track_t *t)               /* once a block and part */
{
    uint32_t tr = (uint32_t)(t - trk), k, same = 1, chg = 0;
    synx_mem_t *m = synx_of(t);
    uint8_t e[7];
    int32_t slot;
    if (tr >= NTRK || !m)
        return;
    slot = clamp(t->p[P_E7], 1, (int)SYNX_NPROG);
    for (k = 0; k < 7u; k++) {
        e[k] = (uint8_t)clamp(t->p[P_E0 + k], 0, 127);
        same &= e[k] == m->e_seen[k];
    }
    if (!m->started) {
        m->started = 1;
        memset(&m->perf, 0, sizeof m->perf);                /* the joystick at its centre (Scintilla's sc_perf_default) */
        m->perf.bend_osc = m->perf.bend_filt = SCX_Q16(0.3);
        m->perf.lfo2_osc = m->perf.lfo2_filt = SCX_Q16(0.5);
        synx_slot_seen[tr] = 255;
        m->prog = 0;
    }
    if (synx_slot_seen[tr] != (uint8_t)slot || !m->prog) {
        int user = synx_slot_seen[tr] != 255u && same;      /* PROGRAM turned (the EDIT values still the last block's) */
        synx_slot_seen[tr] = (uint8_t)slot;
        if (user) {                                         /* a new program: its own values, voice mode and sends */
            synx_map_t mp;
            scx_preset_load((uint32_t)slot - 1u, &m->patch);
            synx_e_from_patch(&m->patch, e);
            synx_host_from_patch(&m->patch, &mp);
            for (k = 0; k < 7u; k++)
                t->p[P_E0 + k] = e[k];
            t->p[P_VOICE] = mp.vmode == 2u ? V_UNISON : mp.vmode == 1u ? V_LEGATO : V_POLY;
            t->p[P_CHOR] = mp.cho;
            t->p[P_DLY] = mp.dly;
            t->p[P_REV] = mp.rev;
        }
        synx_build(m, (uint32_t)slot, e);                   /* (a restored project, a preset: the EDIT values as they are) */
        chg = 1;
    } else {
        for (k = 0; k < 7u; k++)
            chg |= e[k] != m->cur[k];
        if (chg)
            synx_build(m, (uint32_t)slot, e);
    }
    for (k = 0; k < 7u; k++)
        m->e_seen[k] = (uint8_t)clamp(t->p[P_E0 + k], 0, 127);
}

static int32_t synx_vel_q16(uint32_t vel) { return (int32_t)(((uint32_t)clamp((int)vel, 1, 127) * 65536u + 63u) / 127u); }

/* the layers a key plays */
static uint32_t synx_route(const synx_mem_t *m, uint32_t note)
{
    switch (m->patch.voice_mode) {
    case SCX_VM_DOUBLE: return 3u;
    case SCX_VM_SPLIT: return note >= (uint32_t)m->patch.split ? 1u : 2u;
    default: return 1u;
    }
}

static void synx_note_on(track_t *t, voice_t *v)
{
    synx_mem_t *m = synx_of(t);
    uint32_t idx = (uint32_t)(v - t->v), r, k;
    synx_v_t *o;
    if (!m || idx >= NVOICE)
        return;
    if (!m->started || !m->prog)
        synx_block(t);
    o = &m->vs[idx];
    if (!o->init) {
        scx_voice_init(&o->v[0], 0x9E3779B9u + idx);
        scx_voice_init(&o->v[1], 0x7F4A7C15u + idx);
        o->init = 1;
    }
    r = synx_route(m, v->note);
    o->note = v->note;
    o->vel = v->vel;
    for (k = 0; k < 2u; k++) {
        o->on[k] = (uint8_t)((r >> k) & 1u);
        o->gate[k] = o->on[k];
        if (o->on[k])
            scx_voice_note_on(&o->v[k], &m->prep[k], v->note, synx_vel_q16(v->vel));
    }
}

static int synx_done(track_t *t, voice_t *v)
{
    synx_mem_t *m = synx_of(t);
    uint32_t idx = (uint32_t)(v - t->v), k;
    synx_v_t *o;
    if (!m || idx >= NVOICE)
        return 1;
    o = &m->vs[idx];
    if (v->gate)
        return 0;
    for (k = 0; k < 2u; k++)
        if (o->on[k] && !scx_voice_idle(&o->v[k]))
            return 0;
    o->on[0] = o->on[1] = 0;
    return 1;
}

static void synx_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *hm)
{
    synx_mem_t *m = synx_of(t);
    uint32_t idx = (uint32_t)(v - t->v), k, i, c;
    synx_v_t *o;
    int32_t bend;
    int32_t buf[32];
    if (!m || idx >= NVOICE)
        return;
    o = &m->vs[idx];
    bend = ((hm->pitch16 - (int32_t)v->note * 16) << 12) + (((hm->fine * 277) >> 8) << 8);   /* semitones, Q16 */
    for (k = 0; k < 2u; k++) {
        if (!o->on[k])
            continue;
        if (!v->gate && o->gate[k]) {                       /* the key let go */
            o->gate[k] = 0;
            scx_voice_note_off(&o->v[k], &m->prep[k]);
        } else if (v->gate && o->gate[k] && v->note != o->note) {   /* the host moved a held voice to another key (LEGATO) */
            o->note = v->note;
            scx_voice_note_on(&o->v[k], &m->prep[k], v->note, synx_vel_q16(o->vel));
        }
        o->v[k].bend = bend;
        for (c = 0; c < n; c += 32u) {
            uint32_t len = n - c < 32u ? n - c : 32u;
            for (i = 0; i < len; i++)
                buf[i] = 0;
            scx_voice_render(&o->v[k], &m->prep[k], buf, len);
            for (i = 0; i < len; i++)
                out[c + i] += voice_amp(buf[i] >> SYNX_OUT_SHIFT, hm, c + i);
        }
    }
}

static const engine_t ENG_SYNTHEX = {
    .name = "SYNTHEX",
    .page_title = {"SYNTHEX", "VOICE"},
    .edit = {
        {"CUTOFF", F_INT, 0, 127, 64, 0, 0},
        {"RESO", F_INT, 0, 127, 20, 0, 0},
        {"ENV AMT", F_INT, 0, 127, 64, 0, 0},
        {"A.ATTACK", F_INT, 0, 127, 20, 0, 0},
        {"A.RELEASE", F_INT, 0, 127, 40, 0, 0},
        {"F.DECAY", F_INT, 0, 127, 60, 0, 0},
        {"VOLUME", F_INT, 0, 127, 100, 0, 0},
        {"PROGRAM", F_ENUM, 0, SYNX_NPROG, 1, SYNX_NAMES, 0},
    },
    .presets = SYNX_PRESETS,
    .npresets = SYNX_NPROG,
    .note_on = synx_note_on,
    .render = synx_render,
    .knob = {P_E0, P_E1, P_E2, P_E7},
    .poly = SYNX_POLY,
    .block = synx_block,
    .ownenv = 1,
    .done = synx_done,
    .mem = sizeof(synx_mem_t),
};
