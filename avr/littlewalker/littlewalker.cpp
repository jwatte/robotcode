
#define F_CPU 16000000

#include "libavr.h"
#include "pins_avr.h"


unsigned char phase;
unsigned char cnt;
short actual[3];
short turn[3];

struct PhaseInfo {
    short turn[3];
    unsigned char cnt;
};

#define REPEAT_INTERVAL 33

#define LIFTSPAN 350
#define TURNSPAN 150

#define LIFTCYCLE 4
#define TURNCYCLE 4

PhaseInfo phases[] = {
    { .turn = { LIFTSPAN, -TURNSPAN, -TURNSPAN, }, .cnt = LIFTCYCLE },
    { .turn = { LIFTSPAN, 0, 0, }, .cnt = TURNCYCLE },
    { .turn = { LIFTSPAN, TURNSPAN, TURNSPAN}, .cnt = TURNCYCLE },
    { .turn = { 0, TURNSPAN, TURNSPAN, }, .cnt = LIFTCYCLE },
    { .turn = { -LIFTSPAN, TURNSPAN, TURNSPAN, }, .cnt = LIFTCYCLE },
    { .turn = { -LIFTSPAN, 0, 0, }, .cnt = TURNCYCLE },
    { .turn = { -LIFTSPAN, -TURNSPAN, -TURNSPAN, }, .cnt = TURNCYCLE },
    { .turn = { 0, -TURNSPAN, -TURNSPAN, }, .cnt = LIFTCYCLE },
};

void do_control(void *) {
    after(REPEAT_INTERVAL, &do_control, 0);
    if (cnt == 0) {
        phase = phase + 1;
        if (phase == sizeof(phases)/sizeof(phases[0])) {
            phase = 0;
        }
        memcpy(turn, phases[phase].turn, sizeof(turn));
        cnt = phases[phase].cnt;
    }
    else {
        --cnt;
    }
    //  Ardiuno D12 is "minimum"
    if (digitalRead(4) == LOW) {
        memset(turn, 0, sizeof(turn));
        turn[0] = -LIFTSPAN;
        turn[1] = -TURNSPAN;
        turn[2] = -TURNSPAN;
    }
    //  Arduino D11 is "maximum"
    else if (digitalRead(3) == LOW) {
        memset(turn, 0, sizeof(turn));
        turn[0] = LIFTSPAN;
        turn[1] = TURNSPAN;
        turn[2] = TURNSPAN;
    }
    for (unsigned char i = 0; i < 3; ++i) {
        actual[i] = actual[i] + (turn[i] - actual[i]) / (cnt + 1);
        IntDisable idi;
        digitalWrite(i + 18, HIGH);
        udelay(1500 + actual[i]);
        digitalWrite(i + 18, LOW);
    }
}


void setup() {
    setup_timers();

    //  servo outputs
    pinMode(18+0, OUTPUT);
    pinMode(18+1, OUTPUT);
    pinMode(18+2, OUTPUT);

    //  end-mode buttons
    pinMode(4, INPUT);
    digitalWrite(4, HIGH);  //  pull-up
    pinMode(3, INPUT);
    digitalWrite(3, HIGH);  //  pull-up

    //  blinky light
    pinMode(5, OUTPUT);

    after(20, &do_control, 0);

    digitalWrite(5, LOW);
}

