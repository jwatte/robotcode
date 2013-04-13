#define F_CPU 8000000

#include <libavr.h>
#include <pins_avr.h>


#define BAUDRATE 57600
#define DIR_PORT PINC
#define STEP_PORT PINB

#define LED_YELLOW 0x80
#define LED_GREEN 0x40
#define LED_ALL 0xC0

#define DEBUG_PIN 0x10
#define IOBIT_MASK 0x0C


void blink(bool on) {
    DDRD |= LED_ALL;
    if (on) {
        PORTD |= LED_ALL;
    }
    else {
        PORTD &= ~LED_ALL;
    }
}

unsigned short counts[6];
unsigned char old_step = 0x00;
unsigned short iobits = 0;
unsigned short oldbits = 0;

unsigned char buf[32];
unsigned char buf_ptr = 0;
unsigned short led_timeout;

void handle_packet() {
    if (buf[1] == 1 && buf[2] == 'a') {
        led_timeout = read_timer_fast() + 120;
        PORTD |= LED_YELLOW;
    }
}

bool debugpin = false;

//  PORTC is step signals
void read_pinstate(void *) {
    if (debugpin) {
        PORTD &= ~DEBUG_PIN;
        debugpin = false;
    }
    else {
        PORTD |= DEBUG_PIN;
        debugpin = true;
    }
    after(0, read_pinstate, 0);
    unsigned char dir = DIR_PORT; //  step
    unsigned char step = STEP_PORT; //  direction
    unsigned char mask = 1;
    iobits |= (PIND & IOBIT_MASK);
    for (unsigned char bit = 0; bit < 6; ++bit) {
        //  if this pin transitioned from low to high
        if ((step & mask) && !(old_step & mask)) {
            PORTD &= ~LED_GREEN;
            if (dir & mask) {
                counts[bit]++;
            }
            else {
                counts[bit]--;
            }
        }
        mask = mask << 1;
    }
    old_step = step;
    if (uart_available()) {
        unsigned char ch = uart_getch();
        if (buf_ptr == 0) {
            if (ch != 0xed) {
                //  not a good packet
            }
            else {
                buf[0] = ch;
                buf_ptr = 1;
            }
        }
        else if (buf_ptr == 1) {
            if (ch > 30) {
                //  bad packet length
                buf_ptr = 0;
            }
            else {
                buf[1] = ch;
                buf_ptr = 2;
            }
        }
        else {
            buf[buf_ptr++] = ch;
            if (buf_ptr >= buf[1] + 2) {
                handle_packet();
                buf_ptr = 0;
            }
        }
    }
}

unsigned short calc_cksum(unsigned short *val, unsigned char cnt) {
    unsigned short ret = 0xbad1;
    while (cnt > 0) {
        //  overflow arithmetic is fun!
        ret = (ret + *val) * 0x6967 + 0x1551;
        --cnt;
        ++val;
    }
    return ret;
}


void send_update(void *p) {
    after(50, send_update, p);
    if (iobits == oldbits) {
        //  also clears "got a pulse" light
        PORTD |= LED_GREEN;
    }
    else {
        oldbits = iobits;
        //  turns on light
        PORTD &= ~LED_GREEN;
    }
    short diff = (short)(read_timer_fast() - led_timeout);
    if (diff > 0) {
        //  turn off yellow "ack" led
        PORTD &= ~LED_YELLOW;
    }
    unsigned char sbuf[18] = { 0xed, 16 };
    memcpy(&sbuf[2], counts, 12);
    memcpy(&sbuf[14], &iobits, 2);
    iobits = 0;
    unsigned short cksum = calc_cksum((unsigned short *)sbuf, 8);
    memcpy(&sbuf[16], &cksum, 2);
    uart_send_all(18, sbuf);
}

unsigned char reset_packet[3] = {
    0xed,
    1,
    'r'
};

void setup() {
    fatal_set_blink(blink);
    DDRD = LED_ALL | DEBUG_PIN;
    //  pull-ups on iobits
    PORTD |= LED_ALL | DEBUG_PIN | IOBIT_MASK;
    setup_timers(F_CPU);
    uart_setup(BAUDRATE);
    power_adc_enable();
    DDRB &= ~0x3f;  //  direction, input
    DDRC &= ~0x3f;  //  step, input
    PORTC |= 0x3f;  //  pull-up
    PORTB |= 0x3f;  //  pull-up
    MCUCR &= ~(1 << PUD);
    after(50, send_update, 0);
    after(0, read_pinstate, 0);
    uart_send_all(3, reset_packet);
}

