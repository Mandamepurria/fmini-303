/* SPDX-License-Identifier: GPL-3.0-only
 * ACID (src/eng_acid.c, Smileyface's 303 voice) on the host through hostsim.c.
 *   build/host/acid_test
 * 1. pitch: notes 33, 45, 57 sound at 55, 110, 220 Hz (within 15 cents).
 * 2. level: the engine peaks inside a voice's full scale and is not silent; its RMS against FMINI's is printed.
 * 3. release: the voice is free after the note-off.
 * 4. accent: velocity 127 is louder than 100.
 * 5. slide: a note started while the last is held slides (SLIDE 24 is slower than SLIDE 0); LEGATO and MONO.
 * 6. two parts on two MIDI channels: part 1 ACID on channel 1, part 2 FMINI on channel 2, both sound at once,
 *    each channel plays its own part only; the other engines still render beside it.
 * 7. no hanging notes: random MIDI on channels 1-4 over ACID / FMINI / others, every voice free after the offs. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("acid: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static int32_t last_out[2 * CTL];
static void midi_pkt(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (mi_w - mi_r >= MQ)
        mix_block(last_out, CTL);
    midi_in_q[mi_w++ % MQ] = (st >> 4) | st << 8 | d1 << 16 | d2 << 24;
}

static void setup_acid(uint32_t tr, uint32_t voice_mode)
{
    host_preset(&trk[tr], ENGI_ACID, 0);
    trk[tr].p[P_VOICE] = (int16_t)voice_mode;
    trk[tr].p[P_CHOR] = trk[tr].p[P_DLY] = trk[tr].p[P_REV] = trk[tr].p[P_DIST] = 0;
    trk[tr].p[P_GLIDE] = 0;
}

static void setup(void)
{
    memset(trk, 0, sizeof trk);
    memset(eng_mem, 0, sizeof eng_mem);
    memset(eng_mem_owner, 0, sizeof eng_mem_owner);
    host_tracks_init();
    setup_acid(0, V_LEGATO);
}

static void render(uint32_t tr, double *y, uint32_t n)
{
    static int32_t b[CTL];
    uint32_t i, k;
    for (i = 0; i < n; i += CTL) {
        track_render(&trk[tr], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++)
            y[i + k] = b[k];
    }
}

/* the fundamental by autocorrelation over y[from .. from + len), lag 30 .. 1000 samples, with a parabola */
static double pitch_hz(const double *y, uint32_t from, uint32_t len)
{
    uint32_t lag, best = 30;
    double bestv = -1e30, v[1003], r0 = 0;
    uint32_t i;
    for (i = 0; i < len; i++)
        r0 += y[from + i] * y[from + i];
    if (r0 < 1e3)
        return 0;
    for (lag = 29; lag <= 1001; lag++) {
        double s = 0;
        for (i = 0; i + lag < len; i++)
            s += y[from + i] * y[from + i + lag];
        v[lag + 1u - 29u] = s / (double)(len - lag);
    }
    for (lag = 30; lag <= 1000; lag++) {
        double c = v[lag + 1u - 29u];
        if (c > bestv && c >= v[lag - 29u] && c >= v[lag + 2u - 29u]) {
            bestv = c;
            best = lag;
        }
    }
    for (lag = 30; lag < best; lag++) {                   /* the first peak that is nearly as high: the fundamental */
        double c = v[lag + 1u - 29u];
        if (c > 0.85 * bestv && c >= v[lag - 29u] && c >= v[lag + 2u - 29u]) {
            best = lag;
            break;
        }
    }
    {
        double a = v[best - 29u], b = v[best + 1u - 29u], c = v[best + 2u - 29u];
        double den = a - 2 * b + c, off = den != 0 ? 0.5 * (a - c) / den : 0;
        return (double)FS / ((double)best + off);
    }
}

static double rms(const double *y, uint32_t from, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += y[from + i] * y[from + i];
    return sqrt(s / (double)n);
}
static double peak(const double *y, uint32_t from, uint32_t n)
{
    double m = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        m = fabs(y[from + i]) > m ? fabs(y[from + i]) : m;
    return m;
}

#define SEC (FS)
static double Y[3 * FS];

int main(void)
{
    uint32_t i, k;
    double cents;

    /* 1. pitch */
    {
        static const uint8_t notes[3] = {33, 45, 57};
        int ok = 1;
        double worst = 0;
        for (i = 0; i < 3u; i++) {
            double want = 440.0 * pow(2.0, ((double)notes[i] - 69.0) / 12.0), hz;
            setup();
            trk[0].p[P_E0] = 6;                       /* a dark filter: the fundamental leads */
            trk[0].p[P_E1] = 0;
            trk[0].p[P_E2] = 0;
            trk_note_on(&trk[0], notes[i], 100);
            render(0, Y, FS / 2u);
            hz = pitch_hz(Y, FS / 4u, FS / 8u);
            cents = hz > 0 ? 1200.0 * log2(hz / want) : 9999;
            printf("   note %u: %.2f Hz (want %.2f), %.1f cents\n", notes[i], hz, want, cents);
            worst = fabs(cents) > worst ? fabs(cents) : worst;
            ok &= fabs(cents) < 15;
        }
        check("pitch: notes 33, 45 and 57 sound at 55, 110 and 220 Hz within 15 cents", ok);
    }

    /* 2. level, against FMINI */
    {
        double pa, ra, pf, rf;
        setup();
        trk_note_on(&trk[0], 45, 100);
        render(0, Y, FS);
        pa = peak(Y, FS / 4u, FS / 2u);
        ra = rms(Y, FS / 4u, FS / 2u);
        host_preset(&trk[1], ENGI_FMINI, 0);
        trk[1].p[P_CHOR] = trk[1].p[P_DLY] = trk[1].p[P_REV] = trk[1].p[P_DIST] = 0;
        trk_note_on(&trk[1], 45, 100);
        render(1, Y, FS);
        pf = peak(Y, FS / 4u, FS / 2u);
        rf = rms(Y, FS / 4u, FS / 2u);
        printf("   ACID peak %.0f rms %.0f; FMINI peak %.0f rms %.0f (rms ratio %.2f)\n", pa, ra, pf, rf, ra / (rf > 0 ? rf : 1));
        check("level: ACID sounds, peaks below a voice's full scale x 1.4 (VOICE_FS 24000)", pa > 1500 && pa < 24000 * 1.4);
    }

    /* 3. release */
    {
        uint32_t t = 0;
        setup();
        trk_note_on(&trk[0], 45, 100);
        render(0, Y, FS / 4u);
        trk_note_off(&trk[0], 45);
        while (trk[0].v[0].active && t < FS) {
            render(0, Y, CTL);
            t += CTL;
        }
        printf("   free %.1f ms after the note-off\n", 1000.0 * t / FS);
        check("release: the voice is free within 300 ms of the note-off", !trk[0].v[0].active && t < 300u * FS / 1000u);
    }

    /* 4. accent */
    {
        double a100, a127;
        setup();
        trk_note_on(&trk[0], 45, 100);
        render(0, Y, FS / 2u);
        a100 = peak(Y, 0, FS / 4u);
        setup();
        trk_note_on(&trk[0], 45, 127);
        render(0, Y, FS / 2u);
        a127 = peak(Y, 0, FS / 4u);
        printf("   velocity 100 peak %.0f, 127 (accent) peak %.0f\n", a100, a127);
        check("accent: velocity 127 is louder than 100", a127 > a100 * 1.08);
    }

    /* 5. slide: note 45 held, 57 pressed over it; pitch 15 ms and 400 ms after */
    {
        static const uint32_t modes[2] = {V_LEGATO, V_MONO};
        for (k = 0; k < 2u; k++) {
            double h_fast, h_slow, h_end, h_fend;
            uint32_t s;
            for (s = 0; s < 2u; s++) {
                setup();
                setup_acid(0, modes[k]);
                trk[0].p[P_E0] = 6;
                trk[0].p[P_E1] = 0;
                trk[0].p[P_E2] = 0;
                trk[0].p[P_E5] = s ? 24 : 0;
                trk_note_on(&trk[0], 45, 100);
                render(0, Y, FS / 4u);
                trk_note_on(&trk[0], 57, 100);       /* 45 still held: a slide */
                render(0, Y, FS);
                if (s)
                    h_slow = pitch_hz(Y, 150u * FS / 1000u, 2048);
                else
                    h_fast = pitch_hz(Y, 150u * FS / 1000u, 2048);
                if (s)
                    h_end = pitch_hz(Y, FS - 4096u, 2048);
                else
                    h_fend = pitch_hz(Y, FS - 4096u, 2048);
            }
            printf("   mode %u: SLIDE 0 -> %.1f Hz and SLIDE 24 -> %.1f Hz 150 ms after the second note; SLIDE 24 ends at %.1f Hz\n",
                   modes[k], h_fast, h_slow, h_end);
            check(k ? "slide (MONO): SLIDE 0 is at the new pitch after 150 ms, SLIDE 24 still on its way, then arrives" :
                      "slide (LEGATO): SLIDE 0 is at the new pitch after 150 ms, SLIDE 24 still on its way, then arrives",
                  fabs(1200.0 * log2(h_fast / 220.0)) < 25 && h_slow < h_fast - 10 && fabs(1200.0 * log2(h_end / 220.0)) < 15 &&
                      fabs(1200.0 * log2(h_fend / 220.0)) < 15);
        }
    }

    /* 6. two parts, two MIDI channels: part 1 ACID on channel 1, part 2 FMINI on channel 2 */
    {
        static int32_t o[2 * CTL];
        double e1 = 0, e2 = 0;
        int a_ok, f_ok, only1, only2;
        setup();
        host_preset(&trk[1], ENGI_FMINI, 0);
        song.g[G_ROUTE] = 0;                          /* CH1-4: channel n plays part n */
        midi_pkt(0x90u | 0u, 45, 110);
        midi_pkt(0x90u | 1u, 57, 100);
        for (i = 0; i < FS / 2u / CTL; i++)
            mix_block(o, CTL);
        a_ok = trk[0].v[0].active && trk[0].v[0].note == 45 && trk[0].engine == ENGI_ACID;
        f_ok = trk[1].v[0].active && trk[1].v[0].note == 57 && trk[1].engine == ENGI_FMINI;
        only1 = !trk[1].v[0].active || trk[1].v[0].note == 57;
        only2 = !trk[0].v[0].active || trk[0].v[0].note == 45;
        for (i = 0; i < 2u * CTL; i++)
            e1 += (double)o[i] * o[i];
        printf("   part 1 note %u engine %u (ACID %u), part 2 note %u engine %u (FMINI %u), last block energy %.0f\n",
               trk[0].v[0].note, trk[0].engine, ENGI_ACID, trk[1].v[0].note, trk[1].engine, ENGI_FMINI, e1);
        check("MIDI: channel 1 plays part 1 (ACID), channel 2 plays part 2 (FMINI)", a_ok && f_ok && only1 && only2);
        {   double ea = 0, ef = 0;                       /* each part's own sound: the other one's level to 0 */
            int16_t l0 = trk[0].p[P_LEVEL], l1 = trk[1].p[P_LEVEL];
            trk[1].p[P_LEVEL] = 0;
            for (i = 0; i < 20; i++) { uint32_t q; mix_block(o, CTL); for (q = 0; q < 2u * CTL; q++) ea += (double)o[q] * o[q]; }
            trk[1].p[P_LEVEL] = l1; trk[0].p[P_LEVEL] = 0;
            for (i = 0; i < 20; i++) { uint32_t q; mix_block(o, CTL); for (q = 0; q < 2u * CTL; q++) ef += (double)o[q] * o[q]; }
            trk[0].p[P_LEVEL] = l0;
            printf("   energy of part 1 alone (ACID) %.0f, of part 2 alone (FMINI) %.0f; P_LEVEL %d %d\n", ea, ef, l0, l1);
            check("MIDI: both parts, ACID and FMINI, sound (each with the other silenced)", ea > 1e6 && ef > 1e6);
        }
        midi_pkt(0x80u | 0u, 45, 0);
        midi_pkt(0x80u | 1u, 57, 0);
        for (i = 0; i < FS / CTL; i++)
            mix_block(o, CTL);
        (void)e2;
        check("MIDI: both note-offs release both parts' voices", !trk[0].v[0].active && !trk[1].v[0].active);
    }

    /* 7. random MIDI on channels 1-4, parts ACID / FMINI / ACID / a random other: nothing hangs, no overflow */
    {
        int32_t o[2 * CTL];
        uint8_t on[4][128] = {{0}};
        uint32_t n, c, hung = 0, rs = 12345;
        double mx = 0;
        setup();
        host_preset(&trk[1], ENGI_FMINI, 0);
        host_preset(&trk[2], ENGI_ACID, 2);
        host_preset(&trk[3], ENGI_FM6, 0);
        for (i = 0; i < 4u; i++)
            trk[i].p[P_VOICE] = (int16_t)(i % 2u ? V_MONO : V_LEGATO);
        for (n = 0; n < 6u * FS / CTL; n++) {
            rs = rs * 1664525u + 1013904223u;
            if ((rs >> 24) < 90u) {
                uint32_t ch = (rs >> 8) & 3u, note = 36u + ((rs >> 12) & 31u);
                if (on[ch][note]) {
                    midi_pkt(0x80u | ch, note, 0);
                    on[ch][note] = 0;
                } else {
                    midi_pkt(0x90u | ch, note, 20u + ((rs >> 17) & 107u));
                    on[ch][note] = 1;
                }
            }
            mix_block(o, CTL);
            for (i = 0; i < 2u * CTL; i++)
                mx = fabs((double)o[i]) > mx ? fabs((double)o[i]) : mx;
        }
        for (c = 0; c < 4u; c++)
            for (k = 0; k < 128u; k++)
                if (on[c][k])
                    midi_pkt(0x80u | c, k, 0);
        for (n = 0; n < 4u * FS / CTL; n++)
            mix_block(o, CTL);
        for (i = 0; i < 4u * NVOICE; i++)
            hung += trk[i / NVOICE].v[i % NVOICE].active;
        printf("   output peak %.0f, voices still active after the offs: %u\n", mx, hung);
        check("random MIDI on channels 1-4 over ACID / FMINI / ACID / FM6: every voice free after the note-offs", hung == 0);
        check("random MIDI: the mix never runs away (peak under 4 x 32768)", mx < 4.0 * 32768);
    }

    /* engine switches on a part keep working: ACID -> FMINI -> ACID, notes held */
    {
        static int32_t o[2 * CTL];
        double e = 0;
        setup();
        trk_note_on(&trk[0], 45, 100);
        for (i = 0; i < 40; i++)
            mix_block(o, CTL);
        host_preset(&trk[0], ENGI_FMINI, 0);
        for (i = 0; i < 80; i++)
            mix_block(o, CTL);
        host_preset(&trk[0], ENGI_ACID, 1);
        trk_note_on(&trk[0], 52, 100);
        {   double mxo = 0; uint32_t z = 0;
            for (i = 0; i < 80; i++) {
                uint32_t q;
                mix_block(o, CTL);
                for (q = 0; q < 2u * CTL; q++) mxo = fabs((double)o[q]) > mxo ? fabs((double)o[q]) : mxo;
                z += (mix_l[0] != 0) + (mix_r[3] != 0);
            }
            printf("   80 blocks: max |out| %.0f, mix_l nonzero samples %u, master_q12 %d\n", mxo, z, (int)song.master_q12); }
        for (i = 0; i < 2u * CTL; i++)
            e += (double)o[i] * o[i];
        { acid_t *a = acid_of(&trk[0]);
          printf("   engine %u, last block energy %.0f, voice 0 active %u note %u; a live %u gate %u vca %d xf_on %u mute %d\n", trk[0].engine, e, trk[0].v[0].active,
               trk[0].v[0].note, a->live, a->gate, a->vca, trk[0].xf_on, trk[0].p[P_MUTE]); }
        check("engine switch ACID -> FMINI -> ACID on one part still sounds", e > 1e5 && trk[0].engine == ENGI_ACID);
    }

    printf("acid: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
