/* SPDX-License-Identifier: GPL-3.0-only
 * Adapted from MIDI clock contributions by ChanceTheMaker and keremimo (2026).
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Audio ISR state. Input timestamps keep USB/TRS jitter outside the render loop. */
static struct {
    uint32_t pos, rendered, last_ms, start_ms, rem, interval_ms;
    uint32_t pulse_samples, interp_q8, tempo_ms;
    uint8_t mode, have_pulse, tempo_valid, tempo_n;
    uint16_t q_ms[16];                              /* the last quarter beats (6 clocks each), ms, oldest at q_head - q_n */
    uint32_t q_sum;
    uint8_t q_n, q_head, q_odd;                     /* q_odd: an off quarter waiting (1 longer, 2 shorter than the mean) */
} midi_clock;

/* A quarter beat (6 clocks) of dt ms into the tempo window of up to 16 (four beats). A quarter more than
 * 1/16 off the mean is held back: a second one off the same way is a new tempo (the window restarts from it, the
 * mixed quarter between them dropped); one off the other way was a late pulse and its early neighbour: both dropped.
 * 0: no new value (the tempo stays). */
static int midi_tempo_quarter(uint32_t dt)
{
    uint32_t mean, odd;
    if (midi_clock.q_n) {
        mean = midi_clock.q_sum / midi_clock.q_n;
        odd = dt * 16u > mean * 17u ? 1u : dt * 16u < mean * 15u ? 2u : 0u;
        if (odd) {
            if (midi_clock.q_odd != odd) {
                midi_clock.q_odd = midi_clock.q_odd ? 0u : (uint8_t)odd;
                return 0;
            }
            midi_clock.q_n = 0;                     /* the second one the same way: the tempo moved */
            midi_clock.q_sum = 0;
        }
    }
    midi_clock.q_odd = 0;
    if (midi_clock.q_n == 16u)
        midi_clock.q_sum -= midi_clock.q_ms[midi_clock.q_head];
    else
        midi_clock.q_n++;
    midi_clock.q_ms[midi_clock.q_head] = (uint16_t)dt;
    midi_clock.q_sum += dt;
    midi_clock.q_head = (uint8_t)((midi_clock.q_head + 1u) & 15u);
    return 1;
}
/* The window's quarter, 2 * num2 / den2 ms: the least-squares slope through its n + 1 quarter marks (each mark's time,
 * not only the first and last as a mean of the quarters would), so the USB clocks' 1 ms jitter moves the tempo about
 * 0.1 BPM at 120 and 0.3 at 240 (one quarter alone: 119..121 at 120, 227..234 at 230). Weights 2k - n, k = 0..n;
 * num2 <= 16 * 400 * 51 fits, den2 = n (n + 1) (n + 2) / 3. Once per quarter beat, never per sample. */
static uint32_t midi_tempo_fit(uint32_t *den2)
{
    uint32_t k, n = midi_clock.q_n, i = (midi_clock.q_head - n) & 15u, t = 0;
    int32_t num2 = 0;
    for (k = 1; k <= n; k++) {
        t += midi_clock.q_ms[(i + k - 1u) & 15u];
        num2 += (int32_t)(2u * k - n) * (int32_t)t;
    }
    *den2 = n * (n + 1u) * (n + 2u) / 3u;
    return (uint32_t)num2;
}

static __attribute__((noinline)) void midi_clock_transport(uint32_t status, uint32_t ms)
{
    if (status == 0xFAu) {                         /* Start: step zero */
        midi_clock.pos = midi_clock.rendered = midi_clock.rem = 0;
        midi_clock.have_pulse = midi_clock.tempo_valid = 0;
        midi_clock.interval_ms = 0;
        midi_clock.start_ms = ms;
        seq_start();
    } else if (status == 0xFBu) {                  /* Continue: preserve the step */
        midi_clock.pos = midi_clock.rendered;
        midi_clock.rem = 0;
        midi_clock.have_pulse = midi_clock.tempo_valid = 0;
        midi_clock.interval_ms = 0;
        midi_clock.start_ms = ms;
        if (!song.playing)
            motion_begin();
        song.playing = 1;
    } else if (status == 0xFCu) {
        seq_stop();
    }
}

static __attribute__((noinline)) void midi_clock_pulse(uint32_t ms)
{
    uint32_t q;
    if (!midi_clock.tempo_valid) {
        midi_clock.tempo_valid = 1;
        midi_clock.tempo_ms = ms;
        midi_clock.tempo_n = 0;
        midi_clock.q_n = midi_clock.q_head = midi_clock.q_odd = 0;   /* (Start, Continue, a lost cable: a new window) */
        midi_clock.q_sum = 0;
    } else if (++midi_clock.tempo_n == 6u) {
        uint32_t dt = ms - midi_clock.tempo_ms;
        midi_clock.tempo_ms = ms;
        midi_clock.tempo_n = 0;
        /* Six clocks are a quarter of a beat. Reject gaps and corrupt bursts (55..400 ms: 240 and 40 BPM with their
         * jitter; it was 62..375, which dropped the short quarters at 240 and the long ones at 40, and moved the mean) */
        if (dt > 400u) {                           /* a gap: the window is stale, the next quarter starts a new one */
            midi_clock.q_n = midi_clock.q_head = midi_clock.q_odd = 0;
            midi_clock.q_sum = 0;
        } else if (dt >= 55u && midi_tempo_quarter(dt)) {
            uint32_t den2, num2 = midi_tempo_fit(&den2);
            uint32_t old_beat = beat_samples(), new_beat = (uint32_t)((uint64_t)FS * 2u * num2 / (250u * den2));
            int32_t bpm10 = (int32_t)((75000u * den2 + num2 / 2u) / num2), shown = song.g[G_BPM];
            if (song.playing && new_beat != old_beat) {
                uint32_t i, ratio = (new_beat << 12) / old_beat;
                /* Preserve each track's fractional step phase when the master
                 * changes tempo. The next pulse then lands on its new boundary. */
                for (i = 0; i < NTRK; i++) {
                    if (trk[i].seq_pos < (uint32_t)FS * 2u)
                        trk[i].seq_pos = (uint32_t)(((uint64_t)trk[i].seq_pos * ratio + 2048u) >> 12);
                    if (trk[i].seq_off)
                        trk[i].seq_off = (uint32_t)(((uint64_t)trk[i].seq_off * ratio + 2048u) >> 12);
                    if (trk[i].nd_late)            /* (NUDGE: the latched plays, seq_pos + 1, with the step) */
                        trk[i].nd_late = (uint32_t)(((uint64_t)(trk[i].nd_late - 1u) * ratio + 2048u) >> 12) + 1u;
                    if (trk[i].nd_early)
                        trk[i].nd_early = (uint32_t)(((uint64_t)(trk[i].nd_early - 1u) * ratio + 2048u) >> 12) + 1u;
                }
                if (clk_pos < (uint32_t)FS * 2u)   /* (the metronome's beat as the steps) */
                    clk_pos = (uint32_t)(((uint64_t)clk_pos * ratio + 2048u) >> 12);
            }
            midi_beat_samples = new_beat;
            /* the shown tempo (ARP, SEQ) moves only 0.6 BPM or more away from it: no flicker between neighbours */
            if (bpm10 - shown * 10 >= 6 || shown * 10 - bpm10 >= 6)
                song.g[G_BPM] = (int16_t)clamp((bpm10 + 5) / 10, 40, 240);
        }
    }
    if (song.playing) {
        if (midi_clock.have_pulse) {
            uint32_t interval = ms - midi_clock.last_ms;
            if (interval >= 8u && interval <= 80u)
                midi_clock.interval_ms = interval;
            q = beat_samples() + midi_clock.rem;
            midi_clock.pos += q / 24u;
            midi_clock.rem = q % 24u;
        }
        midi_clock.have_pulse = 1;
    }
    midi_clock.pulse_samples = beat_samples() / 24u;
    if (!midi_clock.interval_ms)
        midi_clock.interval_ms = beat_samples() * 1000u / ((uint32_t)FS * 24u);
    midi_clock.interp_q8 = midi_clock.pulse_samples * 256u / midi_clock.interval_ms;
    midi_clock.last_ms = ms;
}

static uint32_t midi_clock_advance(uint32_t now)
{
    uint32_t target, elapsed, offset, n;
    if (!midi_clock.have_pulse)
        return 0;
    elapsed = now - midi_clock.last_ms;
    /* Interpolate to the next pulse, never across it before it arrives. */
    if (elapsed > midi_clock.interval_ms)
        elapsed = midi_clock.interval_ms;
    offset = elapsed * midi_clock.interp_q8 >> 8;
    if (offset >= midi_clock.pulse_samples)
        offset = midi_clock.pulse_samples - 1u;
    target = midi_clock.pos + offset;
    n = (int32_t)(target - midi_clock.rendered) > 0 ? target - midi_clock.rendered : 0u;
    if (n > (uint32_t)FS / 8u)
        n = (uint32_t)FS / 8u;
    midi_clock.rendered += n;
    return n;
}

