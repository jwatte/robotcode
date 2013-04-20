#define F_CPU 20000000
#include <libavr.h>
#include <pins_avr.h>


#define TWI_ADDR_COUNTERS 0x02

//  port B
#define B_BLINK (1<<0)
#define B_SLIPPED (1<<1)

unsigned short counters[4] = { 0 };
unsigned short last_twi = 0;


/*
void debug_out(unsigned char b) {
    while (!(UCSR0A & (1 << UDRE0))) {
        //  wait
    }
    UDR0 = b;
}
*/

void fatal_blink_func(bool on)
{
    DDRB |= B_BLINK | B_SLIPPED;
    if (on) {
        PORTB |= B_BLINK | B_SLIPPED;
    }
    else {
        PORTB &= ~(B_BLINK | B_SLIPPED);
    }
}

void idle(void *p) {
    if (p) {
        PORTB |= B_BLINK;
        PORTB &= ~B_SLIPPED;
        after(100, idle, 0);
    }
    else {
        PORTB &= ~B_BLINK;
        after(1900, idle, (void *)1);
    }
}

unsigned char old0 = 0;
unsigned char old1 = 0;
unsigned char old2 = 0;
unsigned char old3 = 0;

//  Note: if I move two ticks in one check, I don't know which 
//  direction I was moving in, so I don't update at all. An 
//  option would be to remember which the previous direction 
//  was, and move 2 in that. However, I'm likely to see only 
//  8 ticks per millisecond at most, so I'll likely not slip 
//  at all.
static const char delta[16] = {
        //  new is high bits
        //  old new
     0, //  00   00
    -1, //  01   00
     1, //  10   00
     2, //  11   00
     1, //  00   01
     0, //  01   01
     2, //  10   01
    -1, //  11   01
    -1, //  00   10
     2, //  01   10
     0, //  10   10
     1, //  11   10
     2, //  00   11
     1, //  01   11
    -1, //  10   11
     0, //  11   11

};

void read_counters(void *) {

    PORTB &= ~(B_SLIPPED);

    after(0, read_counters, 0);

    unsigned char d = PIND;

    unsigned char c0 = ((d & 0x03) << 2) | old0;
    char d0 = delta[c0];
    if (d0 == 2) {
        PORTB |= B_SLIPPED;
    }
    else {
        counters[0] += d0;
    }
    old0 = (c0 >> 2);

    unsigned char c1 = (d & 0x0c) | old1;
    char d1 = delta[c1];
    if (d1 == 2) {
        PORTB |= B_SLIPPED;
    }
    else {
        counters[1] += d1;
    }
    old1 = (c1 >> 2);

    unsigned char c2 = ((d & 0x30) >> 2) | old2;
    char d2 = delta[c2];
    if (d2 == 2) {
        PORTB |= B_SLIPPED;
    }
    else {
        counters[2] += d2;
    }
    old2 = (c2 >> 2);

    unsigned char c3 = ((d & 0xc0) >> 4) | old3;
    char d3 = delta[c3];
    if (d3 == 2) {
        PORTB |= B_SLIPPED;
    }
    else {
        counters[3] += d3;
    }
    old3 = (c3 >> 2);
}

void check_twi(void *);

void onboot(void*) {
    PORTB &= ~B_BLINK;
    after(0, read_counters, 0);
    after(800, idle, (void *)1);
    after(100, check_twi, 0);
    last_twi = read_timer();
}

class ImSlave : public ITWISlave {
    public:
        virtual void data_from_master(unsigned char n, void const *data) {
        }
        virtual void request_from_master(void *o_buf, unsigned char &o_size) {
            o_size = 8;
            memcpy(o_buf, counters, 8);
            PORTB |= B_BLINK;
            last_twi = read_timer();
        }
};

ImSlave slave;

void rapid_flash(void *p) {
    int i = (int)p;
    if (i & 1) {
        PORTB |= B_BLINK;
    }
    else {
        PORTB &= ~B_BLINK;
    }
    if (i) {
        after(50, rapid_flash, (void *)(i - 1));
    }
}

void check_twi(void *) {
    unsigned short now = read_timer();
    if (now - last_twi > 1000) {
        stop_twi();
        delay(2);
        start_twi_slave(&slave, 0x02);
        last_twi = read_timer();
        after(50, rapid_flash, (void *)10);
    }
    after(100, check_twi, 0);
}



void setup() {
    PORTD = 0;
    DDRD = 0;

    PORTB |= B_BLINK | B_SLIPPED;
    DDRB |= B_BLINK | B_SLIPPED;

    fatal_set_blink(&fatal_blink_func);

    setup_timers(F_CPU);

    start_twi_slave(&slave, TWI_ADDR_COUNTERS);

/*
    //  setup debug uart without interrupts
    power_usart0_enable();
    UCSR0A = (1 << U2X0);  //  2X speed
    UBRR0H = 0;
    UBRR0L = 1; //  1.25 Mbit -- just dump what's there!
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);  //  tx enable
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); //  8 bits, N, 1

    debug_out(0);
    debug_out(0);
    debug_out(0xff);
    debug_out(0xff);
 */

    after(400, onboot, 0);
}

