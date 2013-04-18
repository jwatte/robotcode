#define F_CPU 20000000
#include <libavr.h>
#include <pins_avr.h>


//  port B
#define B_BLINK (1<<0)
#define B_POSITIONING (1<<1)

#define D_SERVO_0 0x80
#define D_SERVO_1 0x40
#define D_SERVO_2 0x20
#define D_SERVO_3 0x10

//  how many 30-millisecond intervals to time out driving the servos?
#define RECV_POSCOUNT 40

//  this is purposefully not right
unsigned short positions[4] = { 2000, 1000, 1250, 1750 };
unsigned char poscount = 0;
extern unsigned long actual_f_cpu_1000;

void fatal_blink_func(bool on)
{
    DDRB |= B_BLINK | B_POSITIONING;
    if (on) {
        PORTB |= B_BLINK | B_POSITIONING;
    }
    else {
        PORTB &= ~(B_BLINK | B_POSITIONING);
    }
}

void idle(void *p) {
    if (p) {
        PORTB |= B_BLINK;
        after(100, idle, 0);
    }
    else {
        PORTB &= ~B_BLINK;
        after(1900, idle, (void *)1);
    }
}

struct {
    unsigned short time;
    unsigned char value;
}
table[5] = {};

void set_servos(void *) {
    after(30, set_servos, 0);
    if (poscount != 0) {
        poscount--;
        PORTB |= B_POSITIONING;

        table[0].time = (unsigned long)positions[0] * actual_f_cpu_1000 / 8000;
        table[0].value = D_SERVO_0;
        table[1].time = (unsigned long)positions[1] * actual_f_cpu_1000 / 8000;
        table[1].value = D_SERVO_1;
        table[2].time = (unsigned long)positions[2] * actual_f_cpu_1000 / 8000;
        table[2].value = D_SERVO_2;
        table[3].time = (unsigned long)positions[3] * actual_f_cpu_1000 / 8000;
        table[3].value = D_SERVO_3;

        for (unsigned char a = 0; a < 3; ++a) {
            unsigned char it = a;
            unsigned short ittime = table[a].time;
            for (unsigned char b = a+1; b < 4; ++b) {
                if (table[b].time < ittime) {
                    ittime = table[b].time;
                    it = b;
                }
            }
            if (it != a) {
                unsigned char x = table[it].value;
                table[it].value = table[a].value;
                table[a].value = x;
                table[it].time = table[a].time;
                table[a].time = ittime;
            }
        }
        unsigned char mask = 0x78;
        for (unsigned char x = 0; x < 5; ++x) {
            mask = mask & ~table[x].value;
            table[x].value = mask;
        }

        //  This may block interrupts for up to 2 milliseconds.
        //  
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            unsigned short start = TCNT1L;
            start |= ((unsigned short)TCNT1H) << 8u;
            unsigned char curix = 0;
            PORTD = D_SERVO_0 | D_SERVO_1 | D_SERVO_2 | D_SERVO_3;
            while (curix < 5) {
                unsigned short cur = TCNT1L;
                cur |= ((unsigned short)TCNT1H) << 8u;
                if (cur - start >= table[curix].time) {
                    PORTD = table[curix].value;
                    ++curix;
                }
            }
        }
    }
    else {
        PORTB &= ~B_POSITIONING;
    }
}

void onboot(void*) {
    //  do some amount of steering up front to the pre-deterined dumb position
    //  if no packet has come in yet.
    if (poscount == 0) {
        poscount = 5;
    }
    PORTB &= ~B_BLINK;
    after(0, set_servos, 0);
    after(800, idle, (void *)1);
}

class ImSlave : public ITWISlave {
    public:
        virtual void data_from_master(unsigned char n, void const *data) {
            memcpy(positions, data, n > 8 ? 8 : n);
            poscount = RECV_POSCOUNT;
            PORTB |= B_BLINK;
        }
        virtual void request_from_master(void *o_buf, unsigned char &o_size) {
        }
};

ImSlave slave;



void setup() {
    PORTD = 0;
    DDRD = D_SERVO_0 | D_SERVO_1 | D_SERVO_2 | D_SERVO_3;

    PORTB |= B_BLINK | B_POSITIONING;
    DDRB |= B_BLINK | B_POSITIONING;

    fatal_set_blink(&fatal_blink_func);

    setup_timers(F_CPU);

    start_twi_slave(&slave, 0x03);

    after(400, onboot, 0);
}

