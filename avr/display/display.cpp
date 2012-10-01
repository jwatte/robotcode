
#define F_CPU 16000000

#include <libavr.h>
#include <lcd.h>

void floop(void *) {
    LCD::clear(0xffff);
    LCD::clear(0x07ff);
    LCD::clear(0xf81f);
    LCD::clear(0xffe0);
    LCD::clear(0xf800);
    LCD::clear(0x07e0);
    LCD::clear(0x001f);
    LCD::clear();
}

void setup() {
    setup_timers(F_CPU);
    LCD::init();
    after(1000, floop, 0);
}

