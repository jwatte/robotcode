
#include <alsa/asoundlib.h>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include <iostream>


//  The higher the factor, the more short bits
#define SPLIT_FACTOR 0.68

//  How likely are we to switch rather than mutate for each bit?
#define SWITCH_VOICE 0.1
#define SWITCH_FREQ 0.2
#define SWITCH_FMOD 0.2
#define SWITCH_FMODPHASE 0.3
#define SWITCH_ACURVE 0.1

struct mood_tonal {
    float freq_scale;
    float freq_add;
    float mod_scale;
    float mod_add;
    float mp_scale;
    float mp_add;
    float acurve_scale;
    float acurve_add;
};

struct mood_rythmic {
    float split_factor;
    float switch_voice;
    float switch_freq;
    float switch_fmod;
    float switch_fmodphase;
    float switch_acurve;
    float triplets;
};

struct mood {
    struct mood_tonal tonal;
    struct mood_rythmic rythmic;
};

mood happy = {
    .tonal = {
        .freq_scale = 70.0,
        .freq_add = 400.0,
        .mod_scale = 0.5,
        .mod_add = 0.2,
        .mp_scale = 4.0,
        .mp_add = 0.5,
        .acurve_scale = 5.0,
        .acurve_add = 2.0
    },
    .rythmic = {
        .split_factor = 0.8,
        .switch_voice = 0.5,
        .switch_freq = 0.5,
        .switch_fmod = 0.5,
        .switch_fmodphase = 0.5,
        .switch_acurve = 0.5,
        .triplets = 0.5
    }
};
mood sad = {
    .tonal = {
        .freq_scale = 20.0,
        .freq_add = 300.0,
        .mod_scale = 0.2,
        .mod_add = 0.1,
        .mp_scale = 0.4,
        .mp_add = 0.1,
        .acurve_scale = 2.0,
        .acurve_add = 1.0
    },
    .rythmic = {
        .split_factor = 0.6,
        .switch_voice = 0.1,
        .switch_freq = 0.1,
        .switch_fmod = 0.1,
        .switch_fmodphase = 0.1,
        .switch_acurve = 0.1,
        .triplets = 0.2
    }
};
mood neutral = {
    .tonal = {
        .freq_scale = 50.0,
        .freq_add = 350.0,
        .mod_scale = 0.75,
        .mod_add = 0.25,
        .mp_scale = 0.75,
        .mp_add = 0.2,
        .acurve_scale = 3.0,
        .acurve_add = 1.2
    },
    .rythmic = {
        .split_factor = 0.7,
        .switch_voice = 0.2,
        .switch_freq = 0.2,
        .switch_fmod = 0.2,
        .switch_fmodphase = 0.2,
        .switch_acurve = 0.2,
        .triplets = 0.35
    }
};

struct section_info {
    short *buf;
    int count;
    int voice;
    float freq;
    float fmod;
    float fmodphase;
    float acurve;
    struct mood const *mood;
};

void initrand(section_info &si, mood const &m) {
    si.voice = rand() * 4.0 / RAND_MAX;
    si.freq = rand() * m.tonal.freq_scale / (float)RAND_MAX;
    si.freq = si.freq * si.freq + m.tonal.freq_add;
    si.fmod = rand() * m.tonal.mod_scale / (float)RAND_MAX + m.tonal.mod_add;
    si.fmodphase = rand() / (float)RAND_MAX;
    si.fmodphase = si.fmodphase * si.fmodphase * m.tonal.mp_scale + m.tonal.mp_add;
    si.acurve = rand() * m.tonal.acurve_scale  / (float)RAND_MAX + m.tonal.acurve_add;
    switch ((int)(rand() * 3.0 / RAND_MAX)) {
        case 0: si.mood = &happy; break;
        case 1: si.mood = &sad; break;
        default: si.mood = &neutral; break;
    }
}

float voicedata[4][1024];

void init_voicedata() {
    //  generate
    for (int v = 0; v < 4; ++v) {
        int no = 1 + v * 4;
        for (int i = 0; i < 1024; ++i) {
            float m = 0;
            for (int o = 0; o < no; ++o) {
                m += sinf(i * 2 * M_PI * (o + 1) / 1024) / (o * o + no);
            }
            voicedata[v][i] = m;
        }
    }
    //  normalize
    for (int v = 0; v < 4; ++v) {
        float m = 0;
        for (int i = 0; i < 1024; ++i) {
            if (fabsf(voicedata[v][i]) > m) {
                m = fabsf(voicedata[v][i]);
            }
        }
        m = 0.9999f / m;
        for (int i = 0; i < 1024; ++i) {
            voicedata[v][i] *= m;
        }
    }
}

float voice(int v, float phase) {
    double dp = phase * 1024 / (2 * M_PI);
    double ipart;
    double fpart = (int)modf(dp, &ipart);
    int i = ((int)ipart) & 1023;
    return voicedata[v][i] * (1 - fpart) +
        voicedata[v][(i + 1) & 1023] * fpart;
}

void mutate(section_info &si, mood const &m) {

    section_info rsi;
    initrand(rsi, m);

    if (rand() / (float)RAND_MAX < m.rythmic.switch_voice) {
        si.voice = rsi.voice;
    }

    if (rand() / (float)RAND_MAX < m.rythmic.switch_freq) {
        si.freq = rsi.freq;
    }
    else {
        si.freq = si.freq * (0.75 + rand() * 0.5 / RAND_MAX);
    }

    if (rand() / (float)RAND_MAX < m.rythmic.switch_fmod) {
        si.fmod = rsi.fmod;
    }
    else {
        si.fmod = si.fmod * (0.75 + rand() * 0.5 / RAND_MAX);
    }

    if (rand() / (float)RAND_MAX < m.rythmic.switch_fmodphase) {
        si.fmodphase = rsi.fmodphase;
    }
    else {
        si.fmodphase = si.fmodphase * (0.75 + rand() * 0.5 / RAND_MAX);
    }

    if (rand() / (float)RAND_MAX < m.rythmic.switch_acurve) {
        si.acurve = rsi.acurve;
    }
    else {
        si.acurve = si.acurve * (0.75 + rand() * 0.5 / RAND_MAX);
    }
}

void generate_section(section_info const &si) {
    if ((rand() & 0x7fff) < si.count * si.mood->rythmic.split_factor) {
        if (rand() * 1.0 / RAND_MAX < si.mood->rythmic.triplets) {
            //  triplets
            section_info s1 = si;
            section_info s2 = si;
            section_info s3 = si;
            s1.count = si.count / 3;
            s2.buf += s1.count;
            s2.count = si.count / 3;
            s3.buf += s1.count + s2.count;
            s3.count = si.count - s1.count - s2.count;
            mutate(s1, *si.mood);
            mutate(s2, *si.mood);
            mutate(s3, *si.mood);
            generate_section(s1);
            generate_section(s2);
            generate_section(s3);
        }
        else {
            //  two
            section_info s1 = si;
            section_info s2 = si;
            s1.count = si.count / 2;
            s2.buf += s1.count;
            s2.count = si.count - s1.count;
            mutate(s1, *si.mood);
            mutate(s2, *si.mood);
            generate_section(s1);
            generate_section(s2);
        }
    }
    else {
        float ph = 0;
        float dph = (float)(2 * M_PI * si.freq / 44100.0);
        float dphi = (float)(2 * M_PI * si.freq / 44100.0);
        float mph = si.fmodphase;
        float mdph = (float)(si.fmodphase * 2 * M_PI / si.count);
        float fmod = si.fmod;
        float ac = si.acurve;
        int n = si.count, n2 = n / 2;
        float ltop = n2 * n2;
        short *ob = si.buf;
        int vc = si.voice;
        for (int i = 0; i < n; ++i) {
            float v = voice(vc, ph);
            dph = dphi + sinf(mph) * fmod * dphi;
            ph = ph + dph;
            if (ph > M_PI) {
                ph -= 2 * M_PI;
            }
            mph = mph + mdph;
            if (mph > M_PI) {
                mph -= 2 * M_PI;
            }
            float l = (ltop - (i - n2) * (i - n2)) / ltop * ac;
            if (l > 1) l = 1;
            if (l < 0) l = 0;
            ob[i] = (short)(32767 * v * l);
        }
    }
}

void generate(short *buf, int count, mood const &m) {
    section_info si;
    si.buf = buf;
    si.count = count;
    si.voice = 0;
    si.freq = 400;
    si.fmod = .1;
    si.fmodphase = 0.8;
    si.acurve = 2.5;

    init_voicedata();
    initrand(si, m);
    si.mood = &m;
    generate_section(si);
}

int main(int argc, char const *argv[]) {

    int err;
    snd_pcm_t *devh;
    snd_pcm_hw_params_t *hwp;
    short bufs[65536 + 8192];
    short *buf = &bufs[4096];
    int count = 65536;
    memset(bufs, 0, sizeof(bufs));
    mood const *imood = &neutral;

    time_t t;
    time(&t);
    srand(t);
    if (argv[1]) {
        if (!strcmp(argv[1], "happy")) {
            imood = &happy;
        }
        else if (!strcmp(argv[1], "sad")) {
            imood = &sad;
        }
        else if (!strcmp(argv[1], "neutral")) {
            imood = &neutral;
        }
        else if (!strcmp(argv[1], "random")) {
            switch ((int)(rand() * 3.0 / RAND_MAX)) {
                case 0: imood = &happy; break;
                case 1: imood = &sad; break;
                default: imood = &neutral; break;
            }
        }
        else {
            fprintf(stderr, "usage: boop [happy|sad|neutral|random [hw:#,#]]\n");
            exit(1);
        }
        ++argv;
        --argc;
    }
    generate(buf, 65536, *imood);
    for (int i = 0; i < 100; ++i) {
        buf[i] = buf[i] * i / 100;
        buf[65535-i] = buf[65535-i] * i / 100;
    }

    char const *devname = (argv[1] ? argv[1] : "plughw:0,0");
    if ((err = snd_pcm_open(&devh, devname, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        std::cerr << "snd_pcm_open(" << devname << ") failed: " << snd_strerror(err) << std::endl;
        exit(1);
    }

    if ((err = snd_pcm_hw_params_malloc(&hwp)) < 0) {
        std::cerr << "snd_pcm_hw_params_malloc() failed: " << snd_strerror(err) <<std::endl;
        exit(1);
    }
    if ((err = snd_pcm_hw_params_any(devh, hwp)) < 0) {
        std::cerr << "snd_pcm_hw_params_any() failed: " << snd_strerror(err) <<std::endl;
        exit(1);
    }
    if ((err = snd_pcm_hw_params_set_access(devh, hwp, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        std::cerr << "snd_pcm_hw_params_set_access() failed: " << snd_strerror(err) <<std::endl;
        exit(1);
    }
    unsigned int rate = 44100;
    int dir = -1;
    if ((err = snd_pcm_hw_params_set_rate_near(devh, hwp, &rate, &dir)) < 0) {
        std::cerr << "snd_pcm_hw_params_set_rate_near(44100) failed: " << snd_strerror(err) <<std::endl;
        exit(1);
    }
    if ((err = snd_pcm_hw_params_set_channels(devh, hwp, 1)) < 0) {
        std::cerr << "snd_pcm_hw_params_set_channels(1) failed: " << snd_strerror(err) <<std::endl;
        exit(1);
    }
    if ((err = snd_pcm_hw_params_set_format(devh, hwp, SND_PCM_FORMAT_S16_LE)) < 0) {
        std::cerr << "snd_pcm_hw_params_set_format() failed: " << snd_strerror(err) <<std::endl;
        exit(1);
    }
    if ((err = snd_pcm_hw_params_set_periods_integer(devh, hwp)) < 0) {
        std::cerr << "snd_pcm_hw_params_set_integer() failed: " << snd_strerror(err) <<std::endl;
        exit(1);
    }
    if ((err = snd_pcm_hw_params_set_buffer_size(devh, hwp, 4096)) < 0) {
        std::cerr << "snd_pcm_hw_params_set_buffer_size(4096) failed: " << snd_strerror(err) <<std::endl;
        exit(1);
    }
    if ((err = snd_pcm_hw_params(devh, hwp)) < 0) {
        std::cerr << "snd_pcm_hw_params() failed: " << snd_strerror(err) <<std::endl;
        exit(1);
    }
    snd_pcm_hw_params_free(hwp);

    if ((err = snd_pcm_prepare(devh)) < 0) {
        perror("prepare() failed");
        exit(1);
    }

    for (int i = 0; i < count + 8192;) {
        int tw = 4096;
        if (tw > count + 8192 - i) {
            tw = count + 8192 - i;
        }
        if ((err = snd_pcm_writei(devh, &bufs[i], tw)) < 0) {
            std::cerr << "snd_pcm_writei failed: " << snd_strerror(err) <<std::endl;
            exit(1);
        }
        i += tw;
    }
   
    snd_pcm_drain(devh);
    snd_pcm_close(devh);

    exit(0);
}

