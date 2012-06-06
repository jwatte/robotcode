#define F_CPU 12000000
#include <libavr.h>
#include <pins_avr.h>

#define BLINK_PIN (16|5)

void blink(void *p)
{
    digitalWrite(BLINK_PIN, p ? HIGH : LOW);
    after(200, blink, p ? NULL : (void *)1);
}

void setup() {
    setup_timers(F_CPU);
    pinMode(BLINK_PIN, OUTPUT);
    after(200, blink, 0);
}
