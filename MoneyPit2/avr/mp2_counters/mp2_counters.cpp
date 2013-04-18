#define F_CPU 20000000
#include <libavr.h>
#include <pins_avr.h>


//  port B
#define B_BLINK (1<<0)
#define B_SLIPPED (1<<1)

unsigned short counters[4] = { 0 };

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
    0,  //  0   0
    -1, //  1   0
    2,  //  2   0
    1,  //  3   0
    1,  //  0   1
    0,  //  1   1
    -1, //  2   1
    2,  //  3   1
    2,  //  0   2
    1,  //  1   2
    0,  //  2   2
    -1, //  3   2
    -1, //  0   3
    2,  //  1   3
    1,  //  2   3
    0,  //  3   3

};

void read_counters(void *) {

    after(0, read_counters, 0);

    unsigned char d = PIND;

    unsigned char c0 = ((d & 0x03) >> 2) | old0;
    short d0 = delta[c0];
    if (d0 == 2) {
        PORTB |= B_SLIPPED;
    }
    else {
        counters[0] += d0;
    }
    old0 = (c0 >> 2);

    unsigned char c1 = (d & 0x0c) | old1;
    short d1 = delta[c1];
    if (d1 == 2) {
        PORTB |= B_SLIPPED;
    }
    else {
        counters[1] += d1;
    }
    old1 = (c1 >> 2);

    unsigned char c2 = ((d & 0x30) << 2) | old2;
    short d2 = delta[c2];
    if (d2 == 2) {
        PORTB |= B_SLIPPED;
    }
    else {
        counters[2] += d2;
    }
    old2 = (c2 >> 2);

    unsigned char c3 = ((d & 0xc0) << 4) | old3;
    short d3 = delta[c3];
    if (d3 == 2) {
        PORTB |= B_SLIPPED;
    }
    else {
        counters[3] += d3;
    }
    old3 = (c3 >> 2);
}

void onboot(void*) {
    PORTB &= ~B_BLINK;
    after(0, read_counters, 0);
    after(800, idle, (void *)1);
}

class ImSlave : public ITWISlave {
    public:
        virtual void data_from_master(unsigned char n, void const *data) {
        }
        virtual void request_from_master(void *o_buf, unsigned char &o_size) {
            o_size = 8;
            memcpy(o_buf, counters, 8);
            PORTB |= B_BLINK;
        }
};

ImSlave slave;



void setup() {
    PORTD = 0;
    DDRD = 0;

    PORTB |= B_BLINK | B_SLIPPED;
    DDRB |= B_BLINK | B_SLIPPED;

    fatal_set_blink(&fatal_blink_func);

    setup_timers(F_CPU);

    start_twi_slave(&slave, 0x02);

    after(400, onboot, 0);
}

