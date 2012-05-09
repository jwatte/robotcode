
#define F_CPU 16000000UL

#include "libavr.h"
#include "pins_avr.h"
#include "cmds.h"
#include <stdio.h>

#define LED_PIN (0|5)

extern void setup_timers(unsigned long l);

void blink(bool on) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void serial_send_packet(uint8_t len, void const *data)
{
  uart_send(1, &len);
  uart_send(len, data);
}

void setup(void) {
  fatal_set_blink(&blink);
  setup_timers(F_CPU);
  pinMode(LED_PIN, OUTPUT);
  uart_setup(115200, 16000000);
  uart_send_all(15, "Hello, world!\n");
  {
    cmd_parameter_value cpv;
    cpv.cmd = CMD_PARAMETER_VALUE;
    cpv.fromNode = NodeUSBInterface;
    cpv.toNode = NodeAny;
    cpv.parameter = ParamEEDump;
    cpv.type = TypeRaw;
    cpv.value[0] = 24;
    eeprom_read_block(&cpv.value[1], (void const *)0, 24);
    serial_send_packet(sizeof(cpv), &cpv);
  }
  unsigned int rate = 1000;
  while (true) {
    char r = 0;
    if (uart_read(1, &r)) {
      rate = (unsigned char)r * 10;
    }
    else {
      uart_send(1, ".");
    }
    digitalWrite(LED_PIN, HIGH);
    delay(rate);
    digitalWrite(LED_PIN, LOW);
    delay(rate);
  }
}

