
#define F_CPU 16000000UL

#include "libavr.h"
#include "pins_avr.h"
#include "cmds.h"
#include <stdio.h>

#define LED_PIN (0|5)

/* UART protocol:

 Usbboard->Host
 O                      On, running
 F <code>               Fatal death, will reboot in 8 seconds
 D <node> <len> <data>  Data polled from node
 N <node>               Nak from node when polling
 R <sensor> <distance>  Distance reading from ranging sensor

 Host->Usbboard


 */
extern void setup_timers(unsigned long l);

void blink(bool on) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

unsigned char requestFrom;

class MyMaster : public ITWIMaster {
public:
  virtual void data_from_slave(unsigned char n, void const *data) {
    unsigned char ch[3] = { 'D', requestFrom, n };
    uart_send_all(3, ch);
    uart_send_all(n, data);
    requestFrom = 0;
  }
  virtual void nack() {
    unsigned char ch[2] = { 'N', requestFrom };
    uart_send_all(2, ch);
    requestFrom = 0;
  }
};
MyMaster twiMaster;
TWIMaster *twi;

void request_from_motor(void *)
{
  unsigned int d = 500;
  if (twi->is_busy()) {
    d = 1;
  }
  else {
    requestFrom = NodeMotorPower;
    twi->request_from(NodeMotorPower);
  }
  after(d, request_from_motor, 0);
}

void request_from_sensor(void *)
{
  unsigned int d = 100;
  if (twi->is_busy()) {
    d = 1;
  }
  else {
    requestFrom = NodeSensorInput;
    twi->request_from(NodeSensorInput);
  }
  after(d, request_from_sensor, 0);
}

void setup(void) {
  fatal_set_blink(&blink);
  setup_timers(F_CPU);
  pinMode(LED_PIN, OUTPUT);
  twi = start_twi_master(&twiMaster);
  adc_setup();
  uart_setup(115200, F_CPU);
  uart_send_all(1, "O");
  after(0, request_from_motor, 0);
  after(0, request_from_sensor, 0);
}

