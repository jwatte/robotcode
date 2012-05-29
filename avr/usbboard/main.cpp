
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
   V <value>              Local voltage value, ff == 16V, 00 == 8V
   X <len> <text>         Debug text

   Host->Usbboard


 */
extern void setup_timers(unsigned long l);

void blink(bool on) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void debug_text(char const *txt)
{
    int l = strlen(txt);
    if (l > 32) {
        l = 32;
    }
    char buf[2] = { 'X', (char)l };
    uart_send_all(2, buf);
    uart_send_all(l, txt);
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
    if (requestFrom || twi->is_busy()) {
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
    if (requestFrom || twi->is_busy()) {
        d = 1;
    }
    else {
        requestFrom = NodeSensorInput;
        twi->request_from(NodeSensorInput);
    }
    after(d, request_from_sensor, 0);
}

void request_from_usbboard(void *)
{
    char data[3] = { 'D', NodeUSBInterface, 0 };
    uart_send_all(3, data);
    after(1000, request_from_usbboard, 0);
}


unsigned char map_voltage(unsigned char val)
{
    if (val == 255) {
        return 255;
    }
    unsigned short us = (unsigned short)val * 37 / 40;
    return (unsigned char)us;
}

void voltage_cb(unsigned char val)
{
    char data[2] = { 'V', map_voltage(val) };
    uart_send_all(2, data);
}

void poll_voltage(void *)
{
    if (!adc_busy()) {
        adc_read(0, &voltage_cb);
    }
    after(500, &poll_voltage, 0);
}

void setup(void) {
    fatal_set_blink(&blink);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    setup_timers(F_CPU);
    uart_setup(115200, F_CPU);
    uart_send_all(1, "O");
    delay(200);
    digitalWrite(LED_PIN, LOW);
    twi = start_twi_master(&twiMaster);
    adc_setup(false);
    after(0, request_from_motor, 0);
    after(0, request_from_sensor, 0);
    after(0, request_from_usbboard, 0);
    //after(0, request_from_compass_a, 0);
    //after(0, request_from_compass_b, 0);
    after(0, poll_voltage, 0);
}

