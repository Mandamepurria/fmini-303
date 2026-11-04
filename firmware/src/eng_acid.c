/* SPDX-License-Identifier: GPL-3.0-only
 * ACID: a 303-style bass voice. The DSP is Smileyface's (pbarilla, GPL-3.0-only, firmware/src/voice.c; built on
 * Chipbox, Dinghy and Felucca): one oscillator (saw or square, polyBLEP), a four-pole ladder with a squashed
 * feedback, an RC-shaped filter envelope that decays, accent and slide. Integers only, as the rest of the firmware.
 *
 * What changed in the port (Smileyface's voice is a whole instrument; this is one engine of a part):
 *   - no sequencer, no pattern memory, no knobs page: the part is played from MIDI (or the keys / the step
 *     sequencer of this firmware) like any other engine; each MIDI channel can be assigned to a part, so one part
 *     can be ACID and another FMINI (the Minimoog) at the same time.
 *   - ACCENT: a note with velocity above 110 is accented (the same threshold Felucca uses for its accent).
 *   - SLIDE: a note that starts while the previous one is still held slides to the new pitch (MONO or LEGATO
 *     voice mode; the presets are LEGATO). The slide's speed is the SLIDE knob (an RC on the pitch, as the 303's).
 *     Leave the part's own GLIDE at 0: the engine does the slide.
 *   - the pitch follows the part's pitch bend, tuning and vibrato (vmod_t.pitch16).
 *   - the amplitude is the voice's own VCA (engine_t.ownenv): the part's ADSR is not used, velocity only
 *     chooses accent / no accent.
 *   - the part's state lives in the part's engine memory (engine_t.mem), one voice a part.
 * The eight EDIT values are the 303's front panel: E0 CUT, E1 RES, E2 ENV MOD, E3 DECAY, E4 ACCENT, E5 SLIDE,
 * E6 DRIVE (0..31 each), E7 WAVE (SAW / SQR). The other knobs of Smileyface (attack, tuning, sub, sweep, poles,
 * muffler ...) stay at its defaults.
 * Compile-time: ACID_OS = 2 (default) runs the oscillator and the ladder twice a sample (as Smileyface does),
 * 1 once (about half the cost, a little more aliasing at high resonance and drive). */
#include "eng_acid_tables.h"

#define ENGI_ACID 15u           /* engines.c ENGINES[] (append-only) */
#ifndef ACID_OS
#define ACID_OS 2
#endif
#if ACID_OS == 2
#define ACID_CUT_G ACID_CUT_G2
#define ACID_OSH 1
#else
#define ACID_CUT_G ACID_CUT_G1
#define ACID_OSH 0
#endif
#define ACID_REL_RATE 2200      /* how fast the volume closes at the end of a note (Smileyface's REL_RATE) */
#define ACID_ACC_VEL 110u       /* a velocity above this is an accented note */

typedef struct {                 /* the knobs of Smileyface's knobs_t that this engine uses, each 0..31 */
    uint8_t cut, res, env, dec, acc, sld, drv, wave, adc, trk, atk, muf, tun, sub, swp, pol, fm, hir, swg;
    int8_t lvl;
} acid_kn_t;

typedef struct {                 /* Smileyface's voice_t plus what the host adds: one per part */
    uint32_t ph, ph2, incT;
    int32_t p, tp;               /* pitch now and wanted, 1/65536 semitone (Smileyface's 1/256 stalled a rising slide short:
                                  * a step of mulq(diff, rate) rounds down to 0 once diff * rate < 32768) */
    uint32_t gate, acc, env_pos, note;
    int32_t drv, xp, dcs, acc_cap, vca, s[4], G, G2, G3, G4, omG, R, k, bl, bl2, bl3, va, dc, ci, v1, vs, idxc, fmk,
        fmy, nd0, nd1, dr0, dr1, dg0, dg1;
    uint32_t tick, rs, nz;
    uint32_t live;               /* a note was started and has not ended */
    acid_kn_t kn;                /* the knobs, taken from the part's EDIT values at each block */
} acid_t;

static inline int32_t acid_mulq(int32_t a, int32_t g)   /* a * g >> 15, g <= 32768, no 64 bit */
{
    return (a >> 15) * g + (((a & 32767) * g) >> 15);
}
static int32_t acid_soft(int32_t x)                    /* a cubic knee: x - x^3 / 3, flat past 1.0 (Q15) */
{
    if (x > 32767)
        x = 32767;
    if (x < -32767)
        x = -32767;
    return x - acid_mulq(acid_mulq(x, x), x) / 3;
}
static int32_t acid_blep(uint32_t t, uint32_t dt)      /* polyBLEP residue, Q15; t, dt are Q16 */
{
    int32_t x;
    if (!dt)
        return 0;
    if (t < dt) {
        x = (int32_t)((t << 15) / dt);
        return 2 * x - acid_mulq(x, x) - 32768;
    }
    if (t > 65536u - dt) {
        x = (int32_t)(((int32_t)t - 65536) * 32768 / (int32_t)dt);
        return acid_mulq(x, x) + 2 * x + 32768;
    }
    return 0;
}

static acid_t *acid_of(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk);
    return p < NPART ? (acid_t *)ENG_MEM(p) : 0;
}

/* Smileyface's defaults (knobs_t kn), but for the eight the EDIT values set */
static void acid_knobs(const track_t *t, acid_kn_t *kn)
{
    const int16_t *p = t->p;
    kn->cut = (uint8_t)clamp(p[P_E0], 0, 31);
    kn->res = (uint8_t)clamp(p[P_E1], 0, 31);
    kn->env = (uint8_t)clamp(p[P_E2], 0, 31);
    kn->dec = (uint8_t)clamp(p[P_E3], 0, 31);
    kn->acc = (uint8_t)clamp(p[P_E4], 0, 31);
    kn->sld = (uint8_t)clamp(p[P_E5], 0, 31);
    kn->drv = (uint8_t)clamp(p[P_E6], 0, 31);
    kn->wave = (uint8_t)(p[P_E7] & 1);          /* bit 0 square; accents on, no bass / drift / diode / two poles */
    kn->adc = 4;
    kn->trk = 12;
    kn->atk = 2;
    kn->muf = 0;
    kn->tun = 16;
    kn->sub = 0;
    kn->swp = 12;
    kn->pol = 31;
    kn->fm = 0;
    kn->hir = 0;
    kn->swg = 0;
    kn->lvl = 0;
}

static void acid_gate_on(acid_t *a, uint32_t note, uint32_t acc, uint32_t slide_in)
{
    const acid_kn_t *kn = &a->kn;
    a->tp = (int32_t)(note < 127u ? note : 126u) << 16;
    a->note = note;
    if (!slide_in || !a->gate || !a->p)
        a->p = a->tp;
    if (!slide_in || !a->gate) {
        a->env_pos = 0;
        a->va = 1 << 24;
        a->rs = a->rs * 1664525u + 1013904223u;
        a->nd0 = (int32_t)(a->rs >> 16) - 32768;
        a->rs = a->rs * 1664525u + 1013904223u;
        a->nd1 = (int32_t)(a->rs >> 16) - 32768;
    } else {
        a->va = 1 << 24;                         /* a slid note keeps its filter sweep, its volume is plucked again */
    }
    a->acc = acc;
    a->acc_cap += acc ? ((kn->swp < 8u ? 65534 : 32767) - a->acc_cap) / 3 : 0;
    a->gate = 1;
    a->live = 1;
}

static void acid_note_on(track_t *t, voice_t *v)
{
    acid_t *a = acid_of(t);
    if (!a)
        return;
    acid_knobs(t, &a->kn);
    /* a note that starts while the last one is still held is a slide (the host sets v->gate before this call, the
     * engine's own gate is as the last block left it) */
    acid_gate_on(a, v->note, v->vel > ACID_ACC_VEL, a->gate && a->live);
}

static int acid_done(track_t *t, voice_t *v)            /* the voice's VCA has closed after the gate went off */
{
    acid_t *a = acid_of(t);
    if (!a || !a->live)
        return 1;
    if (v->gate || a->gate)
        return 0;
    if (a->vca > 40 || a->v1 > 40 || a->vs > 40)
        return 0;
    a->live = 0;
    return 1;
}

static int32_t acid_core_step(acid_t *a)                /* one step of the oscillator and the ladder; x out (Q19) */
{
    const acid_kn_t *kn = &a->kn;
    int32_t s, x, x2, fb;
    uint32_t t, dt;
    if (a->fmk) {                                       /* FILT FM (off: kn.fm is 0) */
        int32_t m = ((a->fmy >> 6) * a->fmk) >> 9, ii = a->idxc * 16 + m, g, fr;
        if (ii < 0)
            ii = 0;
        if (ii > 255 * 16)
            ii = 255 * 16;
        fr = ii & 15;
        ii >>= 4;
        g = ACID_CUT_G[ii] + (((int32_t)ACID_CUT_G[ii < 255 ? ii + 1 : 255] - (int32_t)ACID_CUT_G[ii]) * fr >> 4);
        a->G = g;
        a->G2 = acid_mulq(g, g);
        a->G3 = acid_mulq(a->G2, g);
        a->G4 = acid_mulq(a->G3, g);
        a->omG = 32768 - g;
        a->R = (int32_t)((4096u << 15) / (uint32_t)(4096 + (a->k * a->G4 >> 15)));
    }
    t = a->ph >> 16;
    dt = a->incT >> (16 + ACID_OSH);
    a->ph += a->incT >> ACID_OSH;
    a->ph2 += a->incT >> (ACID_OSH + 1);
    if (kn->wave & 1u) {
        s = t < 32768u ? 22000 : -22000;
        s += acid_mulq(acid_blep(t, dt), 22000);
        s -= acid_mulq(acid_blep((t + 32768u) & 65535u, dt), 22000);
    } else {
        s = ((int32_t)t - 32768) * 22000 / 32768;
        s -= acid_mulq(acid_blep(t, dt), 22000);
        s = -s;
    }
    a->bl += acid_mulq(s - a->bl, 330);
    a->bl2 += acid_mulq(a->bl - a->bl2, 330);
    a->bl3 += acid_mulq(a->bl2 - a->bl3, 330);
    if (kn->sub)
        s += (a->ph2 < 0x80000000u ? 9000 : -9000) * (int32_t)kn->sub / 31;
    {
        int32_t S, u, o;
        S = acid_mulq(acid_mulq(a->s[0], a->G3) + acid_mulq(a->s[1], a->G2) + acid_mulq(a->s[2], a->G) + a->s[3],
                      a->omG);
        fb = (S >> 4) * a->k >> 12;
        if (a->drv < 3) {                               /* a whisper, so the filter can ring on its own */
            a->nz = a->nz * 1664525u + 1013904223u;
            fb -= (int32_t)(a->nz >> 27) - 16;
        }
        u = acid_soft(acid_mulq(((s >> 2) * (int32_t)ACID_DRV_G[a->drv & 31] >> 6) - fb, a->R)) * 16;
        o = acid_mulq(u, a->G) + acid_mulq(a->s[0], a->omG);
        a->s[0] = 2 * o - a->s[0];
        u = o;
        o = acid_mulq(u, a->G) + acid_mulq(a->s[1], a->omG);
        a->s[1] = 2 * o - a->s[1];
        x2 = o;
        (void)x2;
        u = o;
        o = acid_mulq(u, a->G) + acid_mulq(a->s[2], a->omG);
        a->s[2] = 2 * o - a->s[2];
        x = o;
        u = o;
        o = acid_mulq(u, a->G) + acid_mulq(a->s[3], a->omG);
        a->s[3] = 2 * o - a->s[3];
        x = (x * (31 - (int32_t)kn->pol) + o * (int32_t)kn->pol) / 31;   /* POL: three poles to four (31: four) */
    }
    return x;
}

static void acid_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    acid_t *a = acid_of(t);
    uint32_t i;
    if (!a || !a->live)
        return;
    acid_knobs(t, &a->kn);
    {
        const acid_kn_t *kn = &a->kn;
        int32_t want = m->pitch16 * 4096;               /* the part's pitch (bend, tune, vibrato) in 1/65536 semitone */
        want = want < 0 ? 0 : want > 126 * 65536 ? 126 * 65536 : want;
        if (!v->gate) {
            a->gate = 0;
        } else if (a->gate && v->note != a->note) {     /* LEGATO: the host changed the pitch of the sounding note */
            acid_gate_on(a, v->note, v->vel > ACID_ACC_VEL, 1);
        }
        a->tp = want;
        for (i = 0; i < n; i++) {
            int32_t s, x, k;
            if ((a->tick++ & 7u) == 0u) {               /* control rate: 5.5 kHz */
                const uint32_t accx = a->acc;
                const uint32_t swp_on = kn->hir != 15u;
                int32_t tg = a->gate ? (accx && swp_on ? 32767 : 24000) : 0, e, idx, qi, amt, pp;
                uint32_t ep;
                {
                    int32_t d = a->tp - a->p, st = acid_mulq(d, ACID_GLIDE_G[kn->sld]);
                    a->p += (!st && d) ? (d > 0 ? 1 : -1) : st;      /* (the last 1/65536 semitones still arrive) */
                }
                pp = a->p >> 8;                                   /* 1/256 semitone: the tables' resolution */
                pp = pp < 0 ? 0 : pp > 126 * 256 ? 126 * 256 : pp;
                {
                    uint32_t nn = (uint32_t)pp >> 8, na = ACID_NOTE_INC[nn], nb = ACID_NOTE_INC[nn + 1u];
                    a->incT = (uint32_t)acid_mulq((int32_t)(na + (((nb - na) >> 8) * ((uint32_t)pp & 255u))),
                                                  ACID_TUNE_R[kn->tun]);
                }
                a->va = 1 << 24;                         /* VOL DEC at 31: the volume holds for the whole gate */
                {
                    int32_t vr = a->gate ? (accx ? 9000 : ACID_ATK_G[kn->atk]) : ACID_REL_RATE;
                    a->v1 += acid_mulq(tg - a->v1, vr);
                    a->vca += acid_mulq(a->v1 - a->vca, vr);
                }
                ep = a->env_pos >> 16;
                e = ACID_ENV_T[ep < 767u ? ep : 767u];
                a->env_pos += ACID_DEC_STEP[accx ? kn->adc : kn->dec];
                a->acc_cap -= a->acc_cap >> (12 - (int32_t)kn->swp / 6);
                amt = ACID_ENV_AMT[kn->env];
                if (accx && swp_on)
                    amt += acid_mulq(amt + 30, 8192 + (a->acc_cap >> 1) + (int32_t)kn->acc * 1000);
                idx = (int32_t)kn->cut * 7 + acid_mulq(e, amt);
                if (idx > 255)
                    idx = 255;
                if (idx < 0)
                    idx = 0;
                qi = idx << 8;
                if (kn->trk) {                           /* TRK: the cutoff follows the note, in Hz */
                    int32_t hz = (int32_t)ACID_CUT_HZ[idx] + ((pp - 37 * 256) * (int32_t)kn->trk * 29) / 1024, lo = 0,
                            hi = 255;
                    if (hz <= (int32_t)ACID_CUT_HZ[0]) {
                        qi = 0;
                    } else if (hz >= (int32_t)ACID_CUT_HZ[255]) {
                        qi = 255 << 8;
                    } else {
                        while (hi - lo > 1) {
                            int32_t mid = (lo + hi) >> 1;
                            if ((int32_t)ACID_CUT_HZ[mid] <= hz)
                                lo = mid;
                            else
                                hi = mid;
                        }
                        qi = (lo << 8) +
                             (hz - (int32_t)ACID_CUT_HZ[lo]) * 256 / ((int32_t)ACID_CUT_HZ[lo + 1] - (int32_t)ACID_CUT_HZ[lo]);
                    }
                }
                a->ci += (qi - a->ci) / 3;               /* the cutoff does not jump when a note starts */
                idx = a->ci >> 8;
                a->idxc = idx;
                a->fmk = (int32_t)kn->fm * (int32_t)kn->fm;
                k = ACID_RES_K[kn->res];
                a->drv = kn->drv;
                if (kn->hir)
                    k *= 2;
                a->G = ACID_CUT_G[idx];
                a->G2 = acid_mulq(a->G, a->G);
                a->G3 = acid_mulq(a->G2, a->G);
                a->G4 = acid_mulq(a->G3, a->G);
                a->omG = 32768 - a->G;
                a->k = k;
                a->R = (int32_t)((4096u << 15) / (uint32_t)(4096 + (k * a->G4 >> 15)));
            }
            {
                int32_t xa = acid_core_step(a), xb = ACID_OS == 2 ? acid_core_step(a) : xa;
                x = ACID_OS == 2 ? (a->xp + 2 * xa + xb) >> 2 : xa;     /* [1 2 1] down to 44.1 kHz */
                a->xp = xb;
            }
            a->vs += (a->vca - a->vs) >> 3;
            s = acid_mulq(x >> 4, a->vs);
            s = s * (4096 + (a->k >> 2)) >> 12;
            s = acid_soft(s * (6 + (int32_t)kn->muf * 2) / 4) * 7 / 8;
            a->dcs += (s * 16 - a->dcs) >> 9;
            s -= acid_mulq(a->dcs >> 4, a->vs * 4 > 32768 ? 32768 : a->vs * 4);
            s = s * (24 + (int32_t)kn->lvl) / 24;
            a->fmy = s;
            if (s > 32000)
                s = 32000;
            if (s < -32000)
                s = -32000;
            out[i] += voice_amp(s, m, i);               /* the part's fades and LFO -> AMP, and the voice level */
        }
    }
}

static const char *const N_ACID_WAVE[] = {"SAW", "SQR"};

static const preset_t ACID_PRESETS[] = {
    /* E: CUT RES ENV DEC ACC SLD DRV WAVE; env/fenv are the host's ADSR (unused: the voice has its own VCA) */
    {"ACID", {14, 20, 18, 12, 16, 14, 4, 0}, {0, 64, 127, 10}, 0, 1, FX(0, 0, 0, 0)},
    {"SQUELCH", {8, 28, 26, 8, 24, 14, 6, 0}, {0, 64, 127, 10}, 0, 1, FX(8, 0, 20, 8)},
    {"ROUND SQR", {10, 14, 14, 14, 14, 16, 2, 1}, {0, 64, 127, 10}, 0, 1, FX(0, 0, 0, 6)},
    {"DRIVEN", {12, 24, 20, 10, 20, 12, 18, 0}, {0, 64, 127, 10}, 0, 1, FX(14, 0, 14, 8)},
    {"SLIDER", {13, 22, 16, 16, 18, 24, 5, 0}, {0, 64, 127, 10}, 0, 1, FX(0, 0, 16, 10)},
    {"DEEP", {5, 16, 12, 18, 12, 14, 3, 1}, {0, 64, 127, 10}, 0, 1, FX(0, 0, 0, 4)},
};

static const engine_t ENG_ACID = {
    .name = "ACID",
    .page_title = {"303", "VOICE"},
    .edit = {
        {"CUT", F_INT, 0, 31, 14, 0, 0},
        {"RES", F_INT, 0, 31, 20, 0, 0},
        {"ENV", F_INT, 0, 31, 18, 0, 0},
        {"DEC", F_INT, 0, 31, 12, 0, 0},
        {"ACC", F_INT, 0, 31, 16, 0, 0},
        {"SLIDE", F_INT, 0, 31, 14, 0, 0},
        {"DRIVE", F_INT, 0, 31, 4, 0, 0},
        {"WAVE", F_ENUM, 0, 1, 0, N_ACID_WAVE, 0},
    },
    .presets = ACID_PRESETS,
    .npresets = NELEM(ACID_PRESETS),
    .note_on = acid_note_on,
    .render = acid_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 1,
    .ownenv = 1,
    .done = acid_done,
    .mem = sizeof(acid_t),
};
