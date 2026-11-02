/* SPDX-License-Identifier: GPL-3.0-only
 * FMini-1 screen: renders every page of fmini_app.c with the real drawing code to PPMs (OUT/fa_<page>.ppm) and
 * checks that the screen draws and that SELECT / ALGORITHM / PRESETS reach every control. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FMINI_ONLY 1
#include "ui_test.c"
#include "../firmware/src/fmini_rec.c"
#include "../firmware/src/fmini_bank.c"
#include "../firmware/src/fmini_app.c"
#include <stdio.h>
extern int32_t midi_bend_target[NTRK];   /* (voice.c: the bend each part is moving to, Q8 semitones) */
static void dump(const char *dir, int n)
{
    char p[256];
    uint32_t i;
    FILE *f;
    snprintf(p, sizeof p, "%s/fa_%d.ppm", dir, n);
    f = fopen(p, "wb");
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}
/* the FMini-1 loop's frame: fa_input is its input (ui_input is Felucca's) */
static void fframe(void) { host_ticks += 16000u; fm1_ms += 16u; fa_input(); }
static void fframes(uint32_t ms) { uint32_t i; for (i = 0; i < ms / 16u; i++) fframe(); }
static void fpress(uint32_t label)
{
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    fframe();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    fframe();
}
static void fhold(uint32_t label)
{
    uint32_t k;
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    for (k = 0; k < 52u; k++)
        fframe();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    fframe();
}
static void fturn(uint32_t role, int32_t d)
{
    host_enc[panel.enc[role]] += d * panel.dir[role];
    fframe();
    host_ticks += 200000u;
}
int main(int argc, char **argv)
{
    int pg;
    const char *dir = argc > 1 ? argv[1] : ".";
    { uint32_t gi; for (gi = 0; gi < G_COUNT; gi++) song.g[gi] = GP[gi].def; }   /* main.c's power-on globals */
    fb_names_init();                                  /* the RAM presets (main.c: fmini_init, fb_boot) */
    fa_init();
    for (pg = 0; pg < 7; pg++) {
        fa_page = (uint8_t)pg;
        fmg_cur[pg] = pg == 1 ? 5 : 1;
        ui.force = 1;
        fa_draw();
        dump(dir, pg);
    }
    {   /* every knob of FILT / F.ENV / A.ENV / FX moves when turned */
        int pgs[4] = {3, 4, 5, 6}, k, bad = 0;
        for (k = 0; k < 16; k++) {
            int pg2 = pgs[k / 4], kn = k % 4;
            uint32_t before = 0, after = 0, i;
            fa_page = (uint8_t)pg2;
            for (i = 0; i < FG_SIZE; i++) before = before * 31u + fmini_patch[0][i];
            for (i = 0; i < P_COUNT; i++) before = before * 31u + (uint32_t)TSEL->p[i];
            fmg_force_page = pg2;
            fmg_knob((uint32_t)kn, 3);
            for (i = 0; i < FG_SIZE; i++) after = after * 31u + fmini_patch[0][i];
            for (i = 0; i < P_COUNT; i++) after = after * 31u + (uint32_t)TSEL->p[i];
            if (before == after) { printf("page %d knob %d does not move\n", pg2, kn); bad++; }
        }
        if (bad) return 1;
    }
    fa_splash();
    dump(dir, 9);
    {   /* the power-on splash carries the version, bottom right, as ABOUT does: the same picture, and not the bare one */
        uint32_t i, diff = 0, same = 1;
        static uint16_t sp[240u * 240u];
        for (i = 0; i < 240u * 240u; i++) sp[i] = host_screen[i];
        fa_gl = 3; fa_sig_prev = 0; ui.force = 1; fa_draw(); fa_gl = 0;
        for (i = 0; i < 240u * 240u; i++) if (sp[i] != host_screen[i]) same = 0;
        if (!same) { printf("splash != ABOUT\n"); return 1; }
        for (i = 0; i < 240u * 240u; i++) {
            uint32_t x = i % 240u, y = i / 240u;
            if (y >= 218u && y < 232u && x >= 232u - text_w(&AF_S, FMINI_VERSION) && x < 232u && sp[i] != swap16(FMINI_SPLASH_PAL[FMINI_SPLASH[i]])) diff++;
        }
        if (!diff) { printf("splash: no version text\n"); return 1; }
    }
    fa_page = 3;                                       /* patch 100 and the SAVE dialog */
    TSEL->p[P_E7] = 99;
    fmini_load_slot(0, 99);
    ui.force = 1;
    fa_draw();
    dump(dir, 10);
    fa_sv_open();
    fa_sv_name[0] = 'M';
    ui.force = 1;
    fa_draw();
    dump(dir, 11);
    {   /* SAVE: a user preset in the first free slot, found again by PRESETS; the bank is mostly mono, its pads, keys and plucks poly */
        apply_preset_to(TSEL, 5);                       /* SAVE writes the slot being played (150: a factory slot, saved over) */
        TSEL->p[P_E7] = 150;
        fmini_poll();
        fa_gl_close(); fa_ar = 0; fa_sc = 0; fa_sq = 0; fmg_fx = 0;   /* the SAVE dialog alone on the screen */
        fa_sv_open();
        if (!fa_sv || fa_sv_slot != 150) { printf("save: open\n"); return 1; }
        str_cpy(fa_sv_name, "MY PATCH", 13);
        ui.force = 1; fa_draw(); dump(dir, 77);                       /* the name dialog (bigger text) */
        fa_sv_press();                                  /* SAVE: the question, NO by default: nothing written yet */
        if (!fa_sv || !fa_sv_conf || fa_sv_yes || str_eq(ENGINES[ENGI_FMINI]->presets[150].name, "MY PATCH")) { printf("save: no question\n"); return 1; }
        ui.force = 1; fa_draw(); dump(dir, 78);                       /* the question: NO selected */
        fa_sv_press();                                  /* SAVE on NO: back to the name, still open */
        if (!fa_sv || fa_sv_conf || str_eq(ENGINES[ENGI_FMINI]->presets[150].name, "MY PATCH")) { printf("save: NO did not go back\n"); return 1; }
        fa_sv_press();                                  /* SAVE again, then ALGORITHM to YES */
        fa_sv_yes = 1;
        ui.force = 1; fa_draw(); dump(dir, 79);                       /* YES selected */
        /* the restore question on the panel: NO by default, ALGORITHM to YES, SAVE accepts; a page button refuses */
        fb_rs = FB_RS_ASK; fb_rs_yes = 0; fb_rs_t = fm1_ms; ui.force = 1;
        fframes(32);
        fa_draw(); ui.force = 1; fa_draw(); dump(dir, 81);
        fpress(B_SAVE);
        if (fb_rs != FB_RS_NO) { printf("restore: SAVE on NO did not refuse (%d)\n", fb_rs); return 1; }
        fb_rs = FB_RS_ASK; fb_rs_yes = 0; fb_rs_t = fm1_ms;
        panel.enc[EN_ALGO] = 1; panel.dir[EN_ALGO] = 1;   /* (the panel's ALGORITHM encoder: as PANEL_DEFAULT) */
        fturn(EN_ALGO, 1);
        fframes(32);
        if (!fb_rs_yes || fb_rs != FB_RS_ASK) { printf("restore: ALGORITHM did not go to YES (yes %d state %d enc %u dir %d host %d)\n", fb_rs_yes, fb_rs, panel.enc[EN_ALGO], panel.dir[EN_ALGO], (int)host_enc[panel.enc[EN_ALGO]]); return 1; }
        ui.force = 1; fa_draw(); dump(dir, 82);
        fpress(B_SAVE);
        if (fb_rs != FB_RS_OK) { printf("restore: SAVE on YES did not accept (%d)\n", fb_rs); return 1; }
        fframes(32); ui.force = 1; fa_draw(); dump(dir, 83);
        fb_rs = FB_RS_IDLE;
        fframes(32);
        fa_sv_press();                                  /* SAVE on YES writes the slot */
        if (fa_sv || TSEL->p[P_E7] != 150 || !str_eq(ENGINES[ENGI_FMINI]->presets[150].name, "MY PATCH")) { printf("save: not stored\n"); return 1; }
        TSEL->p[P_E7] = (int16_t)(FMINI_NSLOT - 1u);
        fa_preset(1);                                   /* the last slot (199) -> round to the first (0) */
        if (TSEL->p[P_E7] != 0) { printf("save: no wrap\n"); return 1; }
        /* the PRESETS knob: every detent is a preset, however fast they came (25 in one frame: 25 presets) */
        fa_sv = 0; fa_sv_conf = 0; fb_rs = FB_RS_IDLE;      /* the PRESETS knob, not the SAVE dialog's slot */
        TSEL->p[P_E7] = 10; fa_preset(25);
        if (TSEL->p[P_E7] != 35) { printf("knob: 25 detents at once did not move 25 presets (%d)\n", TSEL->p[P_E7]); return 1; }
        fa_preset(-30);
        if (TSEL->p[P_E7] != 5) { printf("knob: -30 detents did not move back 30 with the wrap (%d)\n", TSEL->p[P_E7]); return 1; }
        fa_preset(-7);
        if (TSEL->p[P_E7] != 198) { printf("knob: the wrap below 0 is wrong (%d)\n", TSEL->p[P_E7]); return 1; }
        TSEL->p[P_VOICE] = V_LEGATO;
        fa_mono();
        if (TSEL->p[P_VOICE] != V_MONO) { printf("not mono\n"); return 1; }
        TSEL->p[P_VOICE] = V_POLY;
        fa_mono();
        if (TSEL->p[P_VOICE] != V_POLY) { printf("POLY is not kept\n"); return 1; }
        TSEL->p[P_VOICE] = V_MONO;
        {   /* the bank: 200 patches, a voice mode each (leads, basses and SFX mono; pads, keys, brass chords and plucks poly), names that fit */
            uint32_t k, npoly = 0;
            if (FMINI_NFACTORY != 200u || strlen(FMINI_CATS) != 200u || NELEM(FMINI_PRESETS) != 200u) { printf("bank size %u\n", (uint32_t)FMINI_NFACTORY); return 1; }
            for (k = 0; k < FMINI_NFACTORY; k++) {
                apply_preset_to(TSEL, k);
                fa_mono();
                if (TSEL->p[P_VOICE] != (ENGINES[ENGI_FMINI]->presets[k].mono ? V_MONO : V_POLY)) { printf("preset %u: voice mode %d\n", k, TSEL->p[P_VOICE]); return 1; }
                if (strlen(ENGINES[ENGI_FMINI]->presets[k].name) > 12u) { printf("preset %u: name too long\n", k); return 1; }
                npoly += !ENGINES[ENGI_FMINI]->presets[k].mono;
            }
            if (npoly < 50u || npoly > 150u) { printf("bank: %u poly patches\n", npoly); return 1; }   /* (of 200: the first hundred had 58) */
            TSEL->p[P_VOICE] = V_MONO;
        }
    }
    {   /* CTRL's VOICES (the ninth control): ALGORITHM sets POLY / MONO, SAVE keeps it, a factory patch is MONO, 12 voices play */
        uint32_t k, n = 0;
        fa_page = 0;
        fmg_force_page = 0;
        fmg_cur[0] = 8;
        apply_preset_to(TSEL, 1);
        fa_mono();
        fmg_algo(1);
        if (TSEL->p[P_VOICE] != V_POLY) { printf("voices: ALGORITHM does not set POLY\n"); return 1; }
        fa_sv_open();
        fa_sv_slot = 151;
        str_cpy(fa_sv_name, "POLY PATCH", 13);
        fa_sv_do();
        apply_preset_to(TSEL, 0);
        fa_mono();
        if (TSEL->p[P_VOICE] != V_MONO) { printf("voices: the first factory patch (a lead) is not MONO\n"); return 1; }
        apply_preset_to(TSEL, 151);                     /* the slot saved above (POLY) comes back POLY */
        fa_mono();
        if (TSEL->p[P_VOICE] != V_POLY) { printf("voices: the preset does not keep POLY\n"); return 1; }
        TSEL->p[P_MUTE] = 0;
        for (k = 0; k < 12u; k++)
            trk_note_on(TSEL, (uint8_t)(48u + 3u * k), 100);
        for (k = 0; k < NVOICE; k++)
            n += TSEL->v[k].active && TSEL->v[k].gate;
        if (n != 12u) { printf("voices: %u of 12 voices sound\n", n); return 1; }
        for (k = 0; k < NVOICE; k++) TSEL->v[k].active = TSEL->v[k].gate = 0;
        fmg_algo(-1);
        if (TSEL->p[P_VOICE] != V_MONO) { printf("voices: ALGORITHM does not set MONO\n"); return 1; }
        fmg_cur[0] = 0;
        apply_preset_to(TSEL, 0);
        fa_mono();
    }
    {   /* the ARP settings and the tempo are part of a saved preset: SAVE keeps them, loading the preset sets them,
         * a factory patch brings its own (the FMINI_X table) */
        int16_t bpm0 = song.g[G_BPM];
        apply_preset_to(TSEL, 2);
        TSEL->p[P_AMODE] = 3; TSEL->p[P_ARATE] = 4; TSEL->p[P_AOCT] = 3; TSEL->p[P_AGATE] = 90; TSEL->p[P_AHOLD] = 1;
        song.g[G_BPM] = 93;
        song.octave = -2;
        song.g[G_RSIZE] = 11; song.g[G_DRREV] = 0; song.g[G_CRATE] = 100; song.g[G_DRLVL] = 127;
        fa_sv_open();
        fa_sv_slot = 152;
        str_cpy(fa_sv_name, "ARP PATCH", 13);
        fa_sv_do();
        apply_preset_to(TSEL, 4);
        song.g[G_BPM] = 150;
        song.octave = 3;
        song.g[G_RSIZE] = 50; song.g[G_DRREV] = 50; song.g[G_CRATE] = 50; song.g[G_DRLVL] = 50;
        if (TSEL->p[P_AMODE] != 0 || TSEL->p[P_AHOLD] != 0) { printf("arp: a factory patch leaves the ARP on\n"); return 1; }
        TSEL->p[P_E7] = 4;                              /* the track plays slot 4 */
        fa_preset(1);                                   /* the next factory patch (5, DUB STAB): its own FX settings, arpeggiator and octave */
        if (fa_on_user() || TSEL->p[P_E7] != 5 || song.g[G_RSIZE] != FMINI_X[5].fx[1] || song.g[G_DRLVL] != FMINI_X[5].fx[10] ||
            song.g[G_RTYPE] != FMINI_X[5].fx[0] || song.g[G_DTIME] != FMINI_X[5].fx[3] || song.octave != FMINI_X[5].oct) {
            printf("fx: factory patch 5 keeps the old FX settings (%d %d %d)\n", song.g[G_RSIZE], song.g[G_DRLVL], song.octave); return 1;
        }
        fa_factory_extras(TSEL, 152);                    /* the slot saved above: its own octave (-2) */
        if (song.octave != -2) { printf("octave: preset load -> %d\n", song.octave); return 1; }
        if (song.g[G_RSIZE] != 11 || song.g[G_DRREV] != 0 || song.g[G_CRATE] != 100 || song.g[G_DRLVL] != 127) {
            printf("fx: preset load: %d %d %d %d\n", song.g[G_RSIZE], song.g[G_DRREV], song.g[G_CRATE], song.g[G_DRLVL]);
            return 1;
        }
        /* 1.14.0: a preset holds the arpeggiator's mode, rate, octave and gate, not HOLD or the tempo (both are the song's) */
        if (TSEL->p[P_AMODE] != 3 || TSEL->p[P_ARATE] != 4 || TSEL->p[P_AOCT] != 3 || TSEL->p[P_AGATE] != 90) {
            printf("arp: preset load: mode %d rate %d oct %d gate %d hold %d bpm %d\n", TSEL->p[P_AMODE], TSEL->p[P_ARATE],
                   TSEL->p[P_AOCT], TSEL->p[P_AGATE], TSEL->p[P_AHOLD], song.g[G_BPM]);
            return 1;
        }
        up_load(0);                                     /* the first saved patch has its own (ARP off, its tempo) */
        TSEL->p[P_AMODE] = 0; TSEL->p[P_AHOLD] = 0; TSEL->p[P_ARATE] = TP[P_ARATE].def; TSEL->p[P_AOCT] = 1; TSEL->p[P_AGATE] = TP[P_AGATE].def;
        song.g[G_BPM] = bpm0;
        song.octave = 0;
        apply_preset_to(TSEL, 0);
        TSEL->user = 0;
    }
    {   /* every factory patch brings its own arpeggiator, LFO, modulation, FX settings, LEVEL and octave (the PRESETS knob's way in);
         * about a quarter has the ARP on, the brass (50..59) comes in an octave down, the tempo stays the player's */
        uint32_t k, on = 0, fxk, bad = 0;
        int16_t bpm0 = song.g[G_BPM];
        song.g[G_BPM] = 117;
        for (k = 0; k < FMINI_NSLOT; k++)            /* (the SAVE steps above wrote some slots: back to their factory patches) */
            fb_forget(k);
        for (k = 0; k < FMINI_NFACTORY; k++) {
            const fmini_x_t *x = &FMINI_X[k];
            TSEL->user = 0;
            if (k) {
                TSEL->p[P_E7] = (int16_t)(k - 1u);
                fa_preset(1);
            } else {
                apply_preset_to(TSEL, 0);
                TSEL->p[P_E7] = 0;
                fa_factory_extras(TSEL, 0);
            }
            if (TSEL->p[P_E7] != (int16_t)k || fa_on_user()) { printf("patch %u: PRESETS -> %d\n", k, TSEL->p[P_E7]); return 1; }
            if (TSEL->p[P_AMODE] != (x->arp[0] ? x->arp[1] : 0) || TSEL->p[P_ARATE] != x->arp[2] || TSEL->p[P_AOCT] != x->arp[3] ||
                TSEL->p[P_AGATE] != x->arp[4] || TSEL->p[P_ASWING] != x->arp[5] || TSEL->p[P_APROB] != x->arp[6] || TSEL->p[P_AHOLD] != 0) {
                printf("patch %u: arp %d %d %d %d\n", k, TSEL->p[P_AMODE], TSEL->p[P_ARATE], TSEL->p[P_AOCT], TSEL->p[P_AGATE]); bad++;
            }
            if (x->arp[0] && (x->arp[1] < 1 || x->arp[1] > 14 || x->arp[3] < 1 || x->arp[3] > 4 || x->arp[4] < 1)) { printf("patch %u: arp range\n", k); bad++; }
            on += x->arp[0];
            if (FMINI_FACTORY[k][FG_BPML] | FMINI_FACTORY[k][FG_BPMH] << 7 ? song.g[G_BPM] != (FMINI_FACTORY[k][FG_BPML] | FMINI_FACTORY[k][FG_BPMH] << 7) : song.g[G_BPM] != 117) {
                printf("patch %u: tempo %d\n", k, song.g[G_BPM]); bad++;      /* the patch's TEMPO (1.14.25); 0 keeps the song's */
            }
            if (TSEL->p[P_LEVEL] != x->level || x->level < 60 || x->level > 127) { printf("patch %u: LEVEL %d\n", k, TSEL->p[P_LEVEL]); bad++; }
            if (TSEL->p[P_LRATE] != x->lfo[0] || TSEL->p[P_LWAVE] != x->lfo[1] || TSEL->p[P_LFADE] != x->lfo[2] || TSEL->p[P_LSYNC] != x->lfo[3] ||
                TSEL->p[P_LD_PIT] != x->mod[0] || TSEL->p[P_LD_AMP] != x->mod[1] || TSEL->p[P_ED_PIT] != x->mod[2]) { printf("patch %u: LFO / mod\n", k); bad++; }
            for (fxk = 0; fxk < 12u; fxk++)
                if (song.g[FA_XG[fxk]] != clamp(x->fx[fxk], GP[FA_XG[fxk]].min, GP[FA_XG[fxk]].max) || x->fx[fxk] > GP[FA_XG[fxk]].max) {
                    printf("patch %u: FX %u = %d (%d)\n", k, fxk, song.g[FA_XG[fxk]], x->fx[fxk]); bad++;
                }
            if (song.octave != x->oct || ((k >= 130u && k < 140u) && x->oct != -1)) { printf("patch %u: octave %d\n", k, song.octave); bad++; }
            if (k >= 30u && k < 40u && song.octave != -1) { printf("patch %u: a bass comes in with OCT-1 (%d)\n", k, song.octave); bad++; }
            if ((k == 0u || k == 2u || k == 10u) && (fmini_patch[0][FG_SW1] & FS_MOSC)) { printf("patch %u: MOD OSC is off\n", k); bad++; }
            if (k == 0u && (song.octave != 1 || TSEL->p[P_E0] != 61 || fmini_patch[0][FG_GLID] != 50)) {      /* CUTOFF -0.2 (the ladder's dial, 1.7.0), GLIDE 3.9 (-10 %), OCT+ */
                printf("patch 0: octave %d cutoff %d glide %d\n", song.octave, TSEL->p[P_E0], fmini_patch[0][FG_GLID]); bad++;
            }
            if (k == 3u && (TSEL->p[P_E0] != 37 || TSEL->p[P_REV] != 90 || song.g[G_RTYPE] != 4)) {          /* 1.14.5: PAD SWELL (the 1.14.4 DEEP WELL copy): CUTOFF -2.1 (byte 37), REVERB 9.0, CLOUD */
                printf("patch 3: cutoff %d reverb %d type %d\n", TSEL->p[P_E0], TSEL->p[P_REV], song.g[G_RTYPE]); bad++;
            }
        }
        if (on < 44u || on > 56u) { printf("bank: ARP on in %u of 200 patches\n", on); bad++; }
        if (bad) return 1;
        song.g[G_BPM] = bpm0;                            /* (a Program Change brings the same: ui.c midi_pc_load) */
        ui.mpc[0] = (uint8_t)(0x80u | 14u);              /* 14 RAVE ARP */
        midi_pc_poll();
        if (TSEL->p[P_E7] != 14 || TSEL->p[P_AMODE] != FMINI_X[14].arp[1] || song.g[G_RSIZE] != FMINI_X[14].fx[1] || song.octave != 0) {
            printf("Program Change 14: slot %d arp %d\n", TSEL->p[P_E7], TSEL->p[P_AMODE]); return 1;
        }
        ui.mpc[0] = (uint8_t)(0x80u | 35u);              /* 35 a bass: OCT-1 (the brass 50-59 are OCT 0 since 1.14.1) */
        midi_pc_poll();
        if (TSEL->p[P_E7] != 35 || song.octave != -1 || TSEL->p[P_AMODE] != FMINI_X[35].arp[1] * FMINI_X[35].arp[0]) { printf("Program Change 35: octave %d\n", song.octave); return 1; }
        song.octave = 0;
        TSEL->p[P_AMODE] = 0;
        TSEL->user = 0;
        apply_preset_to(TSEL, 0);
        TSEL->p[P_E7] = 0;
        fa_factory_extras(TSEL, 0);
        TSEL->p[P_AMODE] = 0;
    }
    {   /* a new preset keeps the screen the player is on (any page, the ARP pop-up as it was): PRESETS, a Program Change, a slot change */
        uint32_t pgi;
        panel_init();
        fa_init();
        fa_gl = 0; fa_ar = 0; fa_sv = 0; fa_sq = 0;
        fframes(160);
        for (pgi = 1; pgi < 7u; pgi++) {
            fa_page = (uint8_t)pgi;
            fmg_force_page = (int8_t)pgi;
            fa_ar = pgi == 3u;                           /* (the ARP pop-up open on one of them) */
            fturn(EN_PRESET, 1);
            if (fa_page != pgi || fa_ar != (pgi == 3u)) { printf("PRESET: PRESETS on page %u -> page %d, arp pop-up %d\n", pgi, fa_page, fa_ar); return 1; }
        }
        fa_page = 4;
        fmg_force_page = 4;
        ui.mpc[0] = (uint8_t)(0x80u | 33u);
        fframes(48);
        if (TSEL->p[P_E7] != 33 || fa_page != 4) { printf("PRESET: Program Change -> slot %d page %d\n", TSEL->p[P_E7], fa_page); return 1; }
        fa_page = 2;
        fmg_force_page = 2;
        fframes(48);
        if (fa_page != 2) { printf("HOME: no preset change, but the page moved (%d)\n", fa_page); return 1; }
        fa_page = 0;
        fmg_force_page = 0;
    }
    {   /* power-on with the autosaved patch 00 (the track keeps its sound, the octave is not autosaved): OCT+ is on, as the preset says */
        panel_init();
        TSEL->user = 0;
        apply_preset_to(TSEL, 0);
        TSEL->p[P_E7] = 0;
        TSEL->eng_req = ENGI_FMINI;
        song.octave = 0;
        fa_init();
        if (song.octave != FMINI_X[0].oct || song.octave != 1) { printf("power-on: patch 00 octave %d\n", song.octave); return 1; }
        TSEL->p[P_E7] = 35;
        fa_init();
        if (song.octave != FMINI_X[35].oct) { printf("power-on: patch 35 octave %d\n", song.octave); return 1; }
        song.octave = 0;
    }
    {   /* a CTRL / OSC / MIX knob edits the patch (SLOT becomes OWN): PRESETS still goes on from the patch it came from (13 -> 14, then 14 -> 13) */
        static const uint8_t CASE[5][2] = {{0, 0}, {0, 1}, {0, 2}, {1, 3}, {2, 0}};     /* TUNE, GLIDE, MOD MIX; OSC DET 2; MIX OSC 1 level */
        uint32_t c;
        panel_init();
        fa_init();
        fa_gl = 0; fa_ar = 0; fa_sv = 0; fa_sq = 0;
        for (c = 0; c < 5u; c++) {
            TSEL->user = 0;
            apply_preset_to(TSEL, 13);
            TSEL->p[P_E7] = 13;
            fa_factory_extras(TSEL, 13);
            fmini_poll();
            fa_page = CASE[c][0];
            fmg_force_page = (int8_t)fa_page;
            fframes(48);
            fturn(EN_K1 + CASE[c][1], 3);
            fframes(48);
            if (TSEL->p[P_E7] != (int16_t)FMINI_OWN) { printf("edit %u: the slot is %d, not OWN\n", c, TSEL->p[P_E7]); return 1; }
            fturn(EN_PRESET, 1);
            fframes(48);
            if (TSEL->p[P_E7] != 14 || fa_page != CASE[c][0]) { printf("edit %u: PRESETS from 13 -> %d (page %d), not 14 on page %u\n", c, TSEL->p[P_E7], fa_page, CASE[c][0]); return 1; }
            fa_page = CASE[c][0];
            fmg_force_page = (int8_t)fa_page;
            fturn(EN_K1 + CASE[c][1], 3);
            fframes(48);
            fturn(EN_PRESET, -1);
            fframes(48);
            if (TSEL->p[P_E7] != 13) { printf("edit %u: PRESETS from 14 -> %d, not 13\n", c, TSEL->p[P_E7]); return 1; }
        }
        fa_page = 0;
        fmg_force_page = 0;
        TSEL->user = 0;
        apply_preset_to(TSEL, 0);
        TSEL->p[P_E7] = 0;
        fa_factory_extras(TSEL, 0);
        fmini_poll();
    }
    {   /* GLIDE: the knob alone makes the track glide; ARP pop-up changes the arp; HOME */
        int i;
        fa_page = 0;
        fmg_force_page = 0;
        fmini_load_slot(0, 0);
        TSEL->p[P_E7] = 0;
        fmg_glide_sync();
        for (i = 0; i < 10; i++)
            fmg_knob(1, 2);
        if (TSEL->p[P_GLIDE] <= 0) { printf("glide knob does not glide (%d)\n", TSEL->p[P_GLIDE]); return 1; }
        fa_ar = 1;
        fa_ar_turn(0, 1);                               /* MODE: OFF -> UP */
        fa_ar_turn(1, 1);
        fa_ar_turn(4, 1);                               /* HOLD on */
        if (!TSEL->p[P_AHOLD]) { printf("arp hold\n"); return 1; }
        fa_ar_turn(5, 5);
        if (TSEL->p[P_AMODE] != 1) { printf("arp mode\n"); return 1; }
        ui.force = 1;
        fa_draw();
        dump(dir, 12);
        fa_ar = 0;
        fa_page = 5;
        fa_goto(0);
        if (fa_page != 0) { printf("home\n"); return 1; }
    }
    {   /* GLO: MIDI IN / CHANNEL filter, every control answers to its CC, the pop-ups draw */
        uint32_t n, m, bad = 0;
        fa_cfg_load();
        for (n = 0; n < FA_NCC; n++) {
            if (!fa_cc[n] || !fa_cc_valid(fa_cc[n])) { printf("cc default %u\n", n); return 1; }
            for (m = n + 1u; m < FA_NCC; m++)
                if (fa_cc[n] == fa_cc[m]) { printf("cc %u twice (%u, %u)\n", fa_cc[n], n, m); return 1; }
        }
        fmini_load_slot(0, 0);
        for (n = 0; n < FA_NCC; n++) {                  /* a CC of each control moves it */
            uint32_t h0 = 0, h1 = 0, i;
            fa_ctl_apply(n, 0);
            for (i = 0; i < FG_SIZE; i++) h0 = h0 * 31u + fmini_patch[0][i];
            for (i = 0; i < P_COUNT; i++) h0 = h0 * 31u + (uint32_t)TSEL->p[i];
            for (i = 0; i < G_COUNT; i++) h0 = h0 * 31u + (uint32_t)song.g[i];
            fa_ctl_apply(n, 127);
            for (i = 0; i < FG_SIZE; i++) h1 = h1 * 31u + fmini_patch[0][i];
            for (i = 0; i < P_COUNT; i++) h1 = h1 * 31u + (uint32_t)TSEL->p[i];
            for (i = 0; i < G_COUNT; i++) h1 = h1 * 31u + (uint32_t)song.g[i];
            if (h0 == h1) { const char *tg; char nm[16]; fa_ctl_name(n, &tg, nm); printf("control %u (%s %s) ignores its CC\n", n, tg, nm); bad++; }
        }
        if (bad) return 1;
        /* via the ISR's notes: CC74 -> CUTOFF */
        fa_cc_i = 0;
        fa_gl = 0;
        TSEL->p[P_E0] = 10;
        fm_cc_rx[74] = 100;
        fm_cc_pend[74 >> 5] |= 1u << (74 & 31);
        fa_cc_poll();
        if (TSEL->p[P_E0] == 10) { printf("CC74 did not reach the cutoff\n"); return 1; }
        /* MIDI LEARN: in the MIDI CC list the next free CC is taken for the highlighted line (a CC that line did not own
         * is taken from the line that had it); a reserved CC (CC1 the mod wheel) is refused */
        fa_gl = 2;
        fa_cc_i = 3;
        fa_cc[5] = 80;
        fm_cc_rx[80] = 5;
        fm_cc_pend[(80) >> 5] |= 1u << (80 & 31);
        fa_cc_poll();
        if (fa_cc[3] != 80 || fa_cc[5] != 0) { printf("learn: %u (line 5 %u)\n", fa_cc[3], fa_cc[5]); return 1; }
        fm_cc_pend[(1) >> 5] |= 1u << (1 & 31);
        fa_cc_poll();
        if (fa_cc[3] == 1) { printf("learn took the mod wheel\n"); return 1; }
        fa_gl = 0;
        {   /* PITCH BEND and the MOD WHEEL (CC1) from USB and from TRS: the bend's target and the depth follow */
            fm_midi_src = 3; fm_midi_ch = 0; fm_bend_mode = 0;
            midi_enqueue(0xEu | 0xE0u << 8 | 0x00u << 16 | 0x40u << 24, 1u); events_block(CTL);      /* centre */
            if (midi_bend_target[0] != 0) { printf("bend centre (USB) %d\n", midi_bend_target[0]); return 1; }
            midi_enqueue(0xEu | 0xE0u << 8 | 0x7Fu << 16 | 0x7Fu << 24, 1u); events_block(CTL);      /* full up, USB */
            if (midi_bend_target[0] != 512) { printf("bend up (USB) %d\n", midi_bend_target[0]); return 1; }
            um_byte(0xE0); um_byte(0x00); um_byte(0x00); events_block(CTL);                           /* down, TRS */
            if (midi_bend_target[0] != -512) { printf("bend down (TRS) %d\n", midi_bend_target[0]); return 1; }
            midi_enqueue(0xEu | 0xE0u << 8 | 0x00u << 16 | 0x40u << 24, 2u); events_block(CTL);      /* centre, TRS */
            if (midi_bend_target[0] != 0) { printf("bend centre (TRS) %d\n", midi_bend_target[0]); return 1; }
            midi_enqueue(0xBu | 0xB0u << 8 | 1u << 16 | 127u << 24, 1u); events_block(CTL);          /* mod wheel 127, USB */
            if (trk[0].mw != 127) { printf("mod wheel (USB) %u\n", trk[0].mw); return 1; }
            um_byte(0xB0); um_byte(1); um_byte(0);                                                   /* mod wheel 0, TRS */
            events_block(CTL);
            if (trk[0].mw != 0) { printf("mod wheel (TRS) %u\n", trk[0].mw); return 1; }
        }
        {   /* DET 2 / 3 (1.14.12): the cents, -700 .. +700 in steps of 1, 0.00 at the middle; a CC is the coarse law,
             * (v - 64) * 11 cents, and the coarse byte follows the knob's cents */
            uint32_t pg = 1, i, f2 = 0;
            const char *lb = 0, *un = 0;
            char val[16];
            int32_t ratio = -1;
            uint32_t ic = 0;
            for (i = 0; i < fmg_count(pg); i++)
                if (fmg_field(pg, i)->a == FG_F2) f2 = i;
            song.sel = 0;
            fmini_patch[0][FG_DC2L] = 0; fmini_patch[0][FG_DC2H] = 0;
            if (fmini_det(fmini_patch[0], FG_DC2L) != 0) { printf("DET centre %d\n", (int)fmini_det(fmini_patch[0], FG_DC2L)); return 1; }
            fmg_card_at(pg, f2, -1, &lb, val, &un, &ratio, &ic);
            if (ratio != 500 || val[0] != '0' || val[1] != 0) { printf("DET centre shown %d '%s'\n", (int)ratio, val); return 1; }
            fmg_turn(pg, f2, 1, EN_K1);
            if (fmini_det(fmini_patch[0], FG_DC2L) <= 0) { printf("DET turn up %d\n", (int)fmini_det(fmini_patch[0], FG_DC2L)); return 1; }
            fmg_det_cc(FG_F2, 127);
            if (fmini_det(fmini_patch[0], FG_DC2L) != 693 || fmini_patch[0][FG_F2] != 127) { printf("DET CC top %d\n", (int)fmini_det(fmini_patch[0], FG_DC2L)); return 1; }
            fmg_det_cc(FG_F2, 0);
            if (fmini_det(fmini_patch[0], FG_DC2L) != -704) { printf("DET CC bottom %d\n", (int)fmini_det(fmini_patch[0], FG_DC2L)); return 1; }
            fmini_det_put(fmini_patch[0], FG_DC2L, 700);
            fmg_turn(pg, f2, 100, EN_K1);
            if (fmini_det(fmini_patch[0], FG_DC2L) != 700) { printf("DET clamp %d\n", (int)fmini_det(fmini_patch[0], FG_DC2L)); return 1; }
            fmg_card_at(pg, f2, -1, &lb, val, &un, &ratio, &ic);
            if (ratio != 1000 || val[0] != '+' || val[1] != '7' || val[2] != '.' || val[3] != '0' || val[4] != '0') { printf("DET top shown %d '%s'\n", (int)ratio, val); return 1; }
            fmini_patch[0][FG_DC2L] = 0; fmini_patch[0][FG_DC2H] = 0; fmini_patch[0][FG_F2] = 64;
        }
        {   /* 1.20.2 (owner): TUNE moves 0.1 a detent on its 0.0 .. 10.0 dial, slow or fast (no acceleration: the turns
             * here all come in the same ms), every tenth shown; 5.0 is byte 64 (A 440), the ends 0 and 127 */
            uint32_t pg = 0, i, ft = 99;
            const char *lb = 0, *un = 0;
            char val[16], want[16];
            int32_t ratio = -1, k;
            uint32_t ic = 0;
            for (i = 0; i < fmg_count(pg); i++)
                if (fmg_field(pg, i)->kind == FK_BYTE && fmg_field(pg, i)->a == FG_TUNE) ft = i;
            if (ft == 99) { printf("TUNE: not on CTRL\n"); return 1; }
            song.sel = 0;
            fmini_patch[0][FG_TUNE] = 64;
            for (k = 0; k <= 100; k++) {                 /* 5.0 up to 10.0, then down to 0.0, then up to 5.0 */
                int32_t d = k <= 50 ? 50 + k : 150 - k;
                if (k) fmg_turn(pg, ft, k <= 50 ? 1 : -1, EN_K1);
                fmg_card_at(pg, ft, -1, &lb, val, &un, &ratio, &ic);
                fa_val(val, ratio, fmg_field(pg, ft));
                fa_num(want, d * 10);
                if (ratio != d * 10 || strcmp(val, want)) { printf("TUNE step %d: shown %d '%s', want '%s'\n", (int)k, (int)ratio, val, want); return 1; }
            }
            for (k = 0; k < 50; k++) fmg_turn(pg, ft, -1, EN_K1);
            fmg_card_at(pg, ft, -1, &lb, val, &un, &ratio, &ic);
            if (fmini_patch[0][FG_TUNE] != 0 || ratio != 0) { printf("TUNE bottom %d\n", fmini_patch[0][FG_TUNE]); return 1; }
            fmg_turn(pg, ft, -1, EN_K1);
            if (fmini_patch[0][FG_TUNE] != 0) { printf("TUNE below 0\n"); return 1; }
            for (k = 0; k < 50; k++) fmg_turn(pg, ft, 1, EN_K1);
            if (fmini_patch[0][FG_TUNE] != 64) { printf("TUNE 5.0 is byte %d, not 64\n", fmini_patch[0][FG_TUNE]); return 1; }
            for (k = 0; k < 50; k++) fmg_turn(pg, ft, 1, EN_K1);
            if (fmini_patch[0][FG_TUNE] != 127) { printf("TUNE 10.0 is byte %d, not 127\n", fmini_patch[0][FG_TUNE]); return 1; }
            fmini_patch[0][FG_TUNE] = 64;
        }
        fa_gl = 0;
        /* the filter: USB 1, MIDI 2, BT 3 */
        fm_midi_src = 0; fm_midi_ch = 0;
        fm_bt = 1;
        if (!fm_midi_ok(1, 5) || fm_midi_ok(2, 5) || fm_midi_ok(3, 5)) { printf("src USB\n"); return 1; }
        fm_midi_src = 1;
        if (fm_midi_ok(1, 5) || !fm_midi_ok(2, 5)) { printf("src MIDI\n"); return 1; }
        fm_midi_src = 3;
        if (!fm_midi_ok(1, 0) || !fm_midi_ok(2, 0) || !fm_midi_ok(3, 0)) { printf("src ALL\n"); return 1; }
        fm_midi_ch = 3;
        if (!fm_midi_ok(1, 2) || fm_midi_ok(1, 3) || fm_midi_ok(1, 0)) { printf("channel 3\n"); return 1; }
        fm_midi_ch = 0;
        /* the pop-ups */
        fa_page = 1;
        fa_gl = 1; fa_gl_row = 0; fm_midi_src = 3; ui.force = 1; fa_draw(); dump(dir, 13);
        fa_gl = 2; fa_cc_i = 20; ui.force = 1; fa_draw(); dump(dir, 14);
        fa_gl = 3; ui.force = 1; fa_draw(); dump(dir, 15);
        fa_gl = 0; fm_midi_src = 3;
        fa_ar = 1; ui.force = 1; fa_draw(); dump(dir, 16);
        fa_ar = 0;
    }
    {   /* preset number always valid, the pop-up, pitch bend ranges, mod wheel */
        char num[8], nm[16];
        uint32_t k;
        fa_init();
        fa_gl = 0; fa_ar = 0; fa_sv = 0;
        apply_preset_to(TSEL, 42);
        TSEL->p[P_E7] = 42;
        fmini_poll();
        fa_track_preset();
        fa_preset_label(num, nm);
        if (strcmp(num, "042") || !fa_pp_on()) { printf("label %s pop %d\n", num, fa_pp_on()); return 1; }
        ui.force = 1; fa_draw(); dump(dir, 17);
        fm1_ms += 1600;
        if (fa_pp_on()) { printf("pop-up stays\n"); return 1; }
        fmg_write(FG_TUNE, 70);                          /* edited: SLOT OWN, the number stays */
        fa_track_preset();
        fa_preset_label(num, nm);
        if (strcmp(num, "042") || fa_pp_on() || nm[0] == 0) { printf("edited label %s\n", num); return 1; }
        for (k = 0; k < FMINI_NSLOT; k++) {               /* every number 000..199 */
            TSEL->p[P_E7] = (int16_t)k;
            fa_track_preset();
            fa_preset_label(num, nm);
            if (num[0] < '0' || num[0] > '9' || num[1] < '0' || num[1] > '9' || num[2] < '0' || num[2] > '9' || num[3]) { printf("label %u\n", k); return 1; }
        }
        for (k = 0; k < 4; k++) {                        /* GLO > PITCH BEND */
            static const int UP[4] = {2, 12, 2, 2}, DN[4] = {2, 12, 12, 24};
            fm_bend_mode = (uint8_t)k;
            midi_event(0xE0u, 0, 127, 127);
            if (midi_bend_target[0] != UP[k] * 256) { printf("bend up mode %u: %d\n", k, midi_bend_target[0]); return 1; }
            midi_event(0xE0u, 0, 0, 0);
            if (midi_bend_target[0] != -DN[k] * 256) { printf("bend down mode %u: %d\n", k, midi_bend_target[0]); return 1; }
            midi_event(0xE0u, 0, 0, 64);
            if (midi_bend_target[0] != 0) { printf("bend centre\n"); return 1; }
        }
        midi_event(0xB0u, 0, 1, 127);                    /* the mod wheel (CC1) reaches the track */
        if (trk[0].mw != 127) { printf("mod wheel not stored (%d)\n", trk[0].mw); return 1; }
        midi_event(0xB0u, 0, 1, 0);
        if (trk[0].mw != 0) { printf("mod wheel back\n"); return 1; }
    }
    {   /* buttons: ARP tap / hold, SEQ + REC + PLAY, the lit keys, GLO fixes */
        panel_init();
        uint32_t sig, k, i;
        fa_init();
        trk[0].p[P_AMODE] = 0; trk[0].p[P_AHOLD] = 0; fa_ar = 0;
        fpress(B_ARP);
        if (trk[0].p[P_AMODE] != 1 || !fa_ar) { printf("arp tap on (%d) down=%d done=%d btn=%u pressedmask=%u\n", trk[0].p[P_AMODE], fa_arp_down, fa_arp_done, panel.btn[B_ARP], host_pressed); return 1; }
        fpress(B_ARP);
        if (trk[0].p[P_AMODE] != 0) { printf("arp tap off\n"); return 1; }
        fpress(B_ARP);
        fhold(B_ARP);
        if (!trk[0].p[P_AHOLD] || trk[0].p[P_AMODE] != 1) { printf("arp hold: hold %d mode %d\n", trk[0].p[P_AHOLD], trk[0].p[P_AMODE]); return 1; }
        fhold(B_ARP);
        if (trk[0].p[P_AHOLD]) { printf("arp hold off\n"); return 1; }
        fframes(4500);                                   /* no timeout: the pop-up stays until it is closed */
        if (!fa_ar) { printf("arp pop-up closed by itself\n"); return 1; }
        fpress(B_HOME);                                  /* HOME closes it; the arpeggio stays on */
        if (fa_ar || trk[0].p[P_AMODE] != 1) { printf("home closes arp (%d)\n", trk[0].p[P_AMODE]); return 1; }
        fpress(B_ARP);                                   /* on, off the pop-up: the tap only opens it */
        if (!fa_ar || trk[0].p[P_AMODE] != 1) { printf("arp tap opens (%d)\n", trk[0].p[P_AMODE]); return 1; }
        fpress(B_ARP);                                   /* on the pop-up: the tap turns it off */
        if (trk[0].p[P_AMODE] != 0 || !fa_ar) { printf("arp tap off on the pop-up\n"); return 1; }
        fpress(B_ARP);
        if (trk[0].p[P_AMODE] != 1) { printf("arp tap on\n"); return 1; }
        {   /* TEMPO row: ALGORITHM opens TAP TEMPO; EDIT taps (about 120 BPM); another button closes it and does nothing else */
            int8_t oc = song.octave;
            int16_t bpm0 = song.g[G_BPM];
            fa_ar_row = 5;
            fturn(EN_ALGO, 1);
            if (!fa_tp) { printf("tempo pop-up\n"); return 1; }
            fpress(B_EDIT);
            fframes(500);
            fpress(B_EDIT);
            if (song.g[G_BPM] < 110 || song.g[G_BPM] > 130) { printf("tap tempo %d\n", song.g[G_BPM]); return 1; }
            fpress(B_OCTUP);
            if (fa_tp || !fa_ar || song.octave != oc) { printf("tempo pop-up closes (%d)\n", fa_tp); return 1; }
            fturn(EN_ALGO, 1);
            if (!fa_tp) { printf("tempo pop-up again\n"); return 1; }
            fturn(EN_SELECT, 1);                         /* SELECT closes it too */
            if (fa_tp || !fa_ar) { printf("tempo pop-up select\n"); return 1; }
            fa_ar_row = 0;
            song.g[G_BPM] = bpm0;                        /* (the tests after this one play at the tempo they had) */
        }
        {   /* TAP TEMPO smooths: the first two taps set the tempo, then the mean beat of the last five taps (32 ms of each
             * press is in the gaps: a gap of 480 ms is a tap 512 ms after the last one) */
            int16_t bpm0 = song.g[G_BPM];
            fa_ar_row = 5;
            song.g[G_BPM] = 120;
            fturn(EN_ALGO, 1);                           /* opens TAP TEMPO: its measurement starts over */
            song.g[G_BPM] = 120;
            fpress(B_EDIT);                              /* the first tap sets no tempo */
            if (song.g[G_BPM] != 120) { printf("tap first %d\n", song.g[G_BPM]); return 1; }
            fframes(480); fpress(B_EDIT);                /* two taps: one beat of 512 ms */
            if (song.g[G_BPM] < 116 || song.g[G_BPM] > 118) { printf("tap two %d\n", song.g[G_BPM]); return 1; }
            for (i = 0; i < 3; i++) { fframes(480); fpress(B_EDIT); }
            if (song.g[G_BPM] < 116 || song.g[G_BPM] > 118) { printf("tap steady %d\n", song.g[G_BPM]); return 1; }
            fframes(288); fpress(B_EDIT);                /* a hurried tap, 320 ms after the last: the mean moves a little (not to 187) */
            if (song.g[G_BPM] < 128 || song.g[G_BPM] > 131) { printf("tap jitter %d\n", song.g[G_BPM]); return 1; }
            fframes(80); fpress(B_EDIT);                 /* a bounce, 112 ms after the last: no tap */
            if (song.g[G_BPM] < 128 || song.g[G_BPM] > 131 || !fa_tp) { printf("tap bounce %d\n", song.g[G_BPM]); return 1; }
            fframes(2000); fpress(B_EDIT);               /* a pause: a new measurement, no tempo yet */
            if (song.g[G_BPM] < 128 || song.g[G_BPM] > 131) { printf("tap pause %d\n", song.g[G_BPM]); return 1; }
            fframes(480); fpress(B_EDIT);
            if (song.g[G_BPM] < 116 || song.g[G_BPM] > 118) { printf("tap again %d\n", song.g[G_BPM]); return 1; }
            fpress(B_OCTUP);                             /* another button closes it */
            if (fa_tp) { printf("tap close\n"); return 1; }
            song.g[G_BPM] = bpm0;
            fa_ar_row = 0;
        }
        trk[0].p[P_AMODE] = 0;
        /* the sequencer */
        fpress(B_SEQ);
        if (!fa_sq) { printf("seq pop-up\n"); return 1; }
        fturn(EN_ALGO, 1);                               /* SEQ: ALGORITHM opens TAP TEMPO as well */
        if (!fa_tp || !fa_sq) { printf("seq tempo pop-up\n"); return 1; }
        fturn(EN_SELECT, 1);
        if (fa_tp) { printf("seq tempo select\n"); return 1; }
        fpress(B_REC);
        if (!fa_sq_rec) { printf("rec\n"); return 1; }
        input_on(&trk[0], 60, 100); input_on(&trk[0], 64, 100); input_on(&trk[0], 67, 100);
        trk_all_off(&trk[0]);
        if (fa_sq_cur != 3 || trk[0].step[0].note[0] != 60 || trk[0].step[2].note[0] != 67 || trk[0].p[P_SLEN] != 3) { printf("seq record %d\n", fa_sq_cur); return 1; }
        fpress(B_REC);
        if (fa_sq_rec) { printf("rec off\n"); return 1; }
        song.g[G_BPM] = 240;                             /* the step count below is timed at 240 BPM, as the test always had it (the patches set their own tempo, 1.14.25) */
        fpress(B_PLAY);
        if (transport_req != 1) { printf("play request\n"); return 1; }
        for (i = 0; i < 400; i++) { events_block(CTL); mix_block((int32_t[2 * CTL]){0}, CTL); }
        if (!song.playing) { printf("not playing\n"); return 1; }
        {   uint32_t seen = 0, lit = 0;
            for (i = 0; i < 1200; i++) {
                int32_t o[2 * CTL];
                mix_block(o, CTL);
                seen |= 1u << (trk[0].seq_idx % 3u);
                lit |= fa_sound_keys();
            }
            if (seen != 7u) { printf("seq steps seen %u\n", seen); return 1; }
            if (!lit) { printf("no key lit while playing\n"); return 1; }
        }
        ui.force = 1; fa_draw(); dump(dir, 18);
        fpress(B_PLAY);
        events_block(CTL);
        if (song.playing) { printf("stop\n"); return 1; }
        {   /* SEQ: outside the sequencer a press (not the release) opens the grid, a hold does nothing more; inside it a hold
             * opens the settings pop-up (MODE, RATE, GATE, TEMPO), a tap closes the grid, and a press on the settings goes back */
            int16_t dv0 = trk[0].p[P_SDIV], g0 = trk[0].p[P_SGATE], md0 = trk[0].p[P_SMODE], bpm0 = song.g[G_BPM];
            fa_sq = 0; fa_sc = 0; fa_tp = 0; fa_sq_rec = 0;
            fhold(B_SEQ);                                 /* outside: the grid, the hold adds nothing */
            if (!fa_sq || fa_sc) { printf("seq opens by hold outside %d %d\n", fa_sq, fa_sc); return 1; }
            fa_sq = 0;
            fpress(B_SEQ);                                /* a press outside: the grid */
            if (!fa_sq || fa_sc) { printf("seq opens %d %d\n", fa_sq, fa_sc); return 1; }
            fa_sq_title_until = 0;
            fhold(B_SEQ);                                 /* inside: the hold opens the settings pop-up */
            if (!fa_sc || fa_sq) { printf("seq hold %d %d\n", fa_sc, fa_sq); return 1; }
            fa_sc_row = 0;
            fturn(EN_ALGO, 1);                            /* MODE: FWD -> BWD */
            if (trk[0].p[P_SMODE] != SM_BWD) { printf("seq mode %d\n", trk[0].p[P_SMODE]); return 1; }
            fturn(EN_SELECT, 1);
            fturn(EN_ALGO, 1);                            /* RATE: one step up */
            if (trk[0].p[P_SDIV] == dv0) { printf("seq rate\n"); return 1; }
            fturn(EN_SELECT, 1);
            fturn(EN_ALGO, 1);                            /* GATE */
            if (trk[0].p[P_SGATE] == g0) { printf("seq gate\n"); return 1; }
            fturn(EN_SELECT, 1);                          /* TEMPO: ALGORITHM opens TAP TEMPO, and the pop-up stays */
            fturn(EN_ALGO, 1);
            if (!fa_tp || !fa_sc) { printf("seq tempo pop-up\n"); return 1; }
            fpress(B_OCTUP);                              /* another button closes TAP TEMPO only */
            if (fa_tp || !fa_sc) { printf("seq tempo closes\n"); return 1; }
            fpress(B_SEQ);                                /* a press on the settings: back to the grid */
            if (fa_sc || !fa_sq) { printf("seq back to grid (%d %d)\n", fa_sc, fa_sq); return 1; }
            fpress(B_SEQ);                                /* a tap inside the grid: it closes */
            if (fa_sq) { printf("seq grid closes\n"); return 1; }
            trk[0].p[P_SDIV] = dv0; trk[0].p[P_SGATE] = g0; trk[0].p[P_SMODE] = md0; song.g[G_BPM] = bpm0;
        }
        {   /* SEQ MODE: the order the steps play (core.h seq_next_in): FWD 0 1 2 3 0 .., BWD 3 2 1 0 3 .., PING 0 1 2 3 2 1 0 1 */
            static const uint32_t WANT[3][8] = {{0, 1, 2, 3, 0, 1, 2, 3}, {3, 2, 1, 0, 3, 2, 1, 0}, {0, 1, 2, 3, 2, 1, 0, 1}};
            uint32_t m, j;
            for (m = 0; m < 3; m++) {
                int8_t dir = 0;
                uint32_t idx = seq_before_first(m, 4, &dir);
                for (j = 0; j < 8; j++) {
                    idx = seq_next_in(m, idx, 4, &dir);
                    if (idx != WANT[m][j]) { printf("seq order mode %u step %u: %u\n", m, j, idx); return 1; }
                }
            }
        }
        {   /* the sequence plays BWD: the three steps from the last down, as the clock runs */
            uint32_t prev, n = 0, bad = 0;
            int32_t o[2 * CTL];
            trk[0].p[P_SMODE] = SM_BWD;
            fpress(B_PLAY);
            for (i = 0; i < 400; i++) { events_block(CTL); mix_block(o, CTL); }
            prev = trk[0].seq_idx;
            for (i = 0; i < 4000; i++) {
                mix_block(o, CTL);
                if (trk[0].seq_idx != prev) {
                    if (trk[0].seq_idx != (prev + 2u) % 3u)
                        bad++;
                    prev = trk[0].seq_idx;
                    n++;
                }
            }
            fpress(B_PLAY);
            events_block(CTL);
            trk[0].p[P_SMODE] = SM_FWD;
            if (song.playing || n < 4 || bad) { printf("seq bwd: %u steps, %u out of order\n", n, bad); return 1; }
        }
        fpress(B_REC);                                   /* a take of 64 ends by itself */
        for (k = 0; k < 70u; k++) input_on(&trk[0], 50u + k, 100);
        trk_all_off(&trk[0]);
        if (fa_sq_rec || trk[0].p[P_SLEN] != 64 || trk[0].step[63].note[0] != 113) { printf("take of 64 (%d)\n", trk[0].p[P_SLEN]); return 1; }
        ui.force = 1; fa_draw(); dump(dir, 19);
        fhold(B_REC);                                    /* HOLD REC TO CLEAR: the steps go, REC is not toggled */
        for (k = 0; k < NSTEP; k++)
            if (trk[0].step[k].n) { printf("hold rec: step %u kept\n", k); return 1; }
        if (fa_sq_rec || !fa_sq) { printf("hold rec toggled\n"); return 1; }
        fpress(B_SEQ);
        if (fa_sq) { printf("seq close\n"); return 1; }
        /* the keys of a sounding note light */
        trk_note_on(&trk[0], kb_map(&trk[0], 5), 100);
        if (!((fa_sound_keys() >> 5) & 1u)) { printf("key led\n"); return 1; }
        trk_all_off(&trk[0]);
        fa_gl = 1; fa_gl_row = 0; fm_midi_src = 3;
        fturn(EN_ALGO, 1);
        if (fm_midi_src != 3) { printf("src max\n"); return 1; }
        fturn(EN_ALGO, -1); fturn(EN_ALGO, -1); fturn(EN_ALGO, -1);
        if (fm_midi_src != 0) { printf("src min\n"); return 1; }
        fturn(EN_SELECT, -1);
        if (fa_gl_row != 5) { printf("rows wrap\n"); return 1; }
        /* GLO: PITCH BEND draws at once, ABOUT closes with either encoder */
        fa_gl = 1; fa_gl_row = 4; fm_bend_mode = 0;
        sig = fa_signature();
        fturn(EN_ALGO, 1);
        if (fm_bend_mode != 1 || fa_signature() == sig) { printf("bend redraw\n"); return 1; }
        /* GLO order: MIDI IN, CHANNEL, MIDI CC, MIDI TEMPO SYNC, PITCH BEND, ABOUT */
        fa_gl = 1; fa_gl_row = 1; fturn(EN_SELECT, 1);
        if (fa_gl_row != 2) { printf("MIDI CC is row 2\n"); return 1; }
        ui.force = 1; fa_draw(); dump(dir, 40);
        fturn(EN_ALGO, 1);
        if (fa_gl != 2) { printf("MIDI CC opens\n"); return 1; }
        fa_gl = 1; fa_gl_row = 2; fturn(EN_SELECT, 1);
        if (fa_gl_row != 3) { printf("MIDI TEMPO SYNC follows MIDI CC\n"); return 1; }
        /* MIDI CLOCK: OFF by default; ALGORITHM cycles OFF, USB, TRS (the global CLK); the setting is kept in the settings record */
        if (fa_clk) { printf("clock on at start\n"); return 1; }
        fa_gl = 1; fa_gl_row = 3;
        fturn(EN_ALGO, 1);
        if (fa_clk != 1 || song.g[G_CLOCK] != 1) { printf("clock USB (%d)\n", song.g[G_CLOCK]); return 1; }
        fturn(EN_ALGO, 1);
        if (fa_clk != 2 || song.g[G_CLOCK] != 2) { printf("clock TRS (%d)\n", song.g[G_CLOCK]); return 1; }
        fturn(EN_ALGO, 1);
        if (fa_clk != 0 || song.g[G_CLOCK] != 0) { printf("clock OFF again (%d)\n", song.g[G_CLOCK]); return 1; }
        fturn(EN_ALGO, -1);
        if (fa_clk != 2 || song.g[G_CLOCK] != 2) { printf("clock back to TRS (%d)\n", song.g[G_CLOCK]); return 1; }
        fa_cfg_dirty = 1; fa_cfg_save(); fa_clk = 0; fa_cfg_load();
        if (fa_clk != 2) { printf("clock kept (%d)\n", fa_clk); return 1; }
        fa_clk = 0; fa_gl = 1; fa_gl_row = 4; fturn(EN_ALGO, 1);
        if (fa_gl != 1 || fa_clk) { printf("pitch bend row\n"); return 1; }
        {   /* MIDI CLOCK: Start / Stop / Continue come from the chosen input only (the engine's clock, through events_block) */
            song.playing = 0; fm1_ms = 9000;
            fa_clk = 1; fa_clk_apply();
            midi_enqueue(0xFu | 0xFAu << 8, 2u); events_block(CTL);
            if (song.playing) { printf("USB clock takes no TRS start\n"); return 1; }
            midi_enqueue(0xFu | 0xFAu << 8, 1u); events_block(CTL);
            if (!song.playing) { printf("USB clock: start\n"); return 1; }
            midi_enqueue(0xFu | 0xFCu << 8, 1u); events_block(CTL);
            if (song.playing) { printf("USB clock: stop\n"); return 1; }
            midi_enqueue(0xFu | 0xFBu << 8, 1u); events_block(CTL);
            if (!song.playing) { printf("USB clock: continue\n"); return 1; }
            midi_enqueue(0xFu | 0xFCu << 8, 1u); events_block(CTL);
            fa_clk = 2; fa_clk_apply();
            midi_enqueue(0xFu | 0xFAu << 8, 1u); events_block(CTL);
            if (song.playing) { printf("TRS clock takes no USB start\n"); return 1; }
            midi_enqueue(0xFu | 0xFAu << 8, 2u); events_block(CTL);
            if (!song.playing) { printf("TRS clock: start\n"); return 1; }
            midi_enqueue(0xFu | 0xFCu << 8, 2u); events_block(CTL);
            if (song.playing) { printf("TRS clock: stop\n"); return 1; }
            fa_clk = 0; fa_clk_apply();
        }
        fa_gl = 1; fa_gl_row = 5; fturn(EN_ALGO, 1);
        if (fa_gl != 3) { printf("ABOUT opens\n"); return 1; }
        fa_gl = 3; fturn(EN_ALGO, 1);
        if (fa_gl != 1) { printf("about ALGO\n"); return 1; }
        fa_gl = 3; fturn(EN_SELECT, 1);
        if (fa_gl != 1) { printf("about SELECT\n"); return 1; }
        fa_gl = 0;
        {   /* FX: the cursor walks onto the four EDIT buttons; ALGORITHM opens each pop-up, K1..K4 edit its four values */
            static const uint8_t PMAP[4][4] = {{P_REV, G_RTYPE, G_RSIZE, G_RDAMP}, {P_DLY, G_DTIME, G_DRREV, G_DFDBK},
                                               {P_CHOR, G_ENGGO, G_CDEPTH, G_CRATE}, {P_DIST, G_INFO, G_DRLVL, G_MIDI}};
            uint32_t q, k;
            fa_ar = 0; fa_page = 6; fmg_cur[6] = 3; fmg_fx = 0;
            for (q = 0; q < 4u; q++) {
                fturn(EN_SELECT, 1); fframes(64);
                if (fmg_cur[6] != 4u + q) { printf("FX cursor %u -> %u\n", q, fmg_cur[6]); return 1; }
                fturn(EN_ALGO, 1); fframes(64);
                if (fmg_fx != q + 1u) { printf("EDIT %u opens pop-up %u\n", q, fmg_fx); return 1; }
                for (k = 0; k < 4u; k++) {
                    int32_t b4 = k == 0u ? TSEL->p[PMAP[q][0]] : song.g[PMAP[q][k]];
                    uint32_t g0;
                    if (k == 0u) TSEL->p[PMAP[q][0]] = 30; else song.g[PMAP[q][k]] = GP[PMAP[q][k]].def;
                    b4 = k == 0u ? TSEL->p[PMAP[q][0]] : song.g[PMAP[q][k]];
                    g0 = fa_signature();
                    fturn(EN_K1 + k, 1); fframes(64);
                    fturn(EN_K1 + k, 1); fframes(64);
                    if ((k == 0u ? TSEL->p[PMAP[q][0]] : song.g[PMAP[q][k]]) == b4) { printf("FX pop-up %u knob %u does not move (%d)\n", q, k, (int)b4); return 1; }
                    if (fa_signature() == g0) { printf("FX pop-up %u knob %u: no redraw\n", q, k); return 1; }
                }
                ui.force = 1; fa_draw(); dump(dir, 20 + (int)q);
                fpress(B_FX);                              /* FX again: the next pop-up (after the last, the page) */
                if (fmg_fx != (q < 3u ? q + 2u : 0u)) { printf("FX cycles %u -> %u\n", q, fmg_fx); return 1; }
                fmg_fx = 0; fmg_cur[6] = (uint8_t)(4u + q);
            }
            {   /* a type's names: the five reverbs, seven delays, three choruses */
                char v[16]; const char *u; uint32_t n;
                static const struct { uint8_t id; uint8_t n; const char *last; } TY[4] = {{G_RTYPE, 5, "CLOUD"}, {G_DTIME, 7, "REVERSE"}, {G_ENGGO, 3, "MULTI"}, {G_INFO, 2, "TUBE"}};
                for (n = 0; n < 4u; n++) {
                    str_cpy(v, GP[TY[n].id].names[GP[TY[n].id].max], 16);
                    if (GP[TY[n].id].max + 1 != TY[n].n || strcmp(v, TY[n].last)) { printf("type list %u: %d '%s'\n", n, GP[TY[n].id].max + 1, v); return 1; }
                }
            }
            {   /* TAP TEMPO (SCL in the delay pop-up): two taps 500 ms apart set the delay TIME to ~500 ms and do not leave the page */
                int32_t ms;
                fmg_fx = 2; fa_page = 6;
                fm1_ms += 3000u; fpress(B_SCL);
                fm1_ms += 500u - 32u; fpress(B_SCL);
                ms = dly_ms(song.g[G_DRREV]);
                if (fa_page != 6 || ms < 480 || ms > 520) { printf("tap tempo: page %d, %d ms\n", fa_page, (int)ms); return 1; }
                fmg_fx = 0;
            }
        }
        fpress(B_SCL);                                   /* SCL: OSC, then MIX, then OSC */
        if (fa_page != 1) { printf("SCL -> OSC (%d)\n", fa_page); return 1; }
        fpress(B_SCL);
        if (fa_page != 2) { printf("SCL -> MIX (%d)\n", fa_page); return 1; }
        fpress(B_SCL);
        if (fa_page != 1) { printf("SCL -> OSC again (%d)\n", fa_page); return 1; }
        fpress(B_EDIT);                                  /* EDIT: FILT */
        if (fa_page != 3) { printf("EDIT -> FILT (%d)\n", fa_page); return 1; }
        fpress(B_LFO);                                   /* LFO and HOME: CTRL, both lit */
        fa_leds();
        if (fa_page != 0) { printf("LFO -> CTRL (%d)\n", fa_page); return 1; }
        fpress(B_SCL);
        fpress(B_HOME);
        if (fa_page != 0) { printf("HOME -> CTRL (%d)\n", fa_page); return 1; }
    }
    {   /* 1.8.0: the battery icon bottom right, four bars, red on the last one, a bolt on USB power; no "n/7" any more */
        static const struct { int32_t raw, usb, bars, red, bolt; } B[6] = {
            {0, 0, 4, 0, 0}, {600, 0, 4, 0, 0}, {570, 0, 3, 0, 0}, {540, 0, 2, 0, 0}, {500, 0, 1, 1, 0}, {500, 1, 1, 1, 1}};
        uint32_t k, bad = 0;
        for (k = 0; k < 6u; k++) {
            uint32_t b, red = 0, bolt = 0, lit = 0, i;
            song.batt_raw = B[k].raw;
            usb.config = (uint8_t)B[k].usb;
            usb.suspended = 0;
            fa_page = 3;
            ui.force = 1;
            fa_draw();
            for (b = 0; b < 4u; b++) {                       /* the bar's centre pixel: 4 bars of 4 x 6 px from x 203, y 229..234 */
                uint16_t c = swap16(host_screen[231u * 240u + 203u + 5u * b + 1u]);
                lit += c != swap16(FA_RGB(0x0E, 0x0D, 0x0C)) && c != swap16(FA_RGB(0x1E, 0x1B, 0x19)) && (c >> 11) > 12u;
            }
            for (i = 0; i < 25u * 10u; i++) {
                uint16_t c = host_screen[(227u + i / 25u) * 240u + 201u + i % 25u];
                red += c == swap16(FA_RED);
                bolt += c == swap16(FA_OR);
            }
            if ((int32_t)lit != B[k].bars - (B[k].bolt ? 0 : 0) && !B[k].bolt) { printf("battery raw %d: %u bars lit, wanted %d\n", B[k].raw, lit, B[k].bars); bad++; }
            if ((red > 0u) != (B[k].red != 0)) { printf("battery raw %d: red %u, wanted %d\n", B[k].raw, red, B[k].red); bad++; }
            if ((bolt > 0u) != (B[k].bolt != 0)) { printf("battery raw %d usb %d: bolt pixels %u, wanted %d\n", B[k].raw, B[k].usb, bolt, B[k].bolt); bad++; }
        }
        usb.config = 0;
        song.batt_raw = 0;
        if (bad) return 1;
    }
    {   /* 1.8.1: every CC alone, 0..127, is polled and its pending bit cleared (the bit 31 of a word, CC 31 / 63 / 95 / 127, did not end the loop) */
        uint32_t cc, n, bad = 0;
        for (cc = 0; cc < 128u; cc++) {
            for (n = 0; n < FA_NCC; n++)
                fa_cc[n] = 0;
            fa_cc[5] = (uint8_t)cc;
            fa_gl = 0;
            fm_cc_rx[cc] = 77;
            fm_cc_pend[0] = fm_cc_pend[1] = fm_cc_pend[2] = fm_cc_pend[3] = 0;
            fm_cc_pend[cc >> 5] = 1u << (cc & 31u);
            fa_cc_poll();
            if (fm_cc_pend[0] | fm_cc_pend[1] | fm_cc_pend[2] | fm_cc_pend[3]) { printf("CC %u: its pending bit was not cleared\n", cc); bad++; }
        }
        for (n = 0; n < FA_NCC; n++)
            fa_cc[n] = 0;
        if (bad) return 1;
    }
    printf("fmini_app_test: ok\n");
    {   /* the CUTOFF / EMPH / CNTR knobs glide (200 ms): a click is a ramp of tiny steps, silence snaps */
        track_t *t0 = &trk[0];
        int32_t last, d, n, kk, big;
        uint32_t vi;
        static const uint8_t KN3[3] = {P_E0, P_E1, P_E2};
        for (kk = 0; kk < 3; kk++) {
            int32_t at275 = 0;
            for (vi = 0; vi < NVOICE; vi++)             /* (the patches played before may be polyphonic: their released voices are silent) */
                t0->v[vi].active = 0;
            t0->p[KN3[kk]] = 64;
            fmini_block(t0);
            if (fmini_sm[0][kk] != 64 << 16) { printf("knob glide: silent block does not snap\n"); return 1; }
            t0->v[0].active = 1;
            fmini_block(t0);                        /* the first block of a note follows the knobs at once */
            t0->p[KN3[kk]] = 65;
            last = fmini_sm[0][kk];
            for (n = 0, big = 0; fmini_sm[0][kk] != 65 << 16 && n < 20000; ) {
                fmini_block(t0);
                n++;
                d = fmini_sm[0][kk] - last;
                if (d <= 0 || fmini_sm[0][kk] > 65 << 16) { printf("knob glide: not monotonic\n"); return 1; }
                if (d > 300) big++;
                last = fmini_sm[0][kk];
                if (n == 275) at275 = fmini_sm[0][kk] - (64 << 16);
            }
            /* one time constant (275 blocks = 200 ms) = 63 % of the click */
            if (n >= 20000 || big || at275 < 65536 * 55 / 100 || at275 > 65536 * 72 / 100) {
                printf("knob glide %d: steps %d big %d at 200 ms %d/65536\n", (int)kk, (int)n, (int)big, (int)at275);
                return 1;
            }
            t0->v[0].active = 0;
            fmini_block(t0);
        }
    }
    {   /* the loudness contour is a Minimoog's ADS: the DECAY knob is the decay and the release; SUSTAIN clamps it */
        track_t *t0 = &trk[0];
        voice_t vv;
        int32_t rel[2], dec_ticks[2], dq, rr;
        for (dq = 0; dq < 2; dq++) {
            for (rr = 0; rr < 2; rr++) {
                int n = 0, nd = 0;
                memset(&vv, 0, sizeof vv);
                t0->p[P_ATK] = 40;
                t0->p[P_DEC] = dq ? 90 : 60;
                t0->p[P_SUS] = 64;
                t0->p[P_REL] = rr ? 127 : 0;                 /* no longer matters */
                esync_coef(t0);
                vv.stage = 1;
                while (vv.stage == 1 && n++ < 100000) env_tick(t0, &vv);
                if (vv.stage != 2) { printf("adsr: attack never ends\n"); return 1; }
                while (vv.env > ((int32_t)t0->p[P_SUS] << 17) + (1 << 18) && nd++ < 100000) env_tick(t0, &vv);
                if (nd >= 100000) { printf("adsr: decay does not land on the sustain (%d)\n", (int)vv.env); return 1; }
                { int hold; for (hold = 0; hold < 50; hold++) env_tick(t0, &vv);
                  if (vv.env < ((int32_t)t0->p[P_SUS] << 17) || vv.env > ((int32_t)t0->p[P_SUS] << 17) + (1 << 18)) { printf("adsr: sustain drifts\n"); return 1; } }
                vv.stage = 3;
                n = 0;
                vv.active = 1;
                while (vv.active && n < 200000) { env_tick(t0, &vv); n++; }
                if (rr == 0) { rel[dq] = n; dec_ticks[dq] = nd; }
                else if (n != rel[dq]) { printf("adsr: the REL param still changes the release (%d vs %d)\n", n, (int)rel[dq]); return 1; }
            }
        }
        if (!(rel[1] > rel[0] * 2) || !(dec_ticks[1] > dec_ticks[0] * 2)) {
            printf("adsr: DECAY must set both: release %d/%d decay %d/%d\n", (int)rel[0], (int)rel[1], (int)dec_ticks[0], (int)dec_ticks[1]);
            return 1;
        }
    }
    {   /* both contours reach 12 s at the top of ATTACK and DECAY (the loudness one here; the filter's uses the same tables) */
        track_t *t0 = &trk[0];
        voice_t vv;
        int32_t n = 0, nd = 0;
        double sec;
        memset(&vv, 0, sizeof vv);
        t0->p[P_ATK] = 127; t0->p[P_DEC] = 127; t0->p[P_SUS] = 0; t0->p[P_REL] = 0;
        esync_coef(t0);
        vv.stage = 1;
        while (vv.stage == 1 && n < 2000000) { env_tick(t0, &vv); n++; }
        sec = (double)n * CTL / FS;
        if (sec < 11.0 || sec > 13.0) { printf("attack at max: %.2f s\n", sec); return 1; }
        while (vv.env > (1 << 20) && nd < 4000000) { env_tick(t0, &vv); nd++; }   /* -24 dB: heard until then */
        sec = (double)nd * CTL / FS;
        if (sec < 11.0 || sec > 13.5) { printf("decay at max: %.2f s\n", sec); return 1; }
        vv.stage = 3; vv.env = 1 << 24; vv.active = 1; nd = 0;
        while (vv.env > (1 << 20) && nd < 4000000) { env_tick(t0, &vv); nd++; }
        sec = (double)nd * CTL / FS;
        if (sec < 11.0 || sec > 13.5) { printf("release at max DECAY: %.2f s\n", sec); return 1; }
    }
    {   /* the filter contour: 12 s at the top of ATTACK, and DECAY is its decay and release */
        track_t *t0 = &trk[0];
        voice_t vv;
        int32_t n = 0, e = 0;
        double sec;
        memset(&vv, 0, sizeof vv);
        t0->p[P_E3] = 127; t0->p[P_E4] = 127; t0->p[P_E5] = 0;
        vv.gate = 1;
        vv.s[7] = 1 << 24;                                   /* stage 1 from 0 */
        do { e = fmini_contour(t0->p, &vv); n++; } while (((uint32_t)vv.s[7] >> 24) == 1u && n < 2000000);
        sec = (double)n * CTL / FS;
        if (sec < 11.0 || sec > 13.0) { printf("filter attack at max: %.2f s\n", sec); return 1; }
        vv.gate = 0;
        n = 0;
        do { e = fmini_contour(t0->p, &vv); n++; } while (e > 2048 && n < 4000000);   /* -24 dB (Q15) */
        sec = (double)n * CTL / FS;
        if (((uint32_t)vv.s[7] >> 24) != 3u || sec < 11.0 || sec > 14.5) { printf("filter release at max DECAY: %.2f s (st %u)\n", sec, (unsigned)((uint32_t)vv.s[7] >> 24)); return 1; }
    }
    {   /* the DECAY time is the drop to the SUSTAIN level, whatever it is: the same ~12 s at the top for sustain 0 and 80 % */
        track_t *t0 = &trk[0];
        voice_t vv;
        int32_t sus, nd[2], k;
        for (k = 0; k < 2; k++) {
            int32_t n = 0;
            memset(&vv, 0, sizeof vv);
            t0->p[P_ATK] = 0; t0->p[P_DEC] = 127; t0->p[P_SUS] = k ? 100 : 0; t0->p[P_REL] = 0;
            esync_coef(t0);
            sus = (int32_t)t0->p[P_SUS] << 17;
            vv.stage = 1;
            while (vv.stage == 1) env_tick(t0, &vv);
            while (vv.env - sus > (((1 << 24) - sus) / 16) && n < 4000000) { env_tick(t0, &vv); n++; }
            nd[k] = n;
            if ((double)n * CTL / FS < 11.0 || (double)n * CTL / FS > 13.5) { printf("decay to sustain %d: %.2f s\n", (int)t0->p[P_SUS], (double)n * CTL / FS); return 1; }
        }
    }
    {   /* the idle light show (ANIM IDLE): it starts after 60 s with no input; a key (down and up) stops it and does not
         * bring it back at once (the screen takes every input: scrn.idle), it comes back after 60 s more */
        uint32_t i, bad0 = 0;
        fa_init();
        ui_idle |= 1u;
        fm1_in.notes = fm1_in.buttons = 0;
        scrn.idle = fm1_ms;
        fframes(4000);
        fm1_led_anim_stop();
        for (i = 0; i < 61000u / 16u; i++) { fframe(); fa_leds(); }
        if (!fm1_led_idle_on()) { printf("idle: not on after 61 s\n"); bad0++; }
        fm1_in.notes = host_notes = 1u << 7; fframe(); fa_leds();
        if (fm1_led_idle_on()) { printf("idle: still on with a key down\n"); bad0++; }
        fm1_in.notes = 0; fframe(); fa_leds();
        for (i = 0; i < 2000u / 16u; i++) { fframe(); fa_leds(); }
        if (fm1_led_idle_on()) { printf("idle: back 2 s after a key up\n"); bad0++; }
        for (i = 0; i < 61000u / 16u; i++) { fframe(); fa_leds(); }
        if (!fm1_led_idle_on()) { printf("idle: not back after 60 s with no input\n"); bad0++; }
        if (bad0) return 1;
    }
    return 0;
}
