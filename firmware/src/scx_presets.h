/* Scintilla (Joakim Eriksson, GPL-3.0-only, https://github.com/joakimeriksson/scintilla-fm1) engine/sc_presets.h with every sc_ / SC_ name
 * prefixed scx_ / SCX_ by tools/gen_synthex.py: not edited by hand. */
/* SPDX-License-Identifier: GPL-3.0-only */
/* Original 25-byte panel records for PRESETS; software extension for MEMORIES.
 * Decode on recall; runtime patches keep exact Q16 and independently editable layers. */
#ifndef SCX_PRESETS_H
#define SCX_PRESETS_H
#include <stddef.h>
#include "scx_patch.h"
#include "scx_presets_data.h"
_Static_assert(sizeof(scx_patch_t) == 620 && sizeof(scx_layer_t) == 256 &&
               offsetof(scx_patch_t, layer) == 28 && offsetof(scx_patch_t, voice_mode) == 540,
               "update the preset extension codec for this patch layout");

static const char *scx_preset_name(unsigned index)
{
    return SCX_PRESET_NAMES + SCX_PRESET_NAME_OFF[index % SCX_N_PRESETS];
}

/* Rational rounding, ties to even; all numerators fit int32. */
static int32_t scx_rom_round(int32_t n, int32_t d)
{
    int32_t sign = n < 0 ? -1 : 1;
    uint32_t a = (uint32_t)(n < 0 ? -n : n), q = a / (uint32_t)d, r = a % (uint32_t)d;
    if (r * 2u > (uint32_t)d || (r * 2u == (uint32_t)d && (q & 1u))) q++;
    return sign * (int32_t)q;
}

/* Decimal -> Q16, Math.round as in gen_presets.mjs (negative ties toward +inf). */
static int32_t scx_rom_q(int32_t n, int32_t scale)
{
    int32_t v = n * 65536;
    return v >= 0 ? (v + scale / 2) / scale : -((-v + (scale - 1) / 2) / scale);
}

static unsigned scx_rom_semis(unsigned value)
{
    static const uint8_t thresholds[13] = {0, 5, 21, 45, 71, 96, 120, 146, 171, 195, 217, 235, 250};
    unsigned i, semis = 0;
    for (i = 1; i < 13; i++) if (value >= thresholds[i]) semis = i;
    return semis;
}

static void scx_rom_load(unsigned index, scx_patch_t *p)
{
    static const uint8_t widths[40] = {
        8,8,8,8,8,8,8,8,8, 7,7,7,7,7,7,7,7, 4,4,4,4,4,4,4,4,4,
        6, 3,3,3,3, 2,2,2,2,2,2,2,2,2
    };
    static const uint8_t wave[5] = { SCX_WAVE_TRI, SCX_WAVE_SAW, SCX_WAVE_SQR, SCX_WAVE_SQR, SCX_WAVE_SQR };
    static const uint8_t filter[4] = { SCX_FLT_LP24, SCX_FLT_BP6, SCX_FLT_BP12, SCX_FLT_HP12 };
    static const uint8_t lfo[4] = { SCX_LFO_TRI, SCX_LFO_RAMPDN, SCX_LFO_RAMP, SCX_LFO_SQR };
    const uint8_t *record = SCX_PRESET_RECORDS[index];
    uint8_t v[40];
    unsigned i, k, bit = 0;
    for (i = 0; i < 40; i++) {
        unsigned value = 0;
        for (k = 0; k < widths[i]; k++, bit++)
            value |= ((record[24 - bit / 8] >> (bit % 8)) & 1u) << k;
        if ((i >= 9 && i < 26) || i >= 34) value <<= 8 - widths[i];
        v[i] = (uint8_t)value;
    }
    scx_layer_t *L = &p->layer[0];
    L->o1_wave = (int8_t)wave[v[29]]; L->o1_oct = (int8_t)(v[27] - 2);
    L->o2_wave = (int8_t)wave[v[30]]; L->o2_oct = (int8_t)(v[28] - 2);
    L->o2_sync = (int8_t)((v[35] & 128u) != 0);
    L->o1_glide_speed = L->o2_glide_speed = 3277;
    int32_t pwm1 = scx_rom_round(v[11] * 1000, 254), pwm2 = scx_rom_round(v[12] * 1000, 254);
    if (pwm1 < 50) pwm1 = 50;
    if (pwm1 > 950) pwm1 = 950;
    if (pwm2 < 50) pwm2 = 50;
    if (pwm2 > 950) pwm2 = 950;
    L->o1_pwm = v[29] >= 3 ? scx_rom_q(pwm1, 1000) : 32768;
    L->o2_pwm = v[30] >= 3 ? scx_rom_q(pwm2, 1000) : 32768;
    L->o2_detune = scx_rom_q(1000 * ((int32_t)scx_rom_semis(v[16]) - (int32_t)scx_rom_semis(v[15])) +
                              scx_rom_round(((int32_t)v[0] - 128) * 125, 32), 10);
    L->mix_o1 = scx_rom_q(scx_rom_round(v[17] * 35, 12), 1000);
    L->mix_o2 = scx_rom_q(scx_rom_round(v[18] * 35, 12), 1000);
    L->mix_noise = (v[39] & 192u) ? scx_rom_q(scx_rom_round(v[19] * 35, 12), 1000) : 0;
    L->noise_pink = (int8_t)((v[39] & 128u) != 0);
    L->ring = (int8_t)((v[37] & 192u) != 0);
    L->cross = v[30] == 4 ? 36438 : 0; L->cross2 = v[29] == 4 ? 36438 : 0;
    L->flt_mode = (int8_t)filter[v[32]];
    /* gen_factory.py's KEY_W = 1 / (2 CUT_SLOPE): full key tracking 1.0 st per key (synthex-web 1da4205) */
    /* gen_factory.py's FILTER_ENV_FIX, the presets corrected one by one against the recording: the cutoff
     * offset in semitones and the envelope amount in tenths (synthex-web 47cc14e; Filtered chorus 25de95b) */
    int32_t fix_st = index == 16 ? -8 : index == 17 ? 6 : index == 23 ? -12 : 0;   /* High pass sweep, Filtered brass, Clavichord I */
    int32_t fix_env = index == 16 ? 5 : index == 17 || index == 18 || index == 23 ? 7 : 10;
    L->cutoff = scx_rom_q(scx_rom_round((-25000 + 1000 * fix_st + 580 * (255 - (int32_t)v[5])) * 12750 +
                                     1200000 * (int32_t)v[6], 1224000), 1000);
    L->key_track = scx_rom_q(scx_rom_round(1000 * (int32_t)v[6], 255), 1000);
    L->reso = scx_rom_q(scx_rom_round(v[10] * 1000, 254), 1000);
    int32_t env_long = (v[20] >> 4) <= 3 && (v[22] >> 4) > 6;
    int32_t env = env_long ? SCX_ROM_ENV_LONG_MILLI[v[7]] : SCX_ROM_ENV_MILLI[v[7]];
    /* a corrected one from the amount itself (0.58 * 1.25 * v / 60), not from the table's rounded value */
    if (fix_env != 10) env = scx_rom_round(725 * (int32_t)v[7] * (env_long ? 6 : 10) * fix_env, 6000);
    L->env_amt = scx_rom_q((v[35] & 64u) ? -env : env, 1000);
    L->env_f.a = scx_rom_q(SCX_ROM_ATTACK_MS[v[20] >> 4], 1000);
    L->env_f.d = scx_rom_q(index == 32 ? 3300 : SCX_ROM_ENV_MS[v[22] >> 4], 1000); /* calibrated Take-off */
    L->env_f.s = scx_rom_q(scx_rom_round(v[8] * 700, 255), 1000);         /* FILTER_SUSTAIN 0.7 */
    L->env_f.r = scx_rom_q((v[38] & 128u) ? SCX_ROM_ENV_MS[v[24] >> 4] : 10, 1000);
    L->env_a.a = scx_rom_q(SCX_ROM_ATTACK_MS[v[21] >> 4], 1000);
    L->env_a.d = scx_rom_q(SCX_ROM_ENV_MS[v[23] >> 4], 1000);
    L->env_a.s = scx_rom_q(scx_rom_round(v[9] * 1000, 254), 1000);
    L->env_a.r = scx_rom_q((v[38] & 128u) ? SCX_ROM_ENV_MS[v[25] >> 4] : 10, 1000);
    L->lfo[0].shape = (int8_t)lfo[v[31]];
    L->lfo[0].rate = scx_rom_q(SCX_ROM_RATE_CHZ[v[3]], 100);
    L->lfo[0].delay = scx_rom_q(SCX_ROM_DELAY_CS[v[4]], 100);
    /* both depth knobs read inverted (byte 255 = none): the LFO board's level CVs bias its OTAs through
     * (5 V - LEV) / 10K. gen_factory.py's PITCH_MOD 0.09, PWM_MOD 2, CUTOFF_MOD 2, AMP_MOD 1 */
    L->lfo[0].depth_a = scx_rom_q(scx_rom_round((255 - (int32_t)v[1]) * 1000, 255), 1000);
    L->lfo[0].depth_b = scx_rom_q(scx_rom_round((255 - (int32_t)v[2]) * 1000, 255), 1000);
    L->lfo[1].rate = 26214; L->lfo[1].depth_a = L->lfo[1].depth_b = 65536;
    if (v[26] & 1u) L->mm[SCX_MM_L1_P1] = scx_rom_q(90, 1000);
    if (v[26] & 2u) L->mm[SCX_MM_L1_P2] = scx_rom_q(90, 1000);
    if (v[26] & 4u) L->mm[SCX_MM_L1_PW1] = 131072;
    if (v[26] & 32u) L->mm[SCX_MM_L1_PW2] = 131072;
    if (v[26] & 16u) L->mm[SCX_MM_L1_CUT] = 131072;
    if (v[26] & 8u) L->mm[SCX_MM_L1_AMP] = 65536;
    L->glide_o1 = L->glide_o2 = L->multi_trigger = 1;
    L->glide_mode = v[34] & 128u ? SCX_GLIDE_PORTA : v[34] & 64u ? SCX_GLIDE_GLIDE : SCX_GLIDE_OFF;
    if (L->glide_mode) {
        L->glide_time = scx_rom_q(SCX_ROM_GLIDE_MS[v[13] >> 1], 1000);
        L->glide_amt = ((int32_t)(v[14] >> 2) - 32) * 65536;
        L->glide_o1 = (int8_t)((v[36] & 128u) != 0);
        L->glide_o2 = (int8_t)((v[36] & 64u) != 0);
    }
    p->split = 60;
    p->fx.chorus.on = (int8_t)(v[33] != 0); p->fx.chorus.mode = v[33] ? (int8_t)v[33] : 2;
    p->fx.chorus.mix = p->fx.chorus.rate = 32768; p->fx.chorus.depth = 39322;
    p->fx.delay.time = 16384; p->fx.delay.feedback = p->fx.delay.spread = 19661;
    p->fx.delay.tone = 32768; p->fx.delay.mix = 13107;
    p->fx.reverb.size = p->fx.reverb.damping = 32768; p->fx.reverb.mix = 13107;
    p->volume = 52429; p->unison_detune = 9830; p->polyphony = 8; p->limiter = 1;
}

static void scx_preset_load(unsigned index, scx_patch_t *p)
{
    unsigned i, k;
    uint8_t *bytes = (uint8_t *)p;
    index %= SCX_N_PRESETS;
    for (i = 0; i < sizeof *p; i++) bytes[i] = 0;
    if (index < 40) scx_rom_load(index, p);
    else {
        const uint8_t *record = SCX_PRESET_RECORDS[index];
        unsigned bit = 0;
        for (i = 0; i < 84; i++) {
            unsigned first = SCX_MEM_PALETTE_OFF[i], count = SCX_MEM_PALETTE_OFF[i + 1] - first;
            unsigned width = 0, code = 0;
            while ((1u << width) < count) width++;
            for (k = 0; k < width; k++, bit++) code |= ((record[bit / 8] >> (bit % 8)) & 1u) << k;
            uint32_t value = SCX_MEM_VALUES[first + code];
            unsigned offset = i < 64 ? 28 + 4 * i : 540 + 4 * (i - 64);
            for (k = 0; k < 4; k++) bytes[offset + k] = (uint8_t)(value >> (8 * k));
        }
    }
    p->layer[1] = p->layer[0];
    const char *name = scx_preset_name(index);
    for (i = 0; name[i] && i < sizeof p->name - 1; i++) p->name[i] = name[i];
}
#endif
