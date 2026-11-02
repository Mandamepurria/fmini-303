# SPDX-License-Identifier: GPL-3.0-only
# FMini-1: the factory bank (200: 000-099 and 100-199, the same ten groups each). 00-09 a showcase, 10-19 hard leads, 20-29 soft leads, 30-39 basses, 40-49 pads and strings,
# 50-59 synth brass (OCT-1), 60-69 plucks, 70-79 keys, 80-89 atmospheres and drones, 90-99 effects.
# tools/gen_fmini_presets.py turns it into firmware/src/eng_fmini_presets.h; tools/preset_lab.py plays and measures it.
#
# Reading a patch: o(range, wave, level, cents) are the three oscillators; cutoff the filter's -3 dB corner in Hz when middle C is
# played (keyboard tracking included), emph the Minimoog's EMPHASIS dial 0 .. 10 (sings from about 8), cntr the filter
# contour's amount (co(octaves)), fenv (attack ms, decay ms, sustain 0..127) the filter contour, env (attack, decay, sustain,
# release) the loudness contour; send = (drive, chorus, delay, reverb) levels; rv / dl / ch / dr the FX settings.
import copy
import os
from gen_fmini_presets import A, o, osc_off, co, st_pit, hz, ms, cut, dly, semi, det, emp, Bank, R, W, RT


def showcase(b):
    b.add("MINI LEAD", "lead", (o("8", "SAW", 90), o("8", "SAW", 90, 11), o("16", "SQR", 70)),
          cutoff=1010, emph=3.5, cntr=co(2.0), kbd=64, fenv=(0, 350, 70), env=(3, 400, 110, 180),
          vib=30, mosc=False, lfo=(5.5, "SIN", 400, "OFF"), glide=40, oct=1,      # (GLIDE 3.9 (-10 %) on the knob)
          send=(30, 0, 55, 40), rv=("HALL", 60, 80), dl=("ANALOG", 375, 45), dr=("TUBE", 70, 64), pat=4)
    b.add("I FEEL ARP", "bass", (o("16", "SAW", 120), o("16", "SQR", 110, 11), o("32", "TRI", 90)),
          cutoff=309, emph=3.6, cntr=co(2.5), kbd=40, fenv=(0, 120, 20), env=(2, 180, 40, 60),
          send=(15, 0, 20, 10), rv=("PLATE", 40, 90), dl=("DIGITAL", 188, 35),
          arp=A("UP", "1/16", 2, 0.45), arp_on=True, pat=2)
    b.add("LUSH STRINGS", "pad", (o("8", "SAW", 80), o("8", "SAW", 80, 22), o("4", "SAW", 60, -22)),
          cutoff=1150, emph=2.3, cntr=0, kbd=64, fenv=(0, 400, 100), env=(350, 600, 118, 700),
          vib=14, mosc=False, lfo=(5.0, "SIN", 800, "OFF"), poly=True,
          send=(0, 90, 0, 50), rv=("HALL", 80, 85), ch=("MULTI", 70, 0.5), pat=5)
    b.add("ROTARY ORG", "keys", (o("16", "TRI", 90), o("8", "TRI", 100), o("4", "SQR", 50)),
          cutoff=2410, emph=0.0, cntr=0, kbd=100, fenv=(0, 100, 127), env=(6, 100, 127, 60),
          lfo=(6.5, "SIN", 0, "OFF"), trem=20, pit=st_pit(0.1), poly=True,
          send=(0, 80, 0, 64), rv=("HALL", 40, 70), ch=("STEREO", 90, 6.5), pat=6)
    b.add("CRYSTAL BELL", "keys", (o("4", "TRI", 100), o("4", "TRI", 70, 693), o("2", "TRI", 40, 516)),
          cutoff=700, emph=2.3, cntr=co(3.0), kbd=127, fenv=(0, 1500, 0), env=(1, 2500, 0, 1500), poly=True,
          send=(0, 20, 30, 70), rv=("PLATE", 80, 100), dl=("DIGITAL", 375, 40), ch=("STEREO", 60, 0.5), pat=7)
    b.add("DUB STAB", "pluck", (o("8", "SAW", 115), o("8", "SAW", 115, 11), o("8", "SQR", 90, -11)),
          cutoff=406, emph=3.3, cntr=co(3.2), kbd=64, fenv=(0, 180, 0), env=(2, 380, 0, 200), poly=True,
          send=(20, 0, 90, 45), rv=("SPRING", 55, 80), dl=("TAPE", 375, 70), dr=("TUBE", 64, 64), pat=6)
    b.add("CS-80 BRASS", "brass", (o("8", "SAW", 90), o("8", "SAW", 90, 14), o("16", "SAW", 70, -11)),
          cutoff=293, emph=3.1, cntr=co(3.0), kbd=64, fenv=(250, 900, 70), env=(90, 600, 118, 500),
          vib=20, mosc=True, lfo=(5.2, "SIN", 700, "OFF"), poly=True, oct=-1,
          send=(0, 60, 0, 45), rv=("HALL", 70, 80), ch=("STEREO", 60, 0.8), pat=5)
    b.add("SPACE DRONE", "atmo", (o("16", "SAW", 90), o("16", "SAW", 90, 17), o("8", "PW", 60)), noise=25, pink=True,
          cutoff=272, emph=3.8, cntr=co(2.5), kbd=40, fenv=(3000, 4000, 60), env=(2500, 3000, 118, 3200),
          vib=70, mflt=True, lfo=(0.08, "SIN", 0, "OFF"), poly=True,
          send=(0, 80, 60, 90), rv=("CLOUD", 90, 70), dl=("REVERSE", 560, 50), ch=("MULTI", 70, 0.3), pat=5)
    b.add("TANGERINE", "pluck", (o("8", "SAW", 100), o("8", "SQR", 90, 7), o("4", "SAW", 60, -7)),
          cutoff=555, emph=4.3, cntr=co(2.6), kbd=70, fenv=(0, 240, 10), env=(1, 330, 25, 160), poly=True,
          send=(20, 40, 85, 55), rv=("PLATE", 60, 90), dl=("DIGITAL", 375, 62), ch=("STEREO", 60, 0.4),
          arp=A("UPDN", "1/16", 3, 0.5, 0, 127), arp_on=True, pat=13)
    b.add("E.PIANO", "keys", (o("8", "TRI", 100), o("4", "TRI", 60), o("2", "SQR", 30)),
          cutoff=973, emph=2.5, cntr=co(2.0), kbd=90, fenv=(0, 250, 10), env=(2, 1800, 10, 250), poly=True,
          send=(0, 70, 0, 25), rv=("PLATE", 50, 90), ch=("STEREO", 60, 0.7), pat=6)


def hard_leads(b):                       # 10-19: intense, aggressive, big
    b.add("EMERSON", "lead", (o("8", "SAW", 100), o("8", "SAW", 100, -17), o("16", "SQR", 75, 6)),
          cutoff=1110, emph=4.6, cntr=co(1.8), kbd=90, fenv=(0, 600, 55), env=(2, 300, 118, 260),
          vib=28, mosc=False, lfo=(5.3, "SIN", 600, "OFF"), glide=150,
          send=(60, 0, 55, 35), rv=("PLATE", 45, 80), dl=("TAPE", 300, 38), dr=("TUBE", 80, 70), pat=4)
    b.add("NUMAN", "lead", (o("8", "SAW", 100), o("8", "SAW", 100, 9), o("4", "SAW", 60, -9)),
          cutoff=1470, emph=2.8, cntr=co(1.2), kbd=100, fenv=(0, 700, 85), env=(4, 220, 122, 320),
          vib=14, mosc=True, lfo=(5.6, "SIN", 500, "OFF"),
          send=(20, 115, 35, 45), rv=("HALL", 65, 85), dl=("DIGITAL", 250, 30), ch=("JUNO", 115, 0.45), pat=4)
    b.add("HAMMER", "lead", (o("8", "SHK", 100), o("8", "PW", 80, 12), o("16", "SQR", 60)),
          cutoff=784, emph=4.3, cntr=co(2.4), kbd=80, fenv=(40, 450, 25), env=(1, 500, 100, 120),
          vib=18, mosc=True, lfo=(5.8, "SIN", 700, "OFF"), glide=45,
          send=(70, 0, 70, 30), rv=("PLATE", 40, 80), dl=("ANALOG", 428, 52), dr=("TUBE", 90, 64), pat=4)
    b.add("FIFTHS", "lead", (o("8", "SAW", 100), o("4", "SAW", 85, -500), o("8", "SHK", 60, 6)),
          cutoff=1340, emph=3.8, cntr=co(1.6), kbd=90, fenv=(0, 450, 65), env=(3, 300, 120, 300),
          vib=20, mosc=True, lfo=(5.0, "SIN", 500, "OFF"), glide=70,
          send=(75, 70, 35, 50), rv=("PLATE", 75, 85), dl=("SLAP", 130, 28), ch=("JUNO", 70, 0.5), dr=("SOFT", 80, 64), pat=4)
    b.add("RAVE ARP", "lead", (o("8", "SAW", 100, -19), o("8", "SAW", 100, 19), o("4", "SAW", 75)),
          cutoff=1430, emph=3.0, cntr=co(2.0), kbd=90, fenv=(0, 200, 50), env=(1, 260, 70, 110), poly=True,
          send=(25, 70, 75, 45), rv=("PLATE", 55, 90), dl=("DIGITAL", 375, 55), ch=("JUNO", 70, 0.35),
          arp=A("UP", "1/16", 2, 0.6, 0, 127), arp_on=True, pat=13)
    b.add("ACID LEAD", "lead", (o("8", "SAW", 115), o("8", "SQR", 60, 7), osc_off()),
          cutoff=227, emph=7.8, cntr=co(4.2), kbd=70, fenv=(0, 160, 5), env=(1, 200, 110, 70), glide=70,
          send=(70, 0, 55, 25), rv=("ROOM", 30, 80), dl=("ANALOG", 375, 55), dr=("TUBE", 85, 70),
          arp=A("UPDN", "1/16", 2, 0.5, 0, 127), arp_on=True, pat=1)
    b.add("PULSE HERO", "lead", (o("8", "PW", 100), o("8", "PN", 95, 13), o("16", "SQR", 70)),
          cutoff=1770, emph=3.1, cntr=co(1.2), kbd=100, fenv=(0, 350, 55), env=(2, 250, 118, 170),
          vib=24, mosc=True, lfo=(5.0, "SIN", 400, "OFF"),
          send=(35, 70, 50, 35), rv=("ROOM", 55, 85), dl=("SLAP", 120, 30), ch=("STEREO", 80, 0.7), pat=4)
    b.add("TITAN", "lead", (o("16", "SQR", 105), o("8", "SAW", 100), o("8", "SAW", 100, -23)),
          cutoff=428, emph=3.7, cntr=co(3.2), kbd=55, fenv=(70, 800, 45), env=(3, 260, 122, 400),
          vib=16, mosc=True, lfo=(4.8, "SIN", 700, "OFF"), glide=80,
          send=(85, 0, 35, 55), rv=("HALL", 100, 50), dl=("TAPE", 375, 40), dr=("TUBE", 50, 70), pat=4)
    b.add("FUZZ LEAD", "lead", (o("8", "SQR", 120), o("8", "SAW", 118, 6), o("4", "SQR", 70, -6)),
          cutoff=2000, emph=2.8, cntr=co(0.9), kbd=100, fenv=(0, 250, 70), env=(1, 200, 120, 140),
          vib=20, mosc=True, lfo=(5.5, "SIN", 500, "OFF"), glide=30,
          send=(127, 0, 50, 25), rv=("ROOM", 40, 80), dl=("DIGITAL", 188, 35), dr=("SOFT", 110, 50), pat=4)
    b.add("GROWL", "lead", (o("8", "SHK", 100), o("8", "SAW", 100, -19), o("8", "PN", 70)),
          cutoff=392, emph=5.0, cntr=co(1.2), kbd=60, fenv=(0, 300, 50), env=(3, 250, 120, 250),
          vib=85, mflt=True, lfo=(2.0, "TRI", 0, "1/8"),
          send=(60, 40, 35, 40), rv=("PLATE", 45, 70), dl=("DIGITAL", 250, 30), dr=("TUBE", 70, 64), pat=4)


def soft_leads(b):                       # 20-29: gentler, singing, mellow
    b.add("MOOG FLUTE", "lead", (o("8", "TRI", 105), o("4", "SQR", 35, 3), osc_off()), noise=14, pink=True,
          cutoff=932, emph=2.5, cntr=co(0.8), kbd=100, fenv=(0, 200, 55), env=(55, 400, 118, 260),
          vib=26, mosc=True, lfo=(5.0, "SIN", 700, "OFF"),
          send=(0, 40, 25, 55), rv=("ROOM", 55, 85), ch=("STEREO", 40, 0.4), pat=4)
    b.add("G-FUNK", "lead", (o("8", "TRI", 100), o("4", "SAW", 50), osc_off()),
          cutoff=994, emph=3.3, cntr=0, kbd=100, fenv=(0, 200, 100), env=(30, 500, 120, 350), glide=240,
          vib=18, mosc=True, lfo=(6.0, "SIN", 800, "OFF"),
          send=(0, 60, 50, 40), rv=("PLATE", 40, 80), dl=("ANALOG", 375, 35), ch=("STEREO", 40, 0.5), pat=4)
    b.add("SINE SOLO", "lead", (o("8", "TRI", 120), o("8", "SAW", 25), osc_off()),
          cutoff=487, emph=0.0, cntr=0, kbd=100, fenv=(0, 200, 100), env=(8, 600, 120, 400), glide=110,
          vib=24, mosc=True, lfo=(5.5, "SIN", 900, "OFF"),
          send=(0, 0, 55, 60), rv=("HALL", 85, 70), dl=("ANALOG", 429, 45), pat=4)
    b.add("BANKS", "lead", (o("8", "SAW", 95), o("8", "SAW", 95, -12), o("16", "TRI", 70)),
          cutoff=719, emph=2.8, cntr=co(1.4), kbd=70, fenv=(120, 1200, 60), env=(40, 500, 118, 450),
          vib=22, mosc=True, lfo=(5.0, "SIN", 800, "OFF"),
          send=(0, 70, 40, 60), rv=("HALL", 90, 70), dl=("ANALOG", 375, 30), ch=("STEREO", 70, 0.35), pat=4)
    b.add("OBOE", "lead", (o("8", "PW", 100), o("8", "SQR", 60, 8), o("4", "PN", 30)),
          cutoff=994, emph=3.6, cntr=co(0.8), kbd=100, fenv=(30, 300, 70), env=(35, 300, 118, 150),
          vib=22, mosc=True, lfo=(5.5, "SIN", 600, "OFF"),
          send=(0, 25, 20, 35), rv=("ROOM", 50, 75), pat=4)
    b.add("SOLO VIOLIN", "lead", (o("8", "SAW", 100), o("8", "SAW", 90, 10), o("4", "TRI", 30)),
          cutoff=1210, emph=2.7, cntr=co(0.8), kbd=100, fenv=(60, 300, 80), env=(110, 300, 120, 220),
          vib=40, mosc=True, lfo=(5.8, "SIN", 900, "OFF"),
          send=(0, 35, 20, 55), rv=("HALL", 85, 75), ch=("STEREO", 40, 0.5), pat=4)
    b.add("DREAMER", "lead", (o("8", "TRI", 100), o("8", "SQR", 55, 5), o("4", "TRI", 60, -6)),
          cutoff=801, emph=2.7, cntr=co(1.5), kbd=90, fenv=(0, 500, 25), env=(5, 700, 40, 500), poly=True,
          send=(0, 60, 80, 80), rv=("CLOUD", 100, 70), dl=("TAPE", 500, 50), ch=("STEREO", 50, 0.3),
          arp=A("UPDN", "8T", 2, 0.8, 0, 127), arp_on=True, pat=13)
    b.add("WHISTLE", "lead", (o("4", "TRI", 110), o("8", "TRI", 40), osc_off()), noise=8, pink=True,
          cutoff=1290, emph=0.3, cntr=0, kbd=100, fenv=(0, 200, 100), env=(80, 300, 120, 300), glide=90,
          vib=34, mosc=True, lfo=(5.8, "SIN", 1200, "OFF"),
          send=(0, 30, 30, 60), rv=("PLATE", 45, 85), pat=4)
    b.add("MELLOW PULSE", "lead", (o("8", "SQR", 100), o("8", "TRI", 80, -14), o("16", "SQR", 40)),
          cutoff=466, emph=2.1, cntr=co(1.2), kbd=80, fenv=(0, 500, 55), env=(10, 400, 118, 300),
          vib=18, mosc=True, lfo=(5.2, "SIN", 500, "OFF"),
          send=(0, 100, 45, 55), rv=("HALL", 80, 70), dl=("ANALOG", 375, 35), ch=("JUNO", 100, 0.4),
          arp=A("UPDN", "1/8", 2, 0.7, 10, 127), arp_on=True, pat=13)
    b.add("SAX LEAD", "lead", (o("8", "SHK", 100), o("8", "PW", 60, 3), o("16", "SQR", 30)), noise=10, pink=True,
          cutoff=801, emph=3.1, cntr=co(1.4), kbd=90, fenv=(25, 250, 55), env=(25, 300, 118, 160), glide=40,
          vib=30, mosc=True, lfo=(5.4, "SIN", 800, "OFF"),
          send=(30, 25, 30, 40), rv=("ROOM", 45, 75), dr=("TUBE", 60, 64), pat=4)


def basses(b):                           # 30-39
    b.add("MOOG BASS", "bass", (o("16", "SAW", 110), o("16", "SAW", 100, -9), o("8", "TRI", 60)),
          cutoff=175, emph=3.0, cntr=co(2.2), kbd=40, fenv=(0, 600, 35), env=(3, 400, 118, 150),
          send=(30, 0, 0, 8), rv=("ROOM", 20, 60), dr=("TUBE", 60, 64), oct=-1, pat=2)
    b.add("CHAMELEON", "bass", (o("16", "SQR", 110), o("16", "SQR", 95, 5), o("8", "SAW", 55, -5)),
          cutoff=210, emph=5.5, cntr=co(3.9), kbd=55, fenv=(0, 95, 0), env=(1, 170, 45, 50),
          send=(35, 0, 25, 10), rv=("ROOM", 20, 60), dl=("SLAP", 100, 25), dr=("TUBE", 70, 64), oct=-1, pat=2)
    b.add("SUB BASS", "bass", (o("32", "TRI", 110), o("16", "TRI", 100), osc_off()),
          cutoff=130, emph=0.6, cntr=co(0.5), kbd=0, fenv=(0, 200, 60), env=(8, 300, 118, 120),
          send=(40, 0, 0, 0), dr=("SOFT", 40, 64), oct=-1, pat=8)
    b.add("WOBBLE BASS", "bass", (o("16", "SAW", 105), o("16", "SAW", 105, -23), o("32", "TRI", 70)),
          cutoff=238, emph=5.1, cntr=co(0.5), kbd=40, fenv=(0, 200, 60), env=(4, 300, 118, 150),
          vib=110, mflt=True, lfo=(2.0, "TRI", 0, "1/8"),
          send=(80, 0, 0, 15), dr=("TUBE", 60, 64), oct=-1, pat=2)
    b.add("OCTAVE PUMP", "bass", (o("16", "SQR", 110), o("8", "SAW", 75, 7), osc_off()),
          cutoff=531, emph=2.5, cntr=co(3.0), kbd=50, fenv=(0, 90, 0), env=(1, 160, 40, 50),
          send=(30, 0, 25, 12), dl=("DIGITAL", 188, 30), arp=A("UP+8", "1/8", 1, 0.55, 0, 127), arp_on=True, oct=-1, pat=2)
    b.add("SEQ BASS", "bass", (o("16", "SAW", 110), o("16", "SAW", 100, 13), o("32", "SQR", 70)),
          cutoff=392, emph=4.8, cntr=co(2.8), kbd=60, fenv=(0, 110, 0), env=(1, 140, 0, 40),
          send=(50, 0, 40, 15), dl=("DIGITAL", 188, 35), dr=("TUBE", 70, 64),
          arp=A("ORD", "1/16", 1, 0.35, 25, 127), arp_on=True, oct=-1, pat=13)
    b.add("REESE", "bass", (o("16", "SAW", 105, -25), o("16", "SAW", 105, 25), o("8", "SAW", 60)),
          cutoff=254, emph=2.5, cntr=co(0.8), kbd=40, fenv=(0, 500, 60), env=(8, 400, 120, 300),
          vib=30, mflt=True, lfo=(0.25, "TRI", 0, "OFF"),
          send=(60, 100, 0, 35), ch=("MULTI", 100, 0.25), dr=("TUBE", 50, 64), oct=-1, pat=8)
    b.add("RUBBER", "bass", (o("16", "TRI", 127), o("16", "SQR", 115), o("8", "TRI", 60)),
          cutoff=316, emph=3.7, cntr=co(2.2), kbd=50, fenv=(0, 120, 0), env=(1, 500, 0, 100), penv=st_pit(2.5),
          send=(25, 0, 18, 0), dr=("TUBE", 50, 64), oct=-1, pat=2)
    b.add("FUZZ BASS", "bass", (o("16", "SQR", 120), o("8", "SQR", 100, 7), o("16", "SQR", 70, -7)),
          cutoff=767, emph=2.7, cntr=co(1.0), kbd=70, fenv=(0, 200, 60), env=(2, 250, 115, 100),
          send=(127, 0, 0, 10), dr=("SOFT", 55, 55), arp=A("REPEAT", "1/8", 1, 0.6, 0, 127), arp_on=True, oct=-1, pat=2)
    b.add("WONDER BASS", "bass", (o("16", "SAW", 100), o("8", "SQR", 95), o("16", "TRI", 70)),
          cutoff=283, emph=3.6, cntr=co(2.8), kbd=60, fenv=(0, 260, 30), env=(3, 400, 100, 120), glide=35,
          send=(20, 0, 20, 20), dl=("ANALOG", 250, 30), oct=-1, pat=2)


def pads(b):                             # 40-49: pads and strings
    b.add("SOLINA", "pad", (o("8", "SAW", 90), o("8", "SAW", 90, 17), o("4", "PW", 55, -9)),
          cutoff=2130, emph=0.5, cntr=0, kbd=100, fenv=(0, 100, 127), env=(90, 400, 120, 400), poly=True,
          vib=8, mosc=True, lfo=(5.5, "SIN", 0, "OFF"),
          send=(0, 127, 0, 35), rv=("ROOM", 50, 100), ch=("JUNO", 120, 0.5), pat=5)
    b.add("WARM PAD", "pad", (o("8", "SAW", 85), o("8", "SQR", 75, -9), o("16", "SAW", 60, 5)),
          cutoff=520, emph=2.4, cntr=co(1.0), kbd=70, fenv=(900, 2000, 60), env=(700, 1200, 115, 1500), poly=True,
          send=(0, 70, 0, 80), rv=("HALL", 110, 60), ch=("STEREO", 60, 0.25), pat=5)
    b.add("JUNO PWM", "pad", (o("8", "PW", 90), o("8", "PN", 75, 13), o("8", "SQR", 60, -13)),
          cutoff=1320, emph=2.1, cntr=co(0.5), kbd=80, fenv=(0, 600, 90), env=(120, 400, 118, 500), poly=True,
          vib=6, mosc=True, lfo=(5.0, "SIN", 0, "OFF"),
          send=(0, 127, 0, 35), rv=("PLATE", 50, 80), ch=("JUNO", 127, 0.3),
          arp=A("UP", "1/8", 2, 0.85, 0, 127), arp_on=True, pat=13)
    b.add("CHOIR", "pad", (o("8", "TRI", 95), o("8", "SQR", 60, 6), o("4", "TRI", 60, -6)), noise=6, pink=True,
          cutoff=854, emph=3.5, cntr=co(0.6), kbd=60, fenv=(200, 800, 80), env=(500, 600, 118, 900), poly=True,
          vib=30, mosc=True, lfo=(5.0, "SIN", 1000, "OFF"),
          send=(0, 50, 0, 85), rv=("HALL", 100, 55), ch=("STEREO", 50, 0.3), pat=5)
    b.add("SWEEP PAD", "pad", (o("8", "SAW", 90), o("8", "SAW", 90, -14), o("16", "SAW", 65, 7)),
          cutoff=184, emph=3.8, cntr=co(3.5), kbd=40, fenv=(2500, 3500, 40), env=(1200, 1500, 115, 1800), poly=True,
          send=(0, 60, 50, 70), rv=("HALL", 90, 70), dl=("TAPE", 500, 50), ch=("STEREO", 50, 0.2), pat=5)
    b.add("ANGELS", "pad", (o("8", "TRI", 90), o("4", "TRI", 70), o("2", "TRI", 50, 7)),
          cutoff=4200, emph=0.0, cntr=0, kbd=100, fenv=(0, 100, 127), env=(900, 800, 110, 2500), poly=True,
          vib=14, mosc=True, lfo=(4.0, "SIN", 1500, "OFF"),
          send=(0, 60, 35, 110), rv=("CLOUD", 120, 90), dl=("TAPE", 750, 45), ch=("STEREO", 70, 0.2), pat=5)
    b.add("ECLIPSE", "pad", (o("32", "SAW", 100), o("16", "SAW", 95, -15), o("16", "SQR", 60, 15)),
          cutoff=175, emph=3.0, cntr=co(1.2), kbd=40, fenv=(1500, 3000, 50), env=(1100, 1500, 118, 2200), poly=True,
          vib=40, mflt=True, lfo=(0.12, "SIN", 0, "OFF"),
          send=(40, 50, 0, 90), rv=("HALL", 127, 35), dr=("TUBE", 30, 60), pat=5)
    b.add("PULSE PAD", "pad", (o("8", "SAW", 85), o("8", "PW", 70, 10), o("4", "SAW", 45, -10)),
          cutoff=767, emph=3.0, cntr=co(1.5), kbd=70, fenv=(0, 400, 35), env=(30, 600, 60, 900), poly=True,
          send=(0, 80, 60, 80), rv=("CLOUD", 100, 70), dl=("TAPE", 375, 45), ch=("STEREO", 60, 0.3),
          arp=A("DIVG", "1/8", 2, 0.9, 0, 127), arp_on=True, pat=13)
    b.add("CELLOS", "pad", (o("16", "SAW", 100), o("16", "SAW", 75, 12), o("8", "TRI", 50)),
          cutoff=337, emph=2.1, cntr=0, kbd=80, fenv=(0, 100, 127), env=(220, 400, 118, 500), poly=True,
          vib=42, mosc=True, lfo=(5.0, "SIN", 1200, "OFF"),
          send=(0, 40, 0, 60), rv=("HALL", 80, 60), ch=("STEREO", 40, 0.4), pat=5)
    b.add("AIR PAD", "pad", (o("8", "TRI", 50), osc_off(), osc_off()), noise=70, pink=True,
          cutoff=1040, emph=5.1, cntr=co(1.5), kbd=60, fenv=(1500, 2500, 50), env=(900, 1000, 118, 1800), poly=True,
          vib=60, mflt=True, lfo=(0.18, "TRI", 0, "OFF"),
          send=(0, 70, 0, 90), rv=("CLOUD", 110, 60), ch=("MULTI", 80, 0.2), pat=5)


def brass(b):                            # 50-59: all come in with OCT-1
    b.add("MOOG BRASS", "brass", (o("8", "SAW", 95), o("8", "SAW", 95, 10), o("16", "SQR", 60, -6)),
          cutoff=401, emph=3.0, cntr=co(2.6), kbd=70, fenv=(70, 300, 50), env=(35, 300, 118, 200), poly=True,
          send=(0, 40, 0, 40), rv=("HALL", 60, 80), ch=("STEREO", 40, 0.4), pat=5)
    b.add("JUMP BRASS", "brass", (o("8", "SAW", 100, -14), o("8", "SAW", 100, 14), o("4", "SAW", 50)),
          cutoff=428, emph=2.4, cntr=co(3.0), kbd=70, fenv=(25, 550, 40), env=(18, 500, 95, 300), poly=True,
          send=(15, 70, 0, 50), rv=("PLATE", 60, 85), ch=("JUNO", 70, 0.4), pat=5)
    b.add("TRUMPET", "brass", (o("8", "SAW", 100), o("8", "SQR", 70, 6), o("4", "SAW", 40)), noise=5, pink=True,
          cutoff=476, emph=2.8, cntr=co(2.6), kbd=90, fenv=(45, 200, 50), env=(30, 300, 118, 140), glide=25,
          vib=30, mosc=True, lfo=(5.5, "SIN", 900, "OFF"),
          send=(20, 0, 25, 35), rv=("ROOM", 50, 80), dl=("ANALOG", 250, 30), pat=4)
    b.add("FRENCH HORN", "brass", (o("8", "TRI", 100), o("4", "SAW", 40), o("16", "SQR", 45)),
          cutoff=323, emph=2.4, cntr=co(1.6), kbd=60, fenv=(120, 500, 60), env=(100, 400, 118, 400), poly=True,
          vib=14, mosc=True, lfo=(5.0, "SIN", 1000, "OFF"),
          send=(0, 35, 0, 70), rv=("HALL", 110, 45), pat=5)
    b.add("BRASS STAB", "brass", (o("8", "SAW", 105), o("8", "SAW", 105, 12), o("8", "SQR", 70, -12)),
          cutoff=520, emph=2.8, cntr=co(3.0), kbd=70, fenv=(0, 220, 0), env=(2, 260, 0, 140), poly=True,
          send=(40, 0, 55, 40), rv=("ROOM", 40, 85), dl=("ANALOG", 250, 35), pat=6)
    b.add("BRASS SWELL", "brass", (o("8", "SAW", 100), o("8", "SQR", 80, -9), o("4", "SAW", 45, 6)), noise=6, pink=True,
          cutoff=376, emph=3.5, cntr=co(3.6), kbd=70, fenv=(650, 700, 75), env=(550, 600, 118, 700), poly=True,
          vib=28, mosc=True, lfo=(5.6, "SIN", 1200, "OFF"),
          send=(40, 30, 35, 60), rv=("PLATE", 80, 80), dl=("ANALOG", 375, 30), dr=("SOFT", 70, 64), pat=5)
    b.add("HORN RIFF", "brass", (o("8", "SAW", 100), o("8", "SAW", 100, 8), o("16", "SQR", 60)),
          cutoff=457, emph=2.6, cntr=co(2.8), kbd=70, fenv=(10, 250, 25), env=(8, 300, 60, 120), poly=True,
          send=(25, 40, 40, 40), rv=("PLATE", 45, 85), dl=("ANALOG", 250, 30),
          arp=A("CHORD", "1/8", 1, 0.45, 18, 127), arp_on=True, pat=13)
    b.add("TROMBONE", "brass", (o("8", "SAW", 100), o("16", "SAW", 70, 5), o("8", "SQR", 50, -5)),
          cutoff=302, emph=2.5, cntr=co(2.0), kbd=60, fenv=(60, 400, 55), env=(50, 300, 118, 220), glide=90,
          vib=22, mosc=True, lfo=(5.0, "SIN", 1000, "OFF"),
          send=(10, 0, 15, 45), rv=("HALL", 65, 70), pat=4)
    b.add("FLUGEL", "brass", (o("8", "TRI", 100), o("4", "SAW", 30), osc_off()),
          cutoff=428, emph=2.1, cntr=co(1.2), kbd=70, fenv=(40, 400, 70), env=(40, 300, 118, 300), poly=True,
          vib=18, mosc=True, lfo=(5.4, "SIN", 800, "OFF"),
          send=(0, 60, 0, 60), rv=("HALL", 70, 75), ch=("STEREO", 60, 0.3), pat=5)
    b.add("FANFARE", "brass", (o("8", "SAW", 100), o("4", "SAW", 90), o("16", "SQR", 75)),
          cutoff=1020, emph=2.7, cntr=co(1.6), kbd=80, fenv=(0, 700, 70), env=(12, 400, 120, 450), poly=True,
          send=(55, 40, 30, 75), rv=("HALL", 110, 80), dl=("ANALOG", 375, 30), ch=("STEREO", 50, 0.35), dr=("TUBE", 80, 64), pat=5)


def plucks(b):                           # 60-69
    b.add("PLUCK SAW", "pluck", (o("8", "SAW", 100), o("8", "SAW", 100, 12), o("4", "SAW", 50, -12)),
          cutoff=352, emph=3.3, cntr=co(3.0), kbd=70, fenv=(0, 200, 0), env=(1, 400, 0, 200), poly=True,
          send=(0, 50, 50, 40), rv=("PLATE", 45, 85), dl=("DIGITAL", 250, 40), pat=3)
    b.add("MARIMBA", "pluck", (o("8", "TRI", 110), o("2", "TRI", 35), osc_off()),
          cutoff=1060, emph=0.2, cntr=co(1.5), kbd=100, fenv=(0, 80, 0), env=(1, 500, 0, 250), poly=True,
          send=(0, 0, 25, 45), rv=("ROOM", 55, 80),
          arp=A("UPDN", "1/16", 2, 0.45, 0, 127), arp_on=True, pat=13)
    b.add("KALIMBA", "pluck", (o("4", "TRI", 100), o("2", "TRI", 25, 516), osc_off()),
          cutoff=1470, emph=0.8, cntr=co(1.0), kbd=100, fenv=(0, 200, 0), env=(1, 500, 0, 400), poly=True,
          send=(0, 30, 70, 60), rv=("PLATE", 70, 95), dl=("TAPE", 375, 55),
          arp=A("UPDN", "1/8", 2, 0.6, 0, 127), arp_on=True, pat=13)
    b.add("FUNK ARP", "pluck", (o("8", "SAW", 100), o("8", "PW", 70, 10), osc_off()),
          cutoff=316, emph=4.7, cntr=co(3.5), kbd=80, fenv=(30, 200, 0), env=(1, 260, 0, 100), poly=True,
          send=(30, 0, 45, 25), rv=("ROOM", 35, 85), dl=("DIGITAL", 188, 30), dr=("TUBE", 70, 64),
          arp=A("PINKY", "1/16", 2, 0.4, 30, 127), arp_on=True, pat=13)
    b.add("NYLON", "pluck", (o("8", "TRI", 100), o("8", "SQR", 40, 4), o("4", "TRI", 30)),
          cutoff=854, emph=2.3, cntr=co(1.6), kbd=90, fenv=(0, 180, 0), env=(1, 700, 0, 350), poly=True,
          send=(0, 20, 20, 50), rv=("ROOM", 50, 80), pat=3)
    b.add("STEELDRUM", "pluck", (o("8", "TRI", 100), o("4", "TRI", 60), o("4", "TRI", 40, 693)),
          cutoff=1370, emph=2.0, cntr=co(1.2), kbd=100, fenv=(0, 250, 0), env=(1, 600, 0, 300), poly=True,
          send=(0, 40, 50, 55), rv=("ROOM", 70, 85), dl=("ANALOG", 375, 40),
          arp=A("WALK", "1/8", 2, 0.55, 0, 115), arp_on=True, pat=13)
    b.add("8-BIT ARP", "pluck", (o("8", "PW", 100), o("4", "PN", 40), osc_off()),
          cutoff=8470, emph=1.2, cntr=0, kbd=0, fenv=(0, 100, 127), env=(1, 60, 90, 25), poly=True,
          send=(0, 0, 30, 12), rv=("ROOM", 15, 100), dl=("DIGITAL", 188, 30),
          arp=A("UP", "1/16", 3, 0.5, 0, 127), arp_on=True, pat=13)
    b.add("PIZZICATO", "pluck", (o("8", "SAW", 90), o("8", "SAW", 90, 10), osc_off()),
          cutoff=376, emph=2.3, cntr=co(2.4), kbd=70, fenv=(0, 120, 0), env=(1, 220, 0, 120), poly=True,
          send=(0, 60, 25, 50), rv=("HALL", 70, 70), ch=("STEREO", 50, 0.4),
          arp=A("UPDN", "1/8", 2, 0.3, 0, 127), arp_on=True, pat=13)
    b.add("POP PLUCK", "pluck", (o("8", "SAW", 100), o("8", "SQR", 60, -8), o("4", "SAW", 40)),
          cutoff=801, emph=3.1, cntr=co(2.0), kbd=90, fenv=(0, 260, 10), env=(1, 420, 0, 220), poly=True,
          send=(0, 50, 90, 45), rv=("PLATE", 50, 85), dl=("ANALOG", 375, 55),
          arp=A("UP", "1/8", 2, 0.5, 0, 127), arp_on=True, pat=13)
    b.add("HARP", "pluck", (o("8", "TRI", 100), o("4", "TRI", 50), osc_off()),
          cutoff=1290, emph=0.3, cntr=co(1.0), kbd=100, fenv=(0, 250, 0), env=(1, 1400, 0, 700), poly=True,
          send=(0, 50, 40, 80), rv=("HALL", 90, 90), dl=("TAPE", 500, 40),
          arp=A("UP", "1/32", 4, 0.9, 0, 127), arp_on=True, pat=13)


def keys(b):                             # 70-79
    b.add("CLAVINET", "keys", (o("8", "PN", 100), o("8", "PW", 80, 3), o("4", "SQR", 35)),
          cutoff=836, emph=2.5, cntr=co(2.8), kbd=110, fenv=(0, 160, 10), env=(1, 600, 0, 60), poly=True,
          send=(25, 0, 0, 15), rv=("ROOM", 30, 85), dr=("TUBE", 60, 64), pat=6)
    b.add("PIPE ORGAN", "keys", (o("16", "TRI", 80), o("8", "SQR", 95), o("4", "SQR", 70)),
          cutoff=2130, emph=0.5, cntr=0, kbd=100, fenv=(0, 100, 127), env=(18, 100, 127, 80), poly=True,
          send=(0, 25, 0, 100), rv=("HALL", 127, 50), pat=5)
    b.add("SYNTH PIANO", "keys", (o("8", "SAW", 90), o("4", "SQR", 50), o("8", "TRI", 60, 2)), noise=10, pink=True,
          cutoff=688, emph=2.0, cntr=co(3.2), kbd=110, fenv=(0, 300, 0), env=(1, 2500, 0, 300), poly=True,
          send=(0, 25, 0, 45), rv=("PLATE", 60, 85), ch=("STEREO", 25, 0.4), pat=6)
    b.add("HARPSICHORD", "keys", (o("8", "SAW", 90), o("4", "SQR", 70), o("8", "PN", 50, 5)),
          cutoff=1560, emph=2.5, cntr=co(1.2), kbd=100, fenv=(0, 150, 0), env=(1, 1000, 0, 80), poly=True,
          send=(0, 0, 0, 40), rv=("ROOM", 45, 90), pat=3)
    b.add("VIBES", "keys", (o("8", "TRI", 110), o("2", "TRI", 25), osc_off()),
          cutoff=1290, emph=0.3, cntr=co(1.0), kbd=100, fenv=(0, 300, 0), env=(1, 3000, 0, 1200), poly=True,
          lfo=(5.0, "SIN", 0, "OFF"), trem=40,
          send=(0, 30, 0, 70), rv=("PLATE", 70, 90), pat=3)
    b.add("WURLY", "keys", (o("8", "SQR", 95), o("8", "TRI", 75, -5), o("4", "SQR", 25)),
          cutoff=1210, emph=2.3, cntr=co(1.2), kbd=100, fenv=(0, 200, 15), env=(2, 1400, 10, 200), poly=True,
          lfo=(5.5, "SIN", 0, "OFF"), trem=22,
          send=(30, 50, 0, 35), rv=("SPRING", 45, 80), ch=("STEREO", 70, 0.5), dr=("TUBE", 70, 64), pat=6)
    b.add("TUBULAR", "keys", (o("8", "TRI", 100), o("4", "TRI", 60, 552), o("2", "TRI", 40, 516)), noise=6,
          cutoff=2000, emph=0.0, cntr=co(1.0), kbd=100, fenv=(0, 400, 0), env=(1, 5000, 0, 2500), poly=True,
          send=(0, 20, 30, 100), rv=("HALL", 110, 80), pat=7)
    b.add("MUSIC BOX", "keys", (o("2", "TRI", 100), o("4", "SQR", 35), osc_off()),
          cutoff=2900, emph=0.8, cntr=co(1.0), kbd=100, fenv=(0, 200, 0), env=(1, 500, 0, 500), poly=True,
          send=(0, 20, 45, 60), rv=("ROOM", 60, 85), dl=("DIGITAL", 375, 35),
          arp=A("DN", "1/8", 2, 0.7, 0, 127), arp_on=True, pat=7)
    b.add("ACCORDION", "keys", (o("8", "PN", 90), o("8", "PN", 90, 25), o("4", "PW", 60, -12)),
          cutoff=1430, emph=2.5, cntr=co(0.5), kbd=90, fenv=(0, 100, 127), env=(35, 200, 118, 90), poly=True,
          vib=14, mosc=True, lfo=(5.8, "SIN", 0, "OFF"), trem=15,
          send=(0, 30, 0, 30), rv=("ROOM", 50, 80), pat=6)
    b.add("FARFISA", "keys", (o("8", "SQR", 100), o("4", "SQR", 70), o("16", "PW", 40)),
          cutoff=1670, emph=2.9, cntr=co(0.5), kbd=100, fenv=(0, 100, 100), env=(4, 100, 120, 70), poly=True,
          vib=16, mosc=True, lfo=(6.0, "SIN", 300, "OFF"),
          send=(0, 70, 0, 30), rv=("ROOM", 40, 80), ch=("STEREO", 80, 5.5), pat=6)


def atmospheres(b):                      # 80-89: textures and drones
    b.add("DARK DRONE", "atmo", (o("32", "SAW", 100), o("16", "SAW", 100, -9), o("16", "SAW", 80, 15)), noise=15, pink=True,
          cutoff=158, emph=3.5, cntr=co(0.5), kbd=0, fenv=(3000, 4000, 70), env=(2500, 3000, 118, 3200),
          vib=70, mflt=True, lfo=(0.05, "SIN", 0, "OFF"),
          send=(40, 0, 0, 80), rv=("HALL", 127, 30), dr=("TUBE", 40, 64), pat=8)
    b.add("STAR DUST", "atmo", (o("2", "TRI", 90), o("4", "TRI", 70, 5), o("2", "SQR", 30, -7)),
          cutoff=2720, emph=0.0, cntr=0, kbd=100, fenv=(0, 100, 127), env=(3, 900, 0, 900), poly=True,
          send=(0, 50, 70, 110), rv=("CLOUD", 127, 90), dl=("P-PONG", 375, 60), ch=("STEREO", 50, 0.3),
          arp=A("RND", "1/16", 4, 0.4, 0, 95), arp_on=True, pat=13)
    b.add("ALIEN CHOIR", "atmo", (o("8", "PW", 90), o("8", "PN", 90, 23), o("4", "TRI", 50)),
          cutoff=567, emph=5.8, cntr=co(1.0), kbd=50, fenv=(1500, 2000, 60), env=(1500, 1500, 118, 2500), poly=True,
          vib=60, mflt=True, lfo=(0.8, "S&H", 0, "OFF"),
          send=(0, 90, 60, 100), rv=("CLOUD", 120, 60), dl=("REVERSE", 560, 50), pat=5)
    b.add("GLASS HARP", "atmo", (o("4", "TRI", 90), o("4", "TRI", 60, 693), o("2", "TRI", 40, 516)),
          cutoff=3080, emph=2.4, cntr=co(0.5), kbd=100, fenv=(0, 100, 127), env=(1800, 2000, 118, 3500), poly=True,
          vib=20, mosc=True, lfo=(0.4, "SIN", 2000, "OFF"),
          send=(0, 60, 50, 110), rv=("CLOUD", 127, 100), dl=("TAPE", 750, 55), ch=("MULTI", 60, 0.15), pat=5)
    b.add("OCEAN TIDE", "atmo", (osc_off(), osc_off(), osc_off()), noise=100, pink=True,
          cutoff=303, emph=3.3, cntr=co(2.0), kbd=0, fenv=(3500, 4500, 20), env=(3000, 4000, 118, 3200),
          vib=80, mflt=True, lfo=(0.3, "SIN", 0, "2BAR"),
          send=(0, 60, 0, 90), rv=("CLOUD", 120, 45), pat=8)
    b.add("PULSAR", "atmo", (o("8", "SAW", 90), o("8", "SAW", 90, -13), o("4", "TRI", 50)),
          cutoff=555, emph=3.0, cntr=co(1.0), kbd=70, fenv=(0, 600, 70), env=(200, 600, 118, 900), poly=True,
          lfo=(4.0, "SIN", 0, "1/8"), trem=95,
          send=(0, 70, 70, 90), rv=("HALL", 100, 60), dl=("DIGITAL", 375, 55), pat=5)
    b.add("IRON DRONE", "atmo", (o("16", "SAW", 90), o("16", "SAW", 90, 693), o("8", "SQR", 70, -420)),
          cutoff=419, emph=7.3, cntr=co(1.0), kbd=0, fenv=(2500, 3500, 50), env=(2000, 2500, 118, 3200),
          vib=55, mflt=True, lfo=(0.7, "S&H", 0, "OFF"), trem=30,
          send=(70, 0, 50, 70), rv=("HALL", 120, 25), dl=("REVERSE", 700, 50), dr=("TUBE", 40, 70), pat=8)
    b.add("GHOST SEQ", "atmo", (o("8", "PW", 90), o("4", "TRI", 60), osc_off()),
          cutoff=873, emph=3.6, cntr=co(1.8), kbd=70, fenv=(0, 400, 15), env=(5, 900, 15, 900), poly=True,
          send=(0, 60, 80, 100), rv=("CLOUD", 110, 55), dl=("P-PONG", 375, 60),
          arp=A("WALK", "1/8", 3, 0.5, 0, 100), arp_on=True, pat=13)
    b.add("HORROR", "atmo", (o("8", "SAW", 90), o("8", "SAW", 85, 100), o("8", "SAW", 60, 600)),
          cutoff=376, emph=3.6, cntr=co(1.5), kbd=40, fenv=(3000, 4000, 40), env=(2500, 2500, 118, 3500), poly=True,
          vib=80, mosc=True, lfo=(4.5, "SIN", 3000, "OFF"), trem=50,
          send=(30, 50, 40, 100), rv=("HALL", 127, 30), dl=("REVERSE", 560, 50), pat=5)
    b.add("SINGING BOWL", "atmo", (o("8", "TRI", 100), o("4", "TRI", 40, 552), o("8", "TRI", 70, 18)),
          cutoff=1770, emph=0.0, cntr=0, kbd=100, fenv=(0, 100, 127), env=(30, 4500, 0, 3500), poly=True,
          lfo=(2.0, "SIN", 0, "OFF"), trem=25,
          send=(0, 40, 30, 100), rv=("HALL", 120, 85), pat=7)


def sfx(b):                              # 90-99: effects
    b.add("JET SWEEP", "fx", (osc_off(), osc_off(), osc_off()), noise=127,
          cutoff=192, emph=6.6, cntr=co(4.5), kbd=0, fenv=(3500, 3500, 0), env=(1500, 3000, 100, 3000),
          send=(0, 100, 0, 50), rv=("PLATE", 70, 90), ch=("JUNO", 127, 0.2), pat=7)
    b.add("THUNDER", "fx", (o("32", "SQR", 40), osc_off(), osc_off()), noise=127, pink=True,
          cutoff=107, emph=0.6, cntr=co(3.0), kbd=0, fenv=(10, 1800, 0), env=(30, 4500, 0, 2500),
          lfo=(1.8, "S&H", 0, "OFF"), trem=70,
          send=(60, 0, 0, 127), rv=("HALL", 127, 25), dr=("SOFT", 40, 64), pat=7)
    b.add("WIND", "fx", (osc_off(), osc_off(), osc_off()), noise=100, pink=True,
          cutoff=477, emph=3.5, cntr=co(0.5), kbd=0, fenv=(0, 100, 127), env=(1500, 1000, 118, 2500),
          vib=90, mflt=True, lfo=(0.6, "S&H", 0, "OFF"), trem=35,
          send=(0, 50, 0, 70), rv=("HALL", 90, 50), pat=7)
    b.add("SIREN", "fx", (o("8", "SQR", 110), osc_off(), osc_off()),
          cutoff=1470, emph=2.7, cntr=0, kbd=0, fenv=(0, 100, 127), env=(80, 300, 120, 300),
          lfo=(0.45, "TRI", 0, "OFF"), pit=st_pit(7),
          send=(0, 0, 40, 45), rv=("PLATE", 60, 80), dl=("ANALOG", 250, 30), pat=7)
    b.add("TELEPHONE", "fx", (o("4", "TRI", 100), o("4", "TRI", 100, 230), osc_off()),
          cutoff=2740, emph=0.0, cntr=0, kbd=0, fenv=(0, 100, 127), env=(1, 100, 127, 30), poly=True,
          lfo=(20.0, "SQR", 0, "OFF"), trem=127,
          send=(0, 0, 0, 25), rv=("ROOM", 25, 90),
          arp=A("REPEAT", "1/2", 1, 0.75, 0, 127), arp_on=True, pat=7)
    b.add("POWER UP", "fx", (o("8", "TRI", 100), o("4", "SQR", 55), osc_off()),
          cutoff=2270, emph=2.9, cntr=co(1.0), kbd=0, fenv=(0, 100, 127), env=(0, 80, 100, 40), poly=True,
          send=(0, 0, 25, 20), rv=("ROOM", 20, 100), dl=("DIGITAL", 188, 30),
          arp=A("UP", "1/32", 4, 0.8, 0, 127), arp_on=True, pat=13)
    b.add("LASER ZAP", "fx", (o("2", "SAW", 100), o("4", "SAW", 70), osc_off()),
          cutoff=477, emph=4.8, cntr=co(4.5), kbd=0, fenv=(0, 200, 0), env=(0, 220, 0, 100), penv=st_pit(11.8),
          send=(0, 0, 35, 40), rv=("PLATE", 50, 85), dl=("DIGITAL", 188, 40), pat=7)
    b.add("HELICOPTER", "fx", (o("32", "SQR", 100), o("16", "SAW", 60, 0), osc_off()), noise=127,
          cutoff=954, emph=2.8, cntr=0, kbd=0, fenv=(0, 100, 127), env=(300, 500, 118, 600),
          lfo=(8.0, "TRI", 0, "1/16"), trem=110,
          send=(0, 0, 0, 40), rv=("ROOM", 40, 60), pat=7)
    b.add("ROBOT", "fx", (o("8", "SQR", 127), o("8", "SAW", 90, 17), osc_off()),
          cutoff=1010, emph=6.8, cntr=co(2.0), kbd=0, fenv=(0, 80, 0), env=(1, 120, 0, 80), poly=True,
          lfo=(12.0, "S&H", 0, "1/16"), pit=st_pit(5),
          send=(10, 0, 70, 50), rv=("ROOM", 40, 85), dl=("P-PONG", 188, 55),
          arp=A("RND", "1/16", 4, 0.4, 0, 100), arp_on=True, pat=13)
    b.add("UFO", "fx", (o("8", "SQR", 100), o("4", "TRI", 50), osc_off()),
          cutoff=1300, emph=3.7, cntr=0, kbd=0, fenv=(0, 100, 127), env=(400, 500, 118, 1500), glide=250,
          lfo=(6.0, "SIN", 1500, "OFF"), pit=st_pit(9),
          send=(0, 100, 60, 90), rv=("CLOUD", 110, 80), dl=("REVERSE", 560, 50), pat=7)


# ---- 100..199 (1.14.1): the second hundred, the same ten groups in the same order (110-119 hard leads, ...).
# Their names are not the first hundred's, and their sounds are written from the same parts: the groups stay the reference.

def show2(b):                            # 100-109: showcase
    b.add("SUNRISE", "lead", (o("8", "SAW", 90), o("8", "SAW", 90, 9), o("16", "TRI", 60)),
          cutoff=1200, emph=3.2, cntr=co(2.2), kbd=64, fenv=(0, 300, 60), env=(4, 350, 115, 220),
          vib=25, mosc=False, lfo=(5.8, "SIN", 350, "OFF"), glide=60,
          send=(25, 0, 50, 50), rv=("HALL", 65, 80), dl=("ANALOG", 330, 40), dr=("TUBE", 60, 64), pat=4)
    b.add("NIGHT SWIM", "pad", (o("8", "SAW", 75), o("8", "SAW", 75, 15), o("4", "TRI", 50, -15)),
          cutoff=880, emph=2.0, cntr=0, kbd=64, fenv=(200, 800, 90), env=(600, 700, 115, 900), poly=True,
          vib=10, lfo=(4.2, "SIN", 600, "OFF"), send=(0, 70, 20, 60), rv=("CLOUD", 70, 80), ch=("MULTI", 60, 0.4), pat=5)
    b.add("GLASS ARP", "pluck", (o("8", "TRI", 100), o("8", "SQR", 60, 12), o("4", "TRI", 40)),
          cutoff=1600, emph=2.4, cntr=co(2.0), kbd=90, fenv=(0, 150, 0), env=(1, 260, 0, 180), poly=True,
          send=(0, 30, 60, 50), rv=("PLATE", 55, 90), dl=("DIGITAL", 250, 45), ch=("STEREO", 50, 0.3),
          arp=A("UPDN", "1/16", 2, 0.4, 0, 127), arp_on=True, pat=13)
    b.add("MINI BASS", "bass", (o("16", "SAW", 120), o("16", "SQR", 100, 7), o("32", "SQR", 80)),
          cutoff=260, emph=4.0, cntr=co(2.8), kbd=40, fenv=(0, 180, 30), env=(2, 240, 60, 120),
          send=(30, 0, 0, 20), rv=("ROOM", 30, 80), dr=("TUBE", 70, 64), pat=2)
    b.add("VELVET EP", "keys", (o("8", "TRI", 100), o("4", "TRI", 55), o("2", "TRI", 25)),
          cutoff=1100, emph=2.2, cntr=co(1.5), kbd=90, fenv=(0, 500, 20), env=(2, 1400, 15, 300), poly=True,
          send=(0, 60, 0, 30), rv=("ROOM", 35, 85), ch=("JUNO", 70, 0.7), pat=6)
    b.add("TIN WHISTLE", "lead", (o("4", "SHK", 90), o("4", "SAW", 40, 5), o("8", "PW", 30)),
          cutoff=1900, emph=3.9, cntr=co(1.0), kbd=100, fenv=(10, 400, 70), env=(15, 500, 110, 200),
          vib=40, lfo=(5.2, "TRI", 250, "OFF"), glide=20,
          send=(0, 0, 40, 60), rv=("SPRING", 45, 85), pat=4)
    b.add("BRASS RISE", "brass", (o("8", "SAW", 95), o("8", "SAW", 95, 12), o("16", "SAW", 70, -12)),
          cutoff=720, emph=3.4, cntr=co(2.6), kbd=70, fenv=(120, 700, 80), env=(60, 500, 116, 420), poly=True, oct=-1,
          vib=15, lfo=(5.0, "SIN", 500, "OFF"), send=(10, 50, 0, 40), rv=("HALL", 60, 80), ch=("STEREO", 45, 0.6), pat=5)
    b.add("STACCATO", "pluck", (o("8", "TRI", 100), o("4", "TRI", 70, 3), o("16", "SQR", 30)),
          cutoff=1350, emph=2.0, cntr=co(2.2), kbd=80, fenv=(0, 120, 0), env=(1, 200, 0, 130), poly=True,
          send=(0, 20, 0, 55), rv=("ROOM", 40, 90), pat=13)
    b.add("MONO SYNC", "lead", (o("8", "SAW", 100), o("8", "SAW", 100, 27), o("16", "SQR", 40)),
          cutoff=1300, emph=4.1, cntr=co(2.6), kbd=90, fenv=(0, 350, 40), env=(2, 280, 110, 200), glide=90,
          vib=20, lfo=(6.2, "TRI", 300, "OFF"), send=(40, 0, 40, 40), rv=("PLATE", 50, 85), dl=("TAPE", 300, 35),
          dr=("TUBE", 80, 66), pat=4)

    b.add("SOUL KEYS", "keys", (o("8", "SQR", 80), o("8", "TRI", 70, 9), o("4", "TRI", 45)),
          cutoff=1180, emph=2.4, cntr=co(1.8), kbd=90, fenv=(0, 350, 40), env=(3, 900, 40, 350), poly=True,
          send=(10, 40, 0, 40), rv=("PLATE", 55, 85), ch=("STEREO", 40, 0.5), pat=6)


def hard2(b):                            # 110-119: intense, aggressive, big
    b.add("OVERDRIVE", "lead", (o("8", "SAW", 105), o("8", "SAW", 105, -13), o("8", "SQR", 70, 4)),
          cutoff=980, emph=5.2, cntr=co(1.8), kbd=90, fenv=(0, 500, 60), env=(1, 260, 120, 240),
          vib=22, lfo=(5.7, "SIN", 450, "OFF"), glide=110,
          send=(75, 0, 45, 30), rv=("PLATE", 40, 80), dl=("TAPE", 275, 32), dr=("TUBE", 90, 70), pat=4)
    b.add("MACHINE", "lead", (o("8", "SQR", 100), o("8", "SQR", 100, 19), o("4", "SAW", 55)),
          cutoff=1560, emph=4.9, cntr=co(2.0), kbd=100, fenv=(0, 250, 90), env=(0, 180, 120, 150),
          lfo=(9.0, "SQR", 0, "OFF"), trem=30,
          send=(40, 0, 30, 35), rv=("ROOM", 45, 75), dl=("DIGITAL", 188, 40), dr=("TUBE", 70, 70), pat=4)
    b.add("SAW WALL", "lead", (o("8", "SAW", 110), o("8", "SAW", 110, 22), o("8", "SAW", 90, -22)),
          cutoff=720, emph=4.4, cntr=co(2.2), kbd=75, fenv=(0, 400, 50), env=(3, 500, 115, 350), poly=False,
          vib=30, mosc=True, lfo=(5.4, "SIN", 600, "OFF"), send=(55, 0, 40, 40), rv=("HALL", 55, 80),
          dl=("ANALOG", 300, 42), pat=4)
    b.add("STRIKE", "lead", (o("8", "SHK", 100), o("4", "SQR", 80, 14), o("16", "SAW", 60)),
          cutoff=1100, emph=5.6, cntr=co(3.0), kbd=85, fenv=(0, 150, 0), env=(0, 220, 80, 120),
          send=(50, 30, 40, 30), rv=("SPRING", 55, 80), dr=("TUBE", 85, 72), pat=13)
    b.add("BIG HOOK", "lead", (o("8", "SAW", 100), o("8", "PW", 90, 7), o("4", "SQR", 60, -7)),
          cutoff=1000, emph=4.5, cntr=co(2.4), kbd=80, fenv=(20, 600, 70), env=(6, 400, 120, 300),
          vib=18, lfo=(5.1, "SIN", 500, "OFF"), glide=40,
          send=(45, 20, 40, 45), rv=("HALL", 70, 75), ch=("JUNO", 70, 0.6), dl=("SLAP", 150, 40), pat=4)
    b.add("ANGRY BUZZ", "lead", (o("8", "SAW", 100), o("8", "SAW", 100, 2), o("8", "SQR", 100, -2)),
          cutoff=600, emph=6.0, cntr=co(3.4), kbd=60, fenv=(0, 350, 40), env=(2, 300, 110, 180), poly=False,
          lfo=(14.0, "SQR", 0, "OFF"), trem=60,
          send=(85, 0, 25, 25), rv=("ROOM", 30, 70), dr=("TUBE", 100, 60), pat=4)
    b.add("CRUNCH", "lead", (o("4", "SAW", 105), o("4", "SQR", 80, 9), o("8", "SAW", 50, -9)),
          cutoff=1800, emph=5.0, cntr=co(2.9), kbd=95, fenv=(0, 220, 30), env=(1, 200, 115, 150), glide=50,
          vib=12, lfo=(6.8, "SAW", 200, "OFF"),
          send=(70, 0, 20, 30), rv=("SPRING", 40, 80), dl=("DIGITAL", 188, 30), dr=("TUBE", 95, 66), pat=4)
    b.add("CONTROL", "lead", (o("8", "SAW", 100), o("16", "SAW", 90, 5), o("4", "SQR", 70, -5)),
          cutoff=850, emph=4.7, cntr=co(2.0), kbd=85, fenv=(0, 600, 65), env=(3, 380, 120, 260), glide=80,
          send=(35, 0, 60, 40), rv=("PLATE", 50, 80), dl=("P-PONG", 250, 45), pat=4)
    b.add("SIREN LEAD", "lead", (o("8", "SQR", 100), o("8", "SQR", 100, 14), osc_off()),
          cutoff=1700, emph=3.6, cntr=co(1.6), kbd=100, fenv=(10, 300, 110), env=(10, 200, 118, 160), glide=160,
          lfo=(0.5, "TRI", 0, "OFF"), pit=st_pit(1.5),
          send=(25, 0, 40, 40), rv=("PLATE", 45, 85), pat=4)
    b.add("MOSQUITO", "lead", (o("2", "SAW", 90), o("4", "SQR", 45, 17), osc_off()),
          cutoff=2100, emph=5.8, cntr=co(2.2), kbd=100, fenv=(0, 90, 80), env=(0, 120, 100, 90),
          vib=50, lfo=(9.5, "SIN", 0, "OFF"), send=(30, 0, 30, 35), rv=("ROOM", 35, 90), dl=("DIGITAL", 120, 30), pat=4)


def soft2(b):                            # 120-129: gentler, singing, mellow
    b.add("MELLOW MAN", "lead", (o("8", "TRI", 100), o("8", "TRI", 80, 5), o("16", "SAW", 35)),
          cutoff=1060, emph=2.1, cntr=co(1.3), kbd=80, fenv=(60, 600, 75), env=(120, 500, 115, 350),
          vib=40, lfo=(5.0, "SIN", 500, "OFF"), glide=120,
          send=(0, 60, 20, 55), rv=("HALL", 60, 75), ch=("STEREO", 60, 0.6), pat=4)
    b.add("FLUTE SOFT", "lead", (o("8", "SHK", 60), o("8", "TRI", 90, 2), osc_off()),
          cutoff=1550, emph=1.5, cntr=co(1.0), kbd=100, fenv=(120, 500, 100), env=(180, 350, 118, 300),
          vib=55, lfo=(5.3, "SIN", 600, "OFF"),
          send=(0, 40, 0, 65), rv=("ROOM", 40, 90), pat=4)
    b.add("HONEY LEAD", "lead", (o("8", "SAW", 85), o("8", "SAW", 85, 8), o("4", "TRI", 40)),
          cutoff=900, emph=2.7, cntr=co(1.6), kbd=70, fenv=(80, 900, 80), env=(60, 600, 115, 420), glide=70,
          vib=32, lfo=(4.8, "SIN", 700, "OFF"),
          send=(10, 40, 50, 55), rv=("HALL", 65, 80), dl=("ANALOG", 420, 36), pat=4)
    b.add("MOONLIGHT", "lead", (o("8", "TRI", 100), o("4", "TRI", 50, 4), osc_off()),
          cutoff=1280, emph=1.9, cntr=co(1.0), kbd=85, fenv=(200, 800, 90), env=(250, 800, 110, 700),
          vib=28, lfo=(4.5, "SIN", 800, "OFF"),
          send=(0, 55, 35, 70), rv=("CLOUD", 60, 85), ch=("JUNO", 55, 0.5), pat=4)
    b.add("SOFT CLAR", "lead", (o("8", "SQR", 75), o("8", "PW", 50, 6), o("16", "TRI", 40)),
          cutoff=1000, emph=2.6, cntr=co(1.4), kbd=75, fenv=(90, 700, 70), env=(90, 500, 116, 380),
          vib=22, lfo=(5.1, "TRI", 400, "OFF"), glide=40,
          send=(0, 30, 0, 45), rv=("ROOM", 35, 85), pat=4)
    b.add("VOICE OOH", "lead", (o("8", "PW", 80), o("8", "TRI", 60, 12), osc_off()),
          cutoff=1320, emph=2.3, cntr=co(1.2), kbd=85, fenv=(150, 900, 85), env=(200, 700, 112, 500),
          vib=35, lfo=(4.9, "SIN", 600, "OFF"),
          send=(0, 60, 0, 60), rv=("HALL", 70, 75), ch=("STEREO", 50, 0.4), pat=4)
    b.add("WARM SQR", "lead", (o("8", "SQR", 90), o("8", "SQR", 90, 6), o("16", "SAW", 40)),
          cutoff=760, emph=2.9, cntr=co(1.5), kbd=70, fenv=(40, 800, 80), env=(30, 600, 110, 400),
          vib=15, mosc=True, lfo=(5.0, "SIN", 300, "OFF"), glide=30,
          send=(15, 40, 40, 40), rv=("PLATE", 50, 80), pat=4)
    b.add("SILK PAD LD", "lead", (o("4", "SAW", 70), o("4", "SAW", 70, 10), o("8", "TRI", 40)),
          cutoff=1450, emph=2.0, cntr=co(1.1), kbd=90, fenv=(200, 900, 70), env=(150, 500, 116, 550),
          vib=18, lfo=(4.4, "SIN", 500, "OFF"), glide=60,
          send=(0, 50, 30, 60), rv=("HALL", 60, 80), ch=("MULTI", 50, 0.3), pat=4)
    b.add("BELL LEAD", "lead", (o("4", "TRI", 90), o("4", "TRI", 60, 700), osc_off()),
          cutoff=1800, emph=1.6, cntr=co(1.5), kbd=100, fenv=(0, 1200, 0), env=(1, 1600, 0, 900),
          send=(0, 20, 60, 70), rv=("PLATE", 70, 90), dl=("DIGITAL", 375, 45), pat=4)
    b.add("DREAM FLUTE", "lead", (o("8", "SHK", 40), o("8", "SAW", 60, -4), o("4", "TRI", 50)),
          cutoff=1400, emph=2.2, cntr=co(1.2), kbd=100, fenv=(250, 800, 90), env=(300, 600, 118, 500),
          vib=60, lfo=(3.8, "TRI", 900, "OFF"),
          send=(0, 40, 40, 70), rv=("CLOUD", 65, 85), pat=4)


def bass2(b):                            # 130-139: mono, round to fat
    b.add("SUB SINE", "bass", (o("16", "TRI", 127), o("32", "TRI", 60), osc_off()),
          cutoff=180, emph=1.0, cntr=0, kbd=50, fenv=(0, 120, 0), env=(1, 350, 90, 90),
          send=(0, 0, 0, 0), rv=("ROOM", 20, 80), pat=2)
    b.add("ROUND 808", "bass", (o("16", "SQR", 110), o("32", "SQR", 60, 4), osc_off()),
          cutoff=260, emph=2.5, cntr=co(2.2), kbd=40, fenv=(0, 500, 0), env=(1, 800, 0, 350),
          pit=st_pit(-2), penv=st_pit(-6), send=(20, 0, 0, 20), rv=("ROOM", 25, 80), pat=2)
    b.add("RUBBER SUB", "bass", (o("8", "SAW", 110), o("8", "SAW", 110, 8), o("16", "SQR", 60)),
          cutoff=390, emph=3.6, cntr=co(2.6), kbd=50, fenv=(0, 180, 30), env=(1, 260, 55, 110),
          send=(20, 0, 15, 15), rv=("ROOM", 25, 85), dr=("SOFT", 60, 64), pat=2)
    b.add("OCTAVE PLK", "bass", (o("16", "SAW", 110), o("8", "SAW", 90, 0), osc_off()),
          cutoff=440, emph=3.1, cntr=co(2.9), kbd=60, fenv=(0, 150, 0), env=(1, 220, 0, 90),
          send=(10, 0, 10, 10), rv=("ROOM", 20, 90), pat=2, arp_on=True,
          arp=A("UP", "1/16", 2, 0.5, 0, 127))
    b.add("WOBBLE", "bass", (o("16", "SAW", 115), o("16", "SQR", 80, 11), osc_off()),
          cutoff=300, emph=5.4, cntr=co(2.4), kbd=40, fenv=(0, 400, 40), env=(1, 350, 100, 120),
          lfo=(3.5, "TRI", 0, "OFF"), mflt=True, vib=0, send=(30, 0, 0, 20), rv=("ROOM", 20, 80), dr=("TUBE", 70, 64), pat=2)
    b.add("STEP BASS", "bass", (o("16", "SAW", 115), o("16", "SAW", 115, -6), o("32", "SQR", 40)),
          cutoff=280, emph=3.9, cntr=co(2.7), kbd=50, fenv=(0, 220, 20), env=(1, 260, 60, 110),
          arp=A("UP", "1/16", 1, 0.6, 0, 127), arp_on=True, send=(15, 0, 0, 10), rv=("ROOM", 20, 80), pat=2)
    b.add("REED BASS", "bass", (o("8", "PW", 110), o("16", "PW", 60, 6), osc_off()),
          cutoff=520, emph=2.2, cntr=co(1.8), kbd=60, fenv=(10, 300, 40), env=(5, 300, 85, 160),
          vib=12, lfo=(5.0, "SIN", 300, "OFF"), send=(0, 0, 0, 25), rv=("ROOM", 30, 80), pat=2)
    b.add("TUBE BASS", "bass", (o("16", "SAW", 105), o("8", "TRI", 60, -5), osc_off()),
          cutoff=340, emph=3.4, cntr=co(2.2), kbd=50, fenv=(0, 260, 30), env=(3, 340, 70, 120),
          send=(45, 0, 0, 15), rv=("ROOM", 20, 80), dr=("TUBE", 80, 66), pat=2)
    b.add("DOUBLE SAW", "bass", (o("16", "SAW", 110), o("16", "SAW", 110, 15), o("8", "SQR", 50)),
          cutoff=230, emph=4.4, cntr=co(3.0), kbd=40, fenv=(0, 300, 30), env=(1, 300, 90, 140),
          send=(20, 0, 0, 15), rv=("ROOM", 20, 80), pat=2)
    b.add("PLUCK BASS", "bass", (o("8", "TRI", 115), o("8", "SQR", 80, 5), o("16", "TRI", 40)),
          cutoff=600, emph=3.0, cntr=co(3.0), kbd=60, fenv=(0, 120, 0), env=(1, 180, 0, 70),
          send=(0, 0, 25, 25), rv=("ROOM", 25, 85), dl=("ANALOG", 250, 30), pat=2)


def pad2(b):                             # 140-149: pads and strings
    b.add("VELVET PAD", "pad", (o("8", "SAW", 80), o("8", "SAW", 80, 12), o("16", "TRI", 50)),
          cutoff=900, emph=2.1, cntr=0, kbd=64, fenv=(500, 900, 90), env=(700, 800, 116, 900), poly=True,
          vib=12, lfo=(4.0, "SIN", 700, "OFF"), send=(0, 80, 0, 70), rv=("HALL", 80, 80), ch=("MULTI", 70, 0.4), pat=5)
    b.add("AURORA", "pad", (o("8", "TRI", 85), o("8", "SAW", 60, 17), o("4", "TRI", 40, -17)),
          cutoff=1400, emph=1.8, cntr=0, kbd=60, fenv=(800, 1200, 100), env=(900, 1000, 120, 1200), poly=True,
          vib=22, mosc=True, lfo=(0.3, "SIN", 0, "OFF"), send=(0, 60, 30, 90), rv=("CLOUD", 85, 75), dl=("TAPE", 500, 50), pat=5)
    b.add("MIST STR", "pad", (o("8", "SAW", 75), o("8", "SAW", 75, 9), o("8", "SAW", 60, -9)),
          cutoff=1080, emph=2.5, cntr=0, kbd=64, fenv=(400, 700, 100), env=(500, 600, 116, 800), poly=True,
          vib=20, lfo=(5.0, "SIN", 600, "OFF"), send=(0, 70, 0, 60), rv=("HALL", 70, 80), ch=("STEREO", 70, 0.5), pat=5)
    b.add("SWELL PAD", "pad", (o("8", "SAW", 90), o("16", "SAW", 60, 5), o("8", "TRI", 40)),
          cutoff=760, emph=2.0, cntr=co(0.8), kbd=60, fenv=(1200, 1500, 110), env=(1500, 900, 118, 1000), poly=True,
          lfo=(0.15, "TRI", 0, "OFF"), mflt=True, send=(0, 40, 40, 90), rv=("CLOUD", 90, 70), pat=5)
    b.add("OBOE PAD", "pad", (o("8", "PW", 80), o("8", "PW", 60, 7), o("4", "SAW", 35)),
          cutoff=1200, emph=2.6, cntr=co(1.0), kbd=70, fenv=(300, 900, 80), env=(400, 800, 112, 700), poly=True,
          vib=30, lfo=(4.6, "SIN", 500, "OFF"), send=(0, 50, 0, 60), rv=("HALL", 75, 75), ch=("JUNO", 55, 0.5), pat=5)
    b.add("CHOIR SOFT", "pad", (o("8", "TRI", 90), o("8", "TRI", 90, 14), o("16", "SAW", 40)),
          cutoff=1000, emph=2.2, cntr=0, kbd=64, fenv=(600, 1000, 95), env=(800, 900, 118, 1100), poly=True,
          vib=25, lfo=(4.4, "SIN", 500, "OFF"), send=(0, 60, 0, 80), rv=("CLOUD", 80, 80), pat=5)
    b.add("HEAVEN", "pad", (o("8", "SAW", 85), o("8", "SAW", 85, 20), o("16", "SAW", 60, -20)),
          cutoff=1250, emph=2.4, cntr=co(0.6), kbd=64, fenv=(1000, 1400, 100), env=(1100, 1000, 118, 1300), poly=True,
          vib=34, mosc=True, lfo=(0.2, "SIN", 0, "OFF"), send=(0, 80, 60, 100), rv=("HALL", 95, 70), pat=5)
    b.add("GLOW PAD", "pad", (o("8", "TRI", 90), o("8", "TRI", 90, 8), o("4", "TRI", 50, -8)),
          cutoff=1700, emph=1.5, cntr=0, kbd=70, fenv=(500, 1000, 110), env=(600, 900, 118, 900), poly=True,
          vib=18, lfo=(3.0, "SIN", 500, "OFF"), send=(0, 60, 40, 80), rv=("PLATE", 80, 85), dl=("DIGITAL", 500, 40), pat=5)
    b.add("MIDNIGHT ST", "pad", (o("8", "SAW", 70), o("8", "PW", 60, 12), o("16", "SAW", 45, -5)),
          cutoff=640, emph=2.8, cntr=co(0.8), kbd=60, fenv=(700, 900, 90), env=(800, 800, 116, 900), poly=True,
          vib=16, lfo=(4.7, "SIN", 400, "OFF"), send=(10, 60, 0, 70), rv=("HALL", 70, 70), ch=("MULTI", 60, 0.4), pat=5)
    b.add("BREATHE", "pad", (o("8", "PN", 40), o("8", "SAW", 80), o("16", "TRI", 50, 6)),
          cutoff=920, emph=2.0, cntr=co(0.5), kbd=64, fenv=(900, 1400, 100), env=(1000, 1000, 112, 1100), poly=True,
          lfo=(0.12, "TRI", 0, "OFF"), mflt=True, noise=10, send=(0, 40, 0, 90), rv=("CLOUD", 90, 70), pat=5)


def brass2(b):                           # 150-159
    b.add("BIG BRASS", "brass", (o("8", "SAW", 95), o("8", "SAW", 95, 10), o("16", "SQR", 60)),
          cutoff=760, emph=3.0, cntr=co(2.4), kbd=70, fenv=(80, 600, 85), env=(50, 500, 118, 380), poly=True,
          vib=12, lfo=(5.3, "SIN", 400, "OFF"), send=(15, 60, 0, 40), rv=("HALL", 60, 80), ch=("STEREO", 40, 0.5), pat=5)
    b.add("MUTE TRP", "brass", (o("8", "SAW", 90), o("8", "PW", 60, 7), o("16", "SAW", 40, -7)),
          cutoff=1100, emph=4.0, cntr=co(2.8), kbd=70, fenv=(20, 400, 60), env=(30, 350, 110, 200), poly=True,
          send=(20, 0, 30, 30), rv=("ROOM", 40, 80), pat=5)
    b.add("SAX BRASS", "brass", (o("8", "SAW", 85), o("8", "SQR", 60, 6), o("16", "TRI", 40)),
          cutoff=880, emph=2.9, cntr=co(2.0), kbd=70, fenv=(120, 700, 75), env=(100, 500, 115, 400), poly=True,
          vib=30, lfo=(5.0, "SIN", 500, "OFF"), send=(10, 40, 0, 45), rv=("HALL", 60, 80), pat=5)
    b.add("STAB 2", "brass", (o("8", "SAW", 105), o("8", "SAW", 105, 11), o("4", "SQR", 45)),
          cutoff=1400, emph=3.6, cntr=co(3.2), kbd=80, fenv=(0, 180, 0), env=(0, 260, 0, 160), poly=True,
          send=(30, 0, 25, 25), rv=("PLATE", 50, 85), pat=5)
    b.add("HORN SECT", "brass", (o("8", "SAW", 95), o("8", "SAW", 95, 16), o("8", "TRI", 40)),
          cutoff=660, emph=3.2, cntr=co(2.2), kbd=60, fenv=(160, 700, 70), env=(120, 450, 118, 360), poly=True,
          vib=20, mosc=False, lfo=(5.1, "SIN", 500, "OFF"), send=(15, 50, 0, 40), rv=("HALL", 65, 75), pat=5)
    b.add("SYNTH HORN", "brass", (o("8", "SQR", 95), o("8", "SAW", 90, 9), o("16", "SAW", 60, -9)),
          cutoff=900, emph=4.2, cntr=co(2.6), kbd=70, fenv=(40, 500, 80), env=(40, 420, 118, 300), poly=False,
          glide=50, send=(35, 0, 40, 35), rv=("PLATE", 45, 80), dr=("TUBE", 70, 66), pat=5)
    b.add("POWER BRS", "brass", (o("8", "SAW", 100), o("8", "SAW", 100, 13), o("4", "SAW", 50)),
          cutoff=1000, emph=3.4, cntr=co(2.4), kbd=70, fenv=(60, 500, 90), env=(70, 450, 120, 360), poly=True,
          vib=10, lfo=(5.6, "SIN", 300, "OFF"), send=(25, 0, 30, 40), rv=("HALL", 70, 80), ch=("STEREO", 55, 0.7), pat=5)
    b.add("MUTED HRN", "brass", (o("8", "PW", 85), o("8", "SAW", 60, -8), o("16", "SAW", 35)),
          cutoff=720, emph=3.8, cntr=co(2.2), kbd=60, fenv=(30, 300, 50), env=(40, 300, 100, 220), poly=True,
          send=(10, 0, 20, 30), rv=("ROOM", 35, 80), pat=5)
    b.add("OCT BRASS", "brass", (o("8", "SAW", 90), o("4", "SAW", 75, 7), o("16", "SQR", 45)),
          cutoff=1200, emph=3.1, cntr=co(2.5), kbd=70, fenv=(100, 600, 85), env=(60, 480, 116, 420), poly=True,
          vib=14, lfo=(5.2, "SIN", 400, "OFF"), send=(15, 40, 0, 45), rv=("HALL", 60, 80), pat=5)
    b.add("BRASS DRONE", "brass", (o("16", "SAW", 90), o("16", "SAW", 90, 13), o("8", "SQR", 45)),
          cutoff=600, emph=2.6, cntr=co(1.8), kbd=60, fenv=(400, 900, 100), env=(500, 700, 118, 800), poly=True,
          vib=20, lfo=(4.8, "SIN", 500, "OFF"), send=(0, 50, 0, 60), rv=("HALL", 70, 75), pat=5)


def pluck2(b):                           # 160-169: plucks and mallets (poly)
    b.add("HARP 2", "pluck", (o("8", "TRI", 100), o("4", "TRI", 70, 5), o("8", "SAW", 30)),
          cutoff=1200, emph=1.8, cntr=co(2.0), kbd=90, fenv=(0, 300, 0), env=(1, 800, 0, 400), poly=True,
          send=(0, 40, 0, 60), rv=("HALL", 60, 80), pat=13)
    b.add("KOTO", "pluck", (o("8", "SQR", 90), o("8", "SQR", 80, 12), o("16", "TRI", 40)),
          cutoff=1700, emph=2.4, cntr=co(2.2), kbd=80, fenv=(0, 400, 0), env=(1, 500, 0, 300), poly=True,
          send=(0, 20, 20, 60), rv=("ROOM", 50, 85), dl=("TAPE", 300, 35), pat=13)
    b.add("MARIMBA 2", "pluck", (o("8", "TRI", 0), o("4", "TRI", 90), o("16", "TRI", 40)),
          cutoff=1900, emph=1.2, cntr=co(1.0), kbd=90, fenv=(0, 200, 0), env=(1, 300, 0, 150), poly=True,
          send=(0, 0, 0, 40), rv=("ROOM", 30, 90), pat=13)
    b.add("CLAV FUNK", "pluck", (o("8", "SQR", 100), o("8", "SAW", 60, 4), o("4", "PW", 40)),
          cutoff=1300, emph=4.4, cntr=co(3.2), kbd=80, fenv=(0, 90, 0), env=(0, 140, 0, 80), poly=True,
          send=(20, 0, 10, 20), rv=("ROOM", 25, 85), pat=13)
    b.add("STEEL PLUCK", "pluck", (o("8", "SAW", 100), o("8", "TRI", 70, 7), o("4", "TRI", 40)),
          cutoff=1100, emph=3.1, cntr=co(2.6), kbd=70, fenv=(0, 160, 0), env=(1, 260, 0, 150), poly=True,
          send=(10, 0, 40, 40), rv=("SPRING", 40, 85), dl=("DIGITAL", 250, 35), pat=13)
    b.add("ACOUSTIC", "pluck", (o("8", "TRI", 90), o("8", "SAW", 35, 2), o("4", "TRI", 45)),
          cutoff=1500, emph=1.5, cntr=co(1.4), kbd=90, fenv=(0, 500, 0), env=(1, 700, 0, 350), poly=True,
          send=(0, 20, 0, 50), rv=("ROOM", 35, 90), pat=13)
    b.add("TICK PLK", "pluck", (o("4", "SQR", 100), o("2", "SHK", 50), osc_off()),
          cutoff=2200, emph=2.7, cntr=co(3.5), kbd=90, fenv=(0, 60, 0), env=(0, 90, 0, 50), poly=True,
          send=(10, 0, 30, 30), rv=("ROOM", 25, 90), dl=("DIGITAL", 188, 25), pat=13)
    b.add("BANJO", "pluck", (o("8", "SAW", 95), o("8", "SQR", 60, -3), o("4", "TRI", 50)),
          cutoff=1450, emph=3.4, cntr=co(2.4), kbd=80, fenv=(0, 220, 0), env=(0, 320, 0, 160), poly=True,
          send=(0, 0, 0, 40), rv=("PLATE", 45, 85), pat=13)
    b.add("PLUCK ARP", "pluck", (o("8", "TRI", 100), o("8", "SAW", 60, 9), o("4", "TRI", 30)),
          cutoff=1400, emph=2.5, cntr=co(2.0), kbd=80, fenv=(0, 160, 0), env=(1, 240, 0, 110), poly=True,
          send=(0, 20, 50, 45), rv=("PLATE", 50, 85), dl=("P-PONG", 375, 40),
          arp=A("UPDN", "1/16", 2, 0.45, 0, 127), arp_on=True, pat=13)
    b.add("CHIME BOX", "pluck", (o("4", "TRI", 0), o("2", "TRI", 100), osc_off()),
          cutoff=2400, emph=0.6, cntr=0, kbd=100, fenv=(0, 500, 0), env=(0, 900, 0, 500), poly=True,
          send=(0, 20, 30, 60), rv=("ROOM", 40, 90), pat=13)


def keys2(b):                            # 170-179: keys and organs (poly)
    b.add("CLAV SOFT", "keys", (o("8", "SQR", 85), o("8", "PW", 40, 6), osc_off()),
          cutoff=1050, emph=2.6, cntr=co(2.2), kbd=90, fenv=(0, 200, 10), env=(1, 500, 10, 200), poly=True,
          send=(0, 30, 0, 30), rv=("ROOM", 30, 85), pat=6)
    b.add("WURLI 2", "keys", (o("8", "TRI", 100), o("4", "TRI", 40, 3), o("16", "SAW", 20)),
          cutoff=1350, emph=2.8, cntr=co(2.0), kbd=90, fenv=(0, 600, 20), env=(1, 1200, 20, 260), poly=True,
          send=(20, 40, 0, 30), rv=("ROOM", 40, 85), ch=("JUNO", 50, 0.5), pat=6)
    b.add("HARPSI 2", "keys", (o("8", "SAW", 90), o("8", "SAW", 90, 9), o("4", "SQR", 40)),
          cutoff=1800, emph=3.8, cntr=co(2.2), kbd=100, fenv=(0, 130, 0), env=(0, 400, 0, 120), poly=True,
          send=(0, 0, 0, 40), rv=("ROOM", 35, 90), pat=6)
    b.add("DRAWBAR", "keys", (o("8", "TRI", 0), o("4", "TRI", 100), o("16", "TRI", 90)),
          cutoff=2200, emph=0.8, cntr=0, kbd=100, fenv=(0, 100, 127), env=(4, 100, 127, 80), poly=True,
          lfo=(6.0, "SIN", 0, "OFF"), trem=25, send=(0, 80, 0, 50), rv=("HALL", 45, 75), ch=("STEREO", 80, 6.0), pat=6)
    b.add("PIPE ORG", "keys", (o("8", "TRI", 90), o("4", "TRI", 80), o("2", "TRI", 60)),
          cutoff=2000, emph=0.4, cntr=0, kbd=100, fenv=(0, 100, 127), env=(60, 100, 127, 150), poly=True,
          vib=0, send=(0, 40, 0, 70), rv=("HALL", 85, 70), pat=7)
    b.add("ELEC PIANO", "keys", (o("8", "TRI", 90), o("8", "SQR", 40, 4), o("4", "TRI", 35)),
          cutoff=1250, emph=2.3, cntr=co(2.0), kbd=90, fenv=(0, 300, 15), env=(1, 1600, 12, 280), poly=True,
          send=(0, 50, 0, 30), rv=("PLATE", 45, 90), dr=("SOFT", 40, 64), pat=6)
    b.add("MELLOTRON", "keys", (o("8", "SAW", 70), o("8", "SAW", 70, 11), o("4", "SAW", 40, -11)),
          cutoff=1000, emph=2.0, cntr=0, kbd=90, fenv=(120, 700, 110), env=(240, 600, 118, 500), poly=True,
          vib=18, mosc=False, lfo=(5.0, "SIN", 400, "OFF"), send=(0, 60, 0, 60), rv=("HALL", 60, 80), pat=6)
    b.add("MALLET KEY", "keys", (o("4", "TRI", 0), o("2", "TRI", 90), o("8", "TRI", 45)),
          cutoff=2100, emph=1.4, cntr=co(1.0), kbd=100, fenv=(0, 300, 0), env=(0, 700, 0, 250), poly=True,
          send=(0, 20, 30, 50), rv=("ROOM", 40, 90), pat=6)
    b.add("CHORUS EP", "keys", (o("8", "TRI", 100), o("8", "TRI", 90, 12), o("4", "TRI", 40)),
          cutoff=1150, emph=2.2, cntr=co(1.8), kbd=90, fenv=(0, 500, 10), env=(2, 1400, 10, 300), poly=True,
          send=(0, 40, 0, 35), rv=("ROOM", 35, 85), ch=("STEREO", 85, 0.9), pat=6)
    b.add("HAMMOND", "keys", (o("8", "TRI", 100), o("4", "TRI", 100), o("2", "TRI", 60)),
          cutoff=2300, emph=0.5, cntr=0, kbd=100, fenv=(0, 80, 127), env=(4, 90, 127, 60), poly=True,
          lfo=(6.2, "SIN", 0, "OFF"), trem=15, send=(20, 60, 0, 40), rv=("ROOM", 40, 80), ch=("JUNO", 80, 6.2), pat=7)


def atmo2(b):                            # 180-189: textures and drones
    b.add("DEEP WELL", "atmo", (o("16", "SAW", 85), o("16", "SAW", 85, 15), o("8", "TRI", 40)), noise=15, pink=True,
          cutoff=260, emph=3.2, cntr=co(2.0), kbd=40, fenv=(2000, 3000, 70), env=(2000, 2500, 118, 3000), poly=True,
          vib=40, lfo=(0.1, "SIN", 0, "OFF"), send=(0, 70, 50, 90), rv=("CLOUD", 90, 60), dl=("TAPE", 500, 55), pat=5)
    b.add("GHOST CHOIR", "atmo", (o("8", "TRI", 60), o("8", "PW", 40, 9), o("16", "SAW", 25)), noise=20, pink=True,
          cutoff=1100, emph=1.4, cntr=0, kbd=60, fenv=(1500, 2500, 90), env=(2000, 2500, 110, 3000), poly=True,
          vib=60, lfo=(0.2, "TRI", 0, "OFF"), send=(0, 60, 30, 100), rv=("CLOUD", 100, 60), pat=5)
    b.add("PULSE DRONE", "atmo", (o("8", "PW", 90), o("8", "PW", 70, 13), o("16", "SQR", 40)),
          cutoff=500, emph=4.2, cntr=co(1.6), kbd=40, fenv=(1000, 1500, 100), env=(1500, 2000, 118, 2000), poly=True,
          lfo=(0.2, "SIN", 0, "OFF"), mflt=True, send=(10, 60, 0, 90), rv=("HALL", 90, 60), pat=5)
    b.add("OCEAN", "atmo", (osc_off(), osc_off(), osc_off()), noise=90, pink=True,
          cutoff=600, emph=2.0, cntr=0, kbd=0, fenv=(2000, 3500, 80), env=(3000, 3000, 118, 3500),
          lfo=(0.07, "SIN", 0, "OFF"), mflt=True, trem=80,
          send=(0, 40, 0, 110), rv=("CLOUD", 110, 60), pat=7)
    b.add("STARFIELD", "atmo", (o("2", "SAW", 60), o("2", "SAW", 60, 21), o("4", "TRI", 40)), noise=30,
          cutoff=2000, emph=3.6, cntr=co(1.0), kbd=50, fenv=(3000, 4000, 60), env=(3000, 3500, 110, 3000), poly=True,
          vib=80, lfo=(0.3, "SIN", 0, "OFF"), send=(0, 50, 60, 100), rv=("CLOUD", 95, 80), dl=("REVERSE", 500, 40), pat=5)
    b.add("MOTHER", "atmo", (o("16", "TRI", 100), o("16", "TRI", 90, 19), o("8", "SAW", 35)),
          cutoff=350, emph=1.6, cntr=0, kbd=40, fenv=(1500, 2000, 100), env=(2500, 2500, 118, 3000), poly=True,
          vib=40, lfo=(0.12, "SIN", 0, "OFF"), send=(0, 60, 0, 90), rv=("HALL", 95, 60), pat=5)
    b.add("FOG BELLS", "atmo", (o("4", "TRI", 90), o("2", "TRI", 60, 510), osc_off()), noise=40, pink=True,
          cutoff=1800, emph=2.4, cntr=co(0.8), kbd=60, fenv=(800, 2500, 60), env=(1000, 3000, 80, 3500), poly=True,
          lfo=(0.4, "SIN", 0, "OFF"), mflt=True, send=(0, 40, 50, 110), rv=("CLOUD", 100, 70), pat=5)
    b.add("MACHINE HUM", "atmo", (o("16", "SAW", 80), o("16", "SAW", 80, 3), o("32", "SQR", 40)), noise=30,
          cutoff=280, emph=5.0, cntr=co(1.4), kbd=30, fenv=(500, 800, 100), env=(500, 1500, 118, 1200),
          lfo=(0.5, "TRI", 0, "OFF"), trem=40, send=(0, 40, 0, 60), rv=("ROOM", 50, 60), pat=7)
    b.add("VOID", "atmo", (o("16", "SQR", 70), o("16", "SQR", 70, 25), osc_off()), noise=50,
          cutoff=400, emph=6.2, cntr=co(2.6), kbd=30, fenv=(1800, 2400, 90), env=(2000, 3000, 118, 3000),
          mflt=True, lfo=(0.08, "TRI", 0, "OFF"), send=(20, 40, 40, 100), rv=("CLOUD", 100, 60), pat=7)
    b.add("TIDAL", "atmo", (o("8", "PW", 80), o("8", "SAW", 60, 7), o("16", "TRI", 45)), noise=60, pink=True,
          cutoff=720, emph=2.8, cntr=co(1.2), kbd=40, fenv=(2500, 3000, 80), env=(3000, 3000, 118, 3500), poly=True,
          lfo=(0.1, "SIN", 0, "OFF"), mflt=True, send=(0, 50, 0, 100), rv=("CLOUD", 100, 60), pat=5)


def fx2(b):                              # 190-199: effects
    b.add("RISER", "fx", (o("8", "SAW", 90), o("8", "SAW", 90, 12), osc_off()), noise=60,
          cutoff=200, emph=4.0, cntr=co(4.8), kbd=0, fenv=(6000, 6000, 0), env=(5000, 1000, 110, 600), pit=st_pit(12),
          send=(0, 40, 0, 60), rv=("HALL", 80, 80), pat=7)
    b.add("IMPACT", "fx", (o("16", "SQR", 80), osc_off(), osc_off()), noise=100, pink=True,
          cutoff=180, emph=1.5, cntr=co(3.5), kbd=0, fenv=(0, 900, 0), env=(0, 1800, 0, 1600),
          send=(20, 0, 0, 90), rv=("HALL", 110, 40), pat=7)
    b.add("RAYGUN", "fx", (o("4", "SAW", 100), o("2", "SQR", 40, 30), osc_off()),
          cutoff=1100, emph=6.4, cntr=co(4.0), kbd=0, fenv=(0, 300, 0), env=(0, 400, 0, 250), penv=st_pit(-12),
          send=(30, 0, 40, 40), rv=("PLATE", 50, 85), dl=("DIGITAL", 188, 45), pat=7)
    b.add("STATIC", "fx", (osc_off(), osc_off(), osc_off()), noise=127,
          cutoff=900, emph=3.0, cntr=0, kbd=0, fenv=(0, 500, 40), env=(10, 300, 90, 200),
          lfo=(12.0, "S&H", 0, "OFF"), trem=90, send=(0, 0, 0, 60), rv=("ROOM", 40, 70), pat=7)
    b.add("CARPET", "fx", (o("16", "SAW", 70), osc_off(), osc_off()), noise=80, pink=True,
          cutoff=500, emph=5.3, cntr=co(2.8), kbd=0, fenv=(0, 600, 60), env=(200, 800, 90, 500),
          lfo=(0.8, "SIN", 0, "OFF"), mflt=True, send=(0, 40, 0, 60), rv=("HALL", 70, 70), pat=7)
    b.add("SPACESHIP", "fx", (o("8", "SQR", 80), o("8", "SQR", 80, 33), osc_off()),
          cutoff=1200, emph=4.5, cntr=co(2.0), kbd=0, fenv=(0, 200, 90), env=(50, 400, 110, 400),
          lfo=(0.7, "TRI", 0, "OFF"), pit=st_pit(0.5), send=(0, 0, 40, 60), rv=("CLOUD", 80, 80), pat=7)
    b.add("BEEP BOOP", "fx", (o("8", "SQR", 100), osc_off(), osc_off()),
          cutoff=2200, emph=3.0, cntr=0, kbd=0, fenv=(0, 60, 0), env=(0, 80, 0, 40), poly=True,
          arp=A("UP", "1/32", 3, 0.6, 0, 127), arp_on=True, send=(0, 0, 25, 25), rv=("ROOM", 25, 85), pat=13)
    b.add("GLITCH FX", "fx", (o("32", "SHK", 100), osc_off(), osc_off()), noise=90,
          cutoff=900, emph=4.2, cntr=co(3.0), kbd=0, fenv=(0, 80, 0), env=(0, 120, 0, 90),
          arp=A("RND", "1/32", 4, 0.5, 0, 90), arp_on=True, send=(40, 0, 40, 50), rv=("SPRING", 50, 70), pat=13)
    b.add("TAPE STOP", "fx", (o("8", "SAW", 90), o("8", "SAW", 90, 5), osc_off()),
          cutoff=1100, emph=3.3, cntr=co(2.4), kbd=0, fenv=(0, 1200, 0), env=(0, 1500, 0, 900), penv=st_pit(-10),
          send=(20, 0, 40, 60), rv=("ROOM", 55, 80), dl=("TAPE", 450, 60), pat=7)
    b.add("SUB DROP", "fx", (o("16", "SQR", 110), osc_off(), osc_off()),
          cutoff=300, emph=2.0, cntr=co(2.0), kbd=0, fenv=(0, 1500, 0), env=(0, 2000, 0, 1200), penv=st_pit(-24),
          send=(15, 0, 0, 50), rv=("HALL", 80, 60), pat=7)


# the arpeggiator on for these of the second hundred (with the 1.14.0 ones: about a quarter of 200)
ARP2 = ['VELVET EP', 'MINI BASS', 'TIN WHISTLE', 'MACHINE', 'CRUNCH', 'MOONLIGHT', 'DREAM FLUTE', 'ROUND 808', 'RUBBER SUB', 'DOUBLE SAW', 'AURORA', 'GLOW PAD', 'STAB 2', 'SAX BRASS', 'HARP 2', 'KOTO', 'BANJO', 'WURLI 2', 'HARPSI 2', 'STARFIELD', 'FOG BELLS', 'RAYGUN', 'SPACESHIP']


def knob(v):
    """a knob read as 0.0 .. 10.0 on the screen -> its byte 0..127"""
    return min(127, max(0, int(round(v * 127 / 10.0))))


def preset_moves(b):
    """1.14.4: the bank's moves (positions are the presets' numbers; each patch keeps its name, levels and settings)."""
    P = b.p
    def swap(i, j):
        P[i], P[j] = P[j], P[i]
    swap(1, 103)                           # I FEEL ARP <-> MINI BASS
    swap(2, 5)                             # LUSH STRINGS <-> DUB STAB
    P[3] = copy.deepcopy(P[180])           # DEEP WELL copied over ROTARY ORG
    tmp = Bank()                           # a new patch for 180: a longer, darker swell than DEEP WELL
    tmp.add("DEEP SWELL 2", "atmo", (o("16", "SAW", 80), o("16", "SAW", 80, 12), o("8", "TRI", 35)), noise=12, pink=True,
            cutoff=180, emph=2.6, cntr=co(1.6), kbd=40, fenv=(2600, 3400, 60), env=(2600, 2800, 110, 3500), poly=True,
            vib=30, lfo=(0.1, "SIN", 0, "OFF"), send=(0, 60, 60, 100), rv=("CLOUD", 90, 60), dl=("TAPE", 500, 55), pat=5)
    P[180] = tmp.p[0]
    swap(4, 5)                             # LUSH STRINGS <-> CRYSTAL BELL
    swap(5, 7)                             # CRYSTAL BELL <-> SPACE DRONE
    # 1.14.5: the sounds of 004 and 142 trade places; the names stay where they are (only the sound moves)
    a, b_ = P[4], P[142]
    for k in a:
        if k != "name":
            a[k], b_[k] = b_[k], a[k]
    # 1.14.8: 004 LUSH STRINGS: CUTOFF +0.2 on the dial (byte 66), CHORUS mix 10.0 (127), the loudness contour's
    # DECAY 7.2 and SUSTAIN 10.0 (the screen's 0.0-10.0 values as bytes; the decay is not in ms here)
    x = P[4]
    x["e"][0] = round((0.2 + 5.0) * 127 / 10.0)
    x["send"][1] = knob(10.0)
    x["amp"][1] = knob(7.2)
    x["amp"][2] = knob(10.0)
    # 1.14.5: 003 PAD SWELL (was DEEP WELL): CUTOFF -2.1, EMPHASIS 2.6, the filter contour's AMOUNT 7.0 (the dials as the screen shows them)
    x = P[3]
    x["name"] = "PAD SWELL"
    x["e"][0] = round((-2.1 + 5.0) * 127 / 10.0)
    x["e"][1] = emp(2.6)
    x["e"][2] = knob(7.0)
    # 1.14.7: 001 keeps the 1.14.4 sound and is called MOOG BASS (renamed after the level lookup, so its level stays)
    P[1]["name"] = "MOOG BASS"
    P[1]["arp"][0] = 0                     # 1.14.7: 001 MOOG BASS: arpeggiator off, OCT-1 on
    P[1]["oct"] = -1
    # 1.14.14: 001 MOOG BASS: the OSC 3 level 10.0 (127), the reverb MIX 3.6 (the send, 0.0-10.0 on the screen)
    P[1]["g"][10] = knob(10.0)
    P[1]["send"][3] = knob(3.6)
    # 1.14.7: 002 DUB STEP (was DUB STAB): the arpeggiator on with 2 octaves; the reverb HALL (mix 4.0, decay 4.5, tone 5.0);
    # the delay P-PONG (mix 3.0, time 374 ms, feedback 2.0). The knobs are read on the 0.0-10.0 screen scale.
    x = P[2]
    x["name"] = "DUB STEP"
    x["arp"][0] = 1
    x["arp"][3] = 2
    x["fxg"][0], x["fxg"][1], x["fxg"][2] = 1, knob(4.5), knob(5.0)        # reverb TYPE HALL, DECAY, TONE
    x["send"][3] = knob(4.0)                                               # reverb MIX
    x["fxg"][3], x["fxg"][4], x["fxg"][5] = 3, dly(374), knob(2.0)         # delay TYPE P-PONG, TIME, FEEDBK
    x["send"][2] = knob(3.0)                                               # delay MIX
    P[30]["name"] = "MINI BASS"            # the 030 bass (was MOOG BASS): the name is free once 001 moves to MOOG BASS
    # 1.14.6: 006 CS-80 BRASS: MOD OSC off (the switch's bit 32 in the oscillators' switches)
    P[6]["g"][12] &= ~32
    # 1.14.15: 001 MOOG BASS: the drive's GAIN 2.5 (the DIST send) and OUTPUT 7.5 (the screen's 0.0-10.0 values)
    P[1]["send"][0] = knob(2.5)
    P[1]["fxg"][11] = knob(7.5)
    # 1.14.15: 007 MEGA LEAD replaces CRYSTAL BELL: the Minimoog-style gliding lead (the recipe of the MusicRadar tutorial):
    # OSC 1 pulse (PW) at 8', OSC 2 saw at 2' (+3 cents), OSC 3 square at 4' (-3 cents); CUTOFF -2.0 on the dial; CONTOUR 85;
    # the filter envelope attack 600 ms, decay 2 s, sustain 6.0; the loudness attack minimum, decay 5.0, sustain 6.0;
    # keyboard tracking on (64); GLIDE about 12 o'clock (200 ms); a heavy tube drive: GAIN 8.0, OUTPUT 9.0; EMPHASIS 7.0
    tmp = Bank()
    tmp.add("MEGA LEAD", "lead", (o("8", "PW", 110), o("2", "SAW", 90, 3), o("4", "SQR", 90, -3)),
            cutoff=300, emph=7.0, cntr=85, kbd=64, fenv=(600, 2000, 76), env=(5, 600, 76, 150), glide=200,
            send=(80, 0, 0, 0), dr=("TUBE", 60, 90), poly=False)
    P[7] = tmp.p[0]
    P[7]["e"][0] = round((-2.0 + 5.0) * 127 / 10.0)
    # 1.14.17: 002 DUB STEP was far louder than the others (level 127): its level is 104, the bank's usual
    P[2]["level"] = 104
    # 1.14.16: 007 and 022 trade places (names and sounds go with them)
    swap(7, 22)
    # 1.14.16: MOD OSC off on every factory patch (the switch's bit 32 of the oscillators' switches, byte 12)
    for x in P:
        x["g"][12] &= ~32
    # 1.14.22: 004 and 007 trade places (names and sounds go with them)
    swap(4, 7)
    # 1.14.22: 000 MINI LEAD: OSC 2 one octave down (16')
    P[0]["g"][1] = 2
    # 1.14.22: 002 and 008 trade sounds; the names stay in their slots
    a, b_ = P[2], P[8]
    for k in a:
        if k != "name":
            a[k], b_[k] = b_[k], a[k]
    # 1.14.22: 002 is called PLUCK ARP; PLUCK ARP (168) becomes PLUCK ARP 2
    P[2]["name"] = "PLUCK ARP"
    P[168]["name"] = "PLUCK ARP 2"
    # 1.14.22: OCEAN (183) is copied to 008 (the whole patch, its name too)
    P[8] = copy.deepcopy(P[183])
    # 1.14.22: 183 becomes a new atmospheric patch, AURA ATMOS (a placeholder name: rename it when you like)
    tmp = Bank()
    tmp.add("AURA ATMOS", "atmo", (o("16", "SAW", 70), o("8", "TRI", 50, 7), o("16", "PW", 40, -6)), noise=30, pink=True,
            cutoff=600, emph=3.0, cntr=co(2.2), kbd=50, fenv=(1500, 3000, 70), env=(1800, 3000, 110, 3500), poly=True,
            vib=30, lfo=(0.15, "TRI", 0, "OFF"), send=(0, 60, 50, 100), rv=("CLOUD", 95, 60), dl=("TAPE", 500, 50), pat=5)
    P[183] = tmp.p[0]
    # 1.14.23: 008 and 183 trade places (names and sounds together)
    swap(8, 183)
    # 1.14.23: 009 and 174 trade places (names and sounds together)
    swap(9, 174)
    # 1.14.23: DETUNE 2 -0.10 (cents -10, in the cents bytes; the coarse byte is the nearest step) on 010 and 040
    def det_cents(c):
        m = abs(c)
        return m & 127, (m >> 7 & 7) | (8 if c < 0 else 0)
    for i in (10, 40):
        P[i]["g"][6] = 63
        P[i]["g"][18], P[i]["g"][19] = det_cents(-10)
    # 1.14.23: 010 EMPHASIS 3.2
    P[10]["e"][1] = emp(3.2)
    # 1.14.24: 099 UFO is copied over 009 (the whole patch, its name too)
    P[9] = copy.deepcopy(P[99])
    # 1.14.24: 099 becomes a new patch, NEW 099 (a placeholder: a plain saw lead until you say what you want)
    tmp = Bank()
    tmp.add("NEW 099", "lead", (o("8", "SAW", 100), o("8", "SAW", 90, 7), osc_off()), cutoff=1200, emph=3.0,
            cntr=co(1.0), kbd=64, fenv=(10, 400, 60), env=(10, 400, 100, 200), poly=False, glide=0, send=(0, 0, 20, 30),
            rv=("HALL", 60, 70), dl=("ANALOG", 375, 30), pat=0)
    P[99] = tmp.p[0]
    # 1.14.25: 015 ACID LEAD: CUTOFF -2.5, EMPHASIS 8.0, the filter contour's AMOUNT 3.8 (the dials as the screen shows them);
    # the arpeggiator RATE 1/8, OCTAVES 1; the reverb MIX 0; the TEMPO 162 (the patch's: the ARP, the SEQ and the delay share it).
    x = P[15]
    x["e"][0] = round((-2.5 + 5.0) * 127 / 10.0)
    x["e"][1] = emp(8.0)
    x["e"][2] = knob(3.8)
    x["arp"][2] = 1                        # RATE 1/8 (AR in gen_fmini_presets.py)
    x["arp"][3] = 1                        # OCTAVES 1
    x["send"][3] = 0                       # reverb MIX 0
    x["g"][22], x["g"][23] = 162 & 127, 162 >> 7   # TEMPO 162 BPM
    # 1.17.0: 004 (SINE SOLO's name and place): OSC 1 on at 8', triangle, VOLUME 10.0; OSC 2 and OSC 3 off; GLIDE 5.2;
    # CUTOFF -1.9 on the dial; the loudness ATTACK 3.6 and DECAY 5.9; the reverb HALL: MIX 2.5, DECAY 5.4, TONE 7.0;
    # the delay MIX 2.5; the chorus MIX 4.0 (the dials as the screen shows them, 0.0-10.0 -> the bytes)
    x = P[4]
    x["g"][0] = R["8"]                     # OSC 1 range 8'
    x["g"][3] = W["TRI"]                   # OSC 1 wave triangle
    x["g"][8] = knob(10.0)                 # OSC 1 volume
    x["g"][9] = 0                          # OSC 2 volume 0 ...
    x["g"][10] = 0                         # OSC 3 volume 0 ...
    x["g"][12] = (x["g"][12] & ~(2 | 4)) | 1   # ... and their switches off (OSC 1 on; the noise, mod bits as they were)
    x["g"][13] |= 1                        # GLIDE on
    x["g"][17] = knob(5.2)                 # GLIDE time
    x["e"][0] = round((-1.9 + 5.0) * 127 / 10.0)   # CUTOFF -1.9 on the dial
    x["amp"][0] = knob(3.6)                # loudness ATTACK
    x["amp"][1] = knob(5.9)                # loudness DECAY
    x["send"][1] = knob(4.0)               # CHORUS MIX
    x["send"][2] = knob(2.5)               # DELAY MIX
    x["send"][3] = knob(2.5)               # REVERB MIX
    x["fxg"][0] = RT["HALL"]               # REVERB TYPE HALL
    x["fxg"][1] = knob(5.4)                # REVERB DECAY
    x["fxg"][2] = knob(7.0)                # REVERB TONE
    # 1.20.1: 099 is CALM SEA, an effect (the 90-99 group; it was SONAR in 1.20.0): soft sea waves that come and go. White
    # noise through the filter, no EMPHASIS; the LFO (SIN, about 9 s a wave) opens and closes the filter (MOD 100 to the
    # filter, no tremolo), so a wave grows louder and brighter together (the foam's hiss) and between them stays a low rumble
    # (about 14 dB between crest and trough); a slow ATTACK and RELEASE (3.5 s: longer leaves voices busy after 12 s), wide chorus, a long dark HALL. Any key: KBD 0.
    tmp = Bank()
    tmp.add("CALM SEA", "fx", (osc_off(), osc_off(), osc_off()), noise=110, pink=False,
            cutoff=380, emph=0.8, cntr=0, kbd=0, fenv=(0, 100, 127), env=(2500, 1000, 127, 3500),
            vib=100, mflt=True, lfo=(0.11, "SIN", 0, "OFF"), trem=0,
            send=(0, 70, 0, 85), rv=("HALL", 115, 35), ch=("STEREO", 90, 0.15), pat=7)
    P[99] = tmp.p[0]
    # 1.20.1: 084 OCEAN TIDE (sea noise, now CALM SEA's job at 099) becomes NORTHERN SKY, in its group (80-89 atmospheres and
    # drones): a wide, luminous drone in fifths. Two saws at 8' a fifth apart and a triangle at 4' a few cents off (a slow
    # beat); the filter's contour swells it in over 4 s and a very slow LFO lets the brightness breathe; poly (chords),
    # deep JUNO chorus, a long TAPE delay and a bright CLOUD.
    tmp = Bank()
    tmp.add("NORTHERN SKY", "atmo", (o("8", "SAW", 85), o("8", "SAW", 70, 702), o("4", "TRI", 55, 5)),
            cutoff=900, emph=2.6, cntr=co(1.3), kbd=50, fenv=(4000, 4000, 90), env=(2500, 3000, 118, 3500), poly=True,
            vib=40, mflt=True, lfo=(0.13, "TRI", 0, "OFF"),
            send=(0, 90, 45, 105), rv=("CLOUD", 115, 90), dl=("TAPE", 750, 50), ch=("JUNO", 110, 0.25), pat=5)
    P[84] = tmp.p[0]

def build(b):
    showcase(b)
    hard_leads(b)
    soft_leads(b)
    basses(b)
    pads(b)
    brass(b)
    plucks(b)
    keys(b)
    atmospheres(b)
    sfx(b)
    show2(b)
    hard2(b)
    soft2(b)
    bass2(b)
    pad2(b)
    brass2(b)
    pluck2(b)
    keys2(b)
    atmo2(b)
    fx2(b)
    for x in b.p[100:]:
        if x["name"] in ARP2:
            x["arp"][0] = 1
    for x in b.p[130:140]:          # 130-139 (the second hundred's basses): OCT-1, as the first hundred's
        x["oct"] = -1
    preset_moves(b)                 # after the rules above: a patch keeps its own arpeggiator and octave when it moves
    if os.environ.get("FMINI_PARTIAL"):
        while len(b.p) < 200:       # a development filler: the groups still to write
            i = len(b.p)
            b.add("TEMP %02d" % i, "lead", (o("8", "SAW", 100), o("8", "SAW", 100, 11), osc_off()), cutoff=1500, emph=3.0,
                  cntr=co(2), env=(5, 300, 100, 150), poly=(i % 3 == 0), oct=-1 if 50 <= i < 60 or 130 <= i < 140 else 0, arp_on=(i % 4 == 0))
    return b
