#define F_CPU 20000000
#include <libavr.h>
#include <pins_avr.h>


//  port B
#define B_BLINK (1<<0)
#define B_FORWARDISH (1<<1)
#define B_BACKWARDISH (1<<2)

//  port D
#define D_PWMHL (1<<3)
#define D_PWMHR (1<<4)
//  OCR0B == left == PD5
#define D_DIRL (1<<5)
//  OCR0A == right == PD6
#define D_DIRR (1<<6)

//  timer 0
#define WGM_0 ((1 << WGM01) | (1 << WGM00))


unsigned char driveR = 0x80;
unsigned char driveL = 0x80;
unsigned char currentR = 0x80;
unsigned char currentL = 0x80;

void fatal_blink_func(bool on)
{
    DDRB |= B_BLINK | B_FORWARDISH | B_BACKWARDISH;
    if (on) {
        PORTB |= B_BLINK | B_FORWARDISH | B_BACKWARDISH;
    }
    else {
        PORTB &= ~(B_BLINK | B_FORWARDISH | B_BACKWARDISH);
    }
    //  immediately brake both sides!
    PORTD &= ~(D_PWMHL | D_PWMHR);
}

void got_adc_r(unsigned char val) {
    currentR = val;
}

void got_adc_l(unsigned char val) {
    currentL = val;
    adc_read(1, &got_adc_r);
}

void read_adc_l(void *) {
    if (adc_busy()) {
        after(1, &read_adc_l, 0);
        return;
    }
    adc_read(0, &got_adc_l);
}

void set_motor_pwm(void *) {
    OCR0A = driveR;
    OCR0B = driveL;
    after(20, set_motor_pwm, 0);
    after(1, read_adc_l, 0);

    PORTB &= ~(B_FORWARDISH | B_BACKWARDISH);
    if (driveR > 0x80 || driveL > 0x80) {
        PORTB |= B_FORWARDISH;
    }
    if (driveR < 0x7f || driveL < 0x7f) {
        PORTB |= B_BACKWARDISH;
    }

    if ((driveR == 0x80) || (driveR == 0x7f)) {
        PORTD &= ~D_PWMHR;
    }
    else {
        PORTD |= D_PWMHR;
    }
    if ((driveL == 0x80) || (driveL == 0x7f)) {
        PORTD &= ~D_PWMHL;
    }
    else {
        PORTD |= D_PWMHL;
    }
}

void idle(void *p) {
    if (p) {
        PORTB |= B_BLINK;
        after(100, &idle, (void *)0);
    }
    else {
        PORTB &= ~B_BLINK;
        after(1900, &idle, (void *)1);
    }
}
void onboot(void*) {
    PORTB &= ~B_BLINK;
    TCCR0A = WGM_0 | (1 << COM0A1) | (1 << COM0B1);
    after(20, set_motor_pwm, 0);
    after(800, idle, (void *)1);
}

class ImSlave : public ITWISlave {
    public:
        virtual void data_from_master(unsigned char n, void const *data) {
            if (n < 2) {
                return;
            }
            driveL = ((unsigned char *)data)[0];
            driveR = ((unsigned char *)data)[1];
            PORTB |= B_BLINK;
        }
        virtual void request_from_master(void *o_buf, unsigned char &o_size) {
            o_size = 2;
            ((unsigned char *)o_buf)[0] = currentL;
            ((unsigned char *)o_buf)[1] = currentR;
            PORTB |= B_BLINK;
        }
};

ImSlave slave;



void setup() {
    PORTD &= ~(D_PWMHR | D_PWMHL);
    DDRD |= D_PWMHR | D_PWMHL | D_DIRL | D_DIRR;

    PORTB |= (B_BLINK | B_FORWARDISH | B_BACKWARDISH);
    DDRB |= (B_BLINK | B_FORWARDISH | B_BACKWARDISH);

    TCCR0A = WGM_0;
    TCCR0B = (1 << CS01);   //  9.7 kHz -- whiiiine!
    OCR0A = 0x80;
    OCR0B = 0x80;

    fatal_set_blink(&fatal_blink_func);

    setup_timers(F_CPU);
    adc_setup();

    start_twi_slave(&slave, 0x01);

    after(400, onboot, 0);
}

