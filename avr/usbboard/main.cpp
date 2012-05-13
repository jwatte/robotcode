
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

class MyMaster : public ITWIMaster {
public:
  virtual void data_from_slave(unsigned char n, void const *data) {
    uart_send_all(n, data);
  }
  virtual void nack() {
    uart_send_all(5, "nak!\n");
  }
};
MyMaster twiMaster;
TWIMaster *twi;

void setup(void) {
  fatal_set_blink(&blink);
  setup_timers(F_CPU);
  pinMode(LED_PIN, OUTPUT);
  twi = start_twi_master(&twiMaster);
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
  unsigned char cnt = 0;
  while (true) {
    char r = 0;
    if (uart_read(1, &r)) {
      rate = (unsigned char)r * 10;
      uart_send(1, "X");
    }
    else {
      uart_send(1, ".");
    }
    if (!twi->is_busy()) {
      if (!(++cnt & 0x7)) {
        uart_send(1, "r");
        twi->request_from(NodeMotorPower);
      }
      else {
        uart_send(1, "s");
        twi->send_to(2, &rate, NodeMotorPower);
      }
    }
    else {
      uart_send(1, "b");
    }
    digitalWrite(LED_PIN, HIGH);
    delay(rate);
    digitalWrite(LED_PIN, LOW);
    delay(rate);
  }
}

