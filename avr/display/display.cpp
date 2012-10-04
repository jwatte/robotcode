
#define F_CPU 16000000

#include <libavr.h>
#include <lcd.h>

extern unsigned char const Droid_Sans_10_ascii_data[] PROGMEM;
Font Droid_10(Droid_Sans_10_ascii_data);

bool lit = false;
unsigned char offset = 0;
unsigned short colors[] = {
    0xffff,
    0x07ff,
    0xf81f,
    0xffe0,
    0xf800,
    0x07e0,
    0x001f,
    0x0000
};

void floop(void *) {
    if (lit) {
        PORTB |= (1 << 5);
    }
    else {
        PORTB &= ~(1 << 5);
    }
    lit = !lit;

    LCD::clear(colors[offset]);
    offset += 1;
    if (offset >= sizeof(colors)/sizeof(colors[0])) {
        offset = 0;
    }

    LCD::text(5, 5, 0xffff, 0x0000, "floop", 5, Droid_10);

    after(500, floop, 0);
}

void setup() {
    setup_timers(F_CPU);
    //  5 is blinky light
    //  3 is reset
    DDRB |= (1 << 5) + (1 << 3);
    PORTB |= (1 << 5) + (1 << 3);
    udelay(100);
    PORTB &= ~(1 << 3);
    udelay(100);
    PORTB |= (1 << 3);
    LCD::init();
    after(500, floop, 0);
    PORTB &= ~(1 << 5);
}

