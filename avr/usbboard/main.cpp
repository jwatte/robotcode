
#define F_CPU 16000000UL

#include "libavr.h"
#include "pins_avr.h"
#include "cmds.h"
#include <stdio.h>

#define LED_PIN (0|5)

/* UART protocol:

   Each cmd prefixed by sync byte value 0xed
   Usbboard->Host
   O                      On, running
   F <code>               Fatal death, will reboot in 8 seconds
   D <node> <len> <data>  Data polled from node
   N <node>               Nak from node when polling
   R <sensor> <distance>  Distance reading from ranging sensor
   X <len> <text>         Debug text

   Host->Usbboard
   W <node> <len> <data>  Write data to given node
 */

#define SYNC_BYTE ((char)0xed)

info_USBInterface g_info;


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
    char buf[3] = { SYNC_BYTE, 'X', (char)(l & 0xff) };
    uart_send_all(3, buf);
    uart_send_all(l, txt);
}

unsigned char requestFrom;
unsigned char requestNext = (unsigned char)NodeMotorPower;

class MyMaster : public ITWIMaster {
    public:
        virtual void data_from_slave(unsigned char n, void const *data) {
            unsigned char ch[4] = { (unsigned char)SYNC_BYTE, 'D', requestFrom, n };
            uart_send_all(4, ch);
            uart_send_all(n, data);
            requestFrom = 0;
        }
        virtual void nack() {
            unsigned char ch[3] = { (unsigned char)SYNC_BYTE, 'N', requestFrom };
            uart_send_all(3, ch);
            requestFrom = 0;
        }
};
MyMaster twiMaster;
TWIMaster *twi;

unsigned char board_size(unsigned char type) {
    switch (type) {
        case NodeMotorPower: return sizeof(info_MotorPower);
        case NodeSensorInput: return sizeof(info_SensorInput);
        default: return 12;
    }
}

void request_from_boards(void *)
{
    if (requestFrom || twi->is_busy()) {
        after(1, request_from_boards, 0);
        return;
    }
    twi->request_from(requestNext, board_size(requestNext));
    switch (requestNext) {
        case NodeMotorPower:
            requestNext = NodeSensorInput;
            break;
        case NodeSensorInput:
            requestNext = NodeMotorPower;
            break;
        default:
            fatal(FATAL_BAD_PARAM);
            break;
    }
    after(100, request_from_boards, 0);
}

void request_from_usbboard(void *)
{
    char data[4] = { SYNC_BYTE, 'D', NodeUSBInterface, (char)sizeof(g_info) };
    uart_send_all(4, data);
    uart_send_all(sizeof(g_info), &g_info);
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
    g_info.r_voltage = map_voltage(val);
}

void poll_voltage(void *)
{
    if (!adc_busy()) {
        adc_read(0, &voltage_cb);
    }
    after(500, &poll_voltage, 0);
}

unsigned char parse_in_cmd(unsigned char n, char const *buf) {
    for (unsigned char ch = 0; ch < n; ++ch) {
        if (buf[ch] == 'W') {
            if (n - ch >= 3) {
                if (n - ch >= 3 + buf[ch + 2]) {
                    //  got a full cmd
                    twi->send_to(buf[ch + 2], &buf[ch + 3], buf[ch + 1]);
                    return ch + 3 + buf[ch + 2];
                }
            }
            return ch;
        }
    }
    return n;
}

char ser_buf[35];

void poll_serial(void *ptr) {
    unsigned char left = sizeof(ser_buf) - ((char *)ptr - ser_buf);
    unsigned char n = uart_available();
    if (n > left) {
        n = left;
    }
    uart_read(n, &ser_buf[sizeof(ser_buf)-left]);
    left -= n;
    while ((n = parse_in_cmd(left, ser_buf)) > 0) {
        memmove(ser_buf, &ser_buf[n], sizeof(ser_buf)-left-n);
        left += n;
    }
}

void setup(void) {
    fatal_set_blink(&blink);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    setup_timers(F_CPU);
    uart_setup(115200, F_CPU);
    uart_send_all(2, "\xedO");
    delay(200);
    digitalWrite(LED_PIN, LOW);
    twi = start_twi_master(&twiMaster);
    adc_setup(false);
    after(0, request_from_boards, 0);
    after(0, request_from_usbboard, 0);
    after(0, poll_serial, ser_buf);
    //after(0, request_from_compass_a, 0);
    //after(0, request_from_compass_b, 0);
    after(0, poll_voltage, 0);
}

