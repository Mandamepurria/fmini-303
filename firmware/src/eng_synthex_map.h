/* SPDX-License-Identifier: GPL-3.0-only
 * SYNTHEX: what a Scintilla patch (scx_patch_t) gives the host, and the seven EDIT values over it. Plain C, no host types: the
 * engine (eng_synthex.c) and the generator of the program table (tools/gen_synthex.py) both include it, so the table's numbers
 * are the ones the engine computes.
 *   E0 CUTOFF, E1 RESO (the upper layer's, 0..127), E2 ENV AMT (64 = none), E3 A.ATTACK, E4 A.RELEASE, E5 F.DECAY (times: an
 *   index 0..127 into SYNX_TIME_Q16, 1 ms .. 10 s, exponential), E6 VOLUME. An edited value is applied to BOTH layers as the step
 *   from the program's own: the layers keep what differs between them. */
#ifndef SYNX_MAP_H
#define SYNX_MAP_H
#include <stdint.h>
#include "scx_patch.h"
#include "scx_fixed.h"

static const uint32_t SYNX_TIME_Q16[128] = {   /* seconds, Q16: 0.001 * 10^(v * 4 / 127) */
    66, 70, 76, 81, 88, 94, 101, 109,
    117, 126, 135, 146, 156, 168, 181, 195,
    209, 225, 242, 260, 280, 301, 323, 347,
    374, 402, 432, 464, 499, 537, 577, 621,
    667, 718, 772, 830, 892, 959, 1031, 1109,
    1192, 1282, 1378, 1482, 1593, 1713, 1842, 1981,
    2130, 2290, 2462, 2647, 2846, 3060, 3291, 3538,
    3804, 4090, 4398, 4729, 5084, 5467, 5878, 6320,
    6796, 7307, 7856, 8447, 9083, 9766, 10500, 11290,
    12139, 13052, 14034, 15090, 16225, 17445, 18757, 20168,
    21685, 23316, 25070, 26956, 28983, 31163, 33507, 36028,
    38738, 41651, 44784, 48153, 51775, 55669, 59856, 64359,
    69199, 74404, 80001, 86018, 92488, 99445, 106925, 114967,
    123615, 132913, 142910, 153659, 165217, 177644, 191006, 205373,
    220820, 237430, 255288, 274490, 295137, 317336, 341205, 366869,
    394464, 424134, 456037, 490338, 527220, 566876, 609514, 655360,
};

typedef struct { uint8_t mono, vmode, cho, dly, rev; } synx_map_t;   /* vmode: 0 poly, 1 mono, 2 unison */

static int32_t synx_clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t synx_q16_to_127(int32_t q) { return synx_clampi((int32_t)(((int64_t)q * 127 + 32768) >> 16), 0, 127); }
static uint32_t synx_time_idx(int32_t q)             /* the index whose time is nearest (in ratio) */
{
    uint32_t i = 0;
    if (q <= (int32_t)SYNX_TIME_Q16[0]) return 0;
    while (i < 127u && (int32_t)SYNX_TIME_Q16[i + 1] <= q) i++;
    if (i < 127u && (uint64_t)q * q > (uint64_t)SYNX_TIME_Q16[i] * SYNX_TIME_Q16[i + 1]) i++;   /* nearer the next one */
    return i;
}
static void synx_e_from_patch(const scx_patch_t *p, uint8_t e[7])
{
    const scx_layer_t *L = &p->layer[0];
    e[0] = (uint8_t)synx_q16_to_127(L->cutoff);
    e[1] = (uint8_t)synx_q16_to_127(L->reso);
    e[2] = (uint8_t)synx_clampi(64 + (int32_t)(((int64_t)L->env_amt * 63 + (L->env_amt < 0 ? -32768 : 32768)) / 65536), 0, 127);
    e[3] = (uint8_t)synx_time_idx(L->env_a.a);
    e[4] = (uint8_t)synx_time_idx(L->env_a.r);
    e[5] = (uint8_t)synx_time_idx(L->env_f.d);
    e[6] = (uint8_t)synx_q16_to_127(p->volume);
}
static int32_t synx_scale_time(int32_t t, uint32_t from, uint32_t to)   /* t * T[to] / T[from] */
{
    uint64_t v;
    if (t <= 0 || from == to) return t;
    v = scx_udiv64((uint64_t)t * SYNX_TIME_Q16[to], SYNX_TIME_Q16[from]);
    return v > 0x7FFFFFFFu ? 0x7FFFFFFF : (int32_t)v;
}
/* p (a freshly decoded program) with the EDIT values e laid over it; base: the program's own (synx_e_from_patch) */
static void synx_apply(scx_patch_t *p, const uint8_t base[7], const uint8_t e[7])
{
    uint32_t k;
    for (k = 0; k < 2u; k++) {
        scx_layer_t *L = &p->layer[k];
        L->cutoff = synx_clampi(L->cutoff + ((int32_t)e[0] - base[0]) * 516, 0, 65536);        /* 65536 / 127 */
        L->reso = synx_clampi(L->reso + ((int32_t)e[1] - base[1]) * 516, 0, 65536);
        L->env_amt = L->env_amt + ((int32_t)e[2] - base[2]) * 1040;                              /* 65536 / 63 */
        L->env_a.a = synx_scale_time(L->env_a.a, base[3], e[3]);
        L->env_a.r = synx_scale_time(L->env_a.r, base[4], e[4]);
        L->env_f.d = synx_scale_time(L->env_f.d, base[5], e[5]);
    }
    p->volume = synx_clampi(p->volume + ((int32_t)e[6] - base[6]) * 516, 0, 131072);
}
static void synx_host_from_patch(const scx_patch_t *p, synx_map_t *m)
{
    int ka = p->layer[0].key_assign;
    m->vmode = ka == SCX_KEY_MONO ? 1 : ka == SCX_KEY_UNISON ? 2 : 0;
    m->mono = ka == SCX_KEY_MONO;
    m->cho = p->fx.chorus.on ? (uint8_t)synx_q16_to_127(p->fx.chorus.mix) : 0;
    m->dly = p->fx.delay.on ? (uint8_t)synx_q16_to_127(p->fx.delay.mix) : 0;
    m->rev = p->fx.reverb.on ? (uint8_t)synx_q16_to_127(p->fx.reverb.mix) : 0;
}
#endif
