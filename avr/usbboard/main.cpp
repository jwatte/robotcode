
#define F_CPU 16000000UL

#include "libavr.h"
#include "pins_avr.h"
#include <stdio.h>


#define LED_PIN (0|0) //  port b, pin 0

volatile unsigned long ul;

void toggle_led(void *ptr) {
  after(1000, &toggle_led, ptr ? 0 : (void *)1);
  digitalWrite(LED_PIN, ptr ? LOW : HIGH);
}

extern void setup_timers(unsigned long l);

void blink(bool on) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void setup(void) {
  fatal_set_blink(&blink);
  setup_timers(F_CPU);
  pinMode(LED_PIN, OUTPUT);
  toggle_led((void *)1);
  uart_setup(115200, F_CPU);
  uart_send_all(14, "Hello, world!\n");
}

