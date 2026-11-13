/* Scintilla (Joakim Eriksson, GPL-3.0-only, https://github.com/joakimeriksson/scintilla-fm1) engine/sc_patch.h with every sc_ / SC_ name
 * prefixed scx_ / SCX_ by tools/gen_synthex.py: not edited by hand. */
/* SPDX-License-Identifier: GPL-3.0-only */
/* A Scintilla patch in integers. It mirrors synthex-web's patch.ts field for field; every continuous
 * value is the TypeScript value in Q16 (x 65536): seconds stay seconds, Hz stay Hz, cents stay
 * cents, levels stay 0..1. Enums are small integers in the TypeScript order. That one rule is what
 * lets tools/gen_presets.mjs convert the factory bank mechanically. */
#ifndef SCX_PATCH_H
#define SCX_PATCH_H
#include <stdint.h>

#define SCX_Q16(x) ((int32_t)((x) * 65536))

enum { SCX_WAVE_TRI, SCX_WAVE_SAW, SCX_WAVE_SQR, SCX_WAVE_SIN, SCX_WAVE_NOISE };
enum { SCX_FLT_LP24, SCX_FLT_LP12, SCX_FLT_BP12, SCX_FLT_BP6, SCX_FLT_HP12 };
enum { SCX_LFO_TRI, SCX_LFO_SQR, SCX_LFO_SH, SCX_LFO_RND, SCX_LFO_RAMP, SCX_LFO_RAMPDN, SCX_LFO_SQRU };
enum { SCX_GLIDE_OFF, SCX_GLIDE_GLIDE, SCX_GLIDE_PORTA };
enum { SCX_KEY_POLY, SCX_KEY_MONO, SCX_KEY_UNISON };
enum { SCX_VM_SINGLE, SCX_VM_SPLIT, SCX_VM_DOUBLE };
enum { SCX_DLY_STANDARD, SCX_DLY_TAPE, SCX_DLY_PINGPONG };
enum { SCX_REV_PLATE, SCX_REV_ROOM, SCX_REV_HALL, SCX_REV_GALACTIC };

/* modMatrix, in patch.ts's ModSlot order */
enum {
    SCX_MM_L1_P1, SCX_MM_L1_P2, SCX_MM_L1_CUT, SCX_MM_L1_PW1, SCX_MM_L1_PW2, SCX_MM_L1_AMP,
    SCX_MM_L2_P1, SCX_MM_L2_P2, SCX_MM_L2_CUT, SCX_MM_L2_PW1, SCX_MM_L2_PW2, SCX_MM_L2_AMP,
    SCX_MM_E1_CUT, SCX_MM_E1_P2,
    SCX_MM_JX_L1, SCX_MM_JX_L2,
    SCX_MM_JY_CUT, SCX_MM_JY_LFOFILT,
    SCX_MM_VEL_AMP, SCX_MM_VEL_CUT, SCX_MM_VEL_E1,
    SCX_MM_N
};

typedef struct { int32_t a, d, s, r; } scx_adsr_t;

typedef struct {
    int8_t shape, sync;
    int32_t rate, delay, depth_a, depth_b;
} scx_lfo_p_t;

typedef struct {                        /* LayerPatch */
    int8_t  o1_wave, o1_oct;
    int32_t o1_pwm, o1_glide_amt, o1_glide_speed;
    int8_t  o2_wave, o2_oct, o2_sync;
    int32_t o2_detune, o2_pwm, o2_glide_amt, o2_glide_speed;
    int32_t mix_o1, mix_o2, mix_noise;
    int8_t  noise_pink, ring;
    int32_t cross, cross2;              /* OSC1 -> OSC2 PWM, OSC2 -> OSC1 PWM */
    int8_t  flt_mode;
    int32_t cutoff, reso, env_amt, key_track;
    scx_adsr_t env_f, env_a;
    scx_lfo_p_t lfo[2];
    int32_t mm[SCX_MM_N];
    int32_t glide_time, glide_amt;
    int8_t  glide_mode, glide_o1, glide_o2;
    int8_t  key_assign, multi_trigger;
    int32_t pan;
} scx_layer_t;

typedef struct { int8_t on, mode; int32_t mix, rate, depth; } scx_chorus_p_t;
typedef struct { int8_t on, mode; int32_t time, feedback, tone, spread, mix; } scx_delay_p_t;
typedef struct { int8_t on, algo; int32_t size, damping, mix; } scx_reverb_p_t;
typedef struct { scx_chorus_p_t chorus; scx_delay_p_t delay; scx_reverb_p_t reverb; } scx_fx_p_t;

typedef struct {                        /* Patch */
    char name[28];
    scx_layer_t layer[2];                /* upper, lower */
    int8_t  voice_mode, split;
    scx_fx_p_t fx;                       /* both layers use it (fxLower is not in the factory bank) */
    int32_t tune, fine_tune, volume, unison_detune;
    int8_t  polyphony, limiter;
} scx_patch_t;

#endif
