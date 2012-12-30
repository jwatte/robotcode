
#define F_CPU 16000000

#include <pins_avr.h>
#include <avr/io.h>
#include <avr/interrupt.h>


#define PWMKHZ 7
#define RUNTIME 120L

//  Vref is 5V
//  resistor divider is 2:1 after R10 mod
//  -> full scale == 10V
//  -> 65/100 of full scale is approximately 6.5V, which is a good cut-off point for 2S lipo
#define BATTERY_WARNING (0xffffUL * 65 / 100)
//  3/10th of 1 Volt in hysteresis before we detect battery is OK again
#define BATTERY_HYSTERESIS (0xffffUL * 3 / 100)

#if PWMKHZ == 62
enum {
    Timer1ClockBits = (1 << CS10)    //  60 kHz PWM
};
enum {
    SECOND_MULTIPLIER = 62000
};
#elif PWMKHZ == 7
enum {
    Timer1ClockBits = (1 << CS11)    //  7.5 kHz PWM
};
enum {
    SECOND_MULTIPLIER = 7812
};
#elif PWMKHZ == 1
enum {
    Timer1ClockBits = (1 << CS11) | (1 << CS10)    //  900 Hz PWM
};
enum {
    SECOND_MULTIPLIER = 980
};
#else
#error "define PWMKHZ correctly"
#endif
unsigned char debounce = 20;

unsigned char x = 0xff;
bool on_boop = false;
bool button = false;

typedef unsigned long timer_t;

volatile timer_t _timer = 0;

ISR(TIMER1_OVF_vect) {
    _timer += 1;
}

timer_t ticks() {
    timer_t ret;
    unsigned char v = disable_interrupts();
    ret = _timer;
    restore_interrupts(v);
    return ret;
}

unsigned long randseed = 0x1337;

unsigned char randbyte() {
    randseed = randseed * 587381 + 592693;
    return (randseed >> 8) & 0xff;
}

void read_button() {
    button = false;
    if (!(PINB & (1 << PB4))) {
        if (debounce != 0xff) {
            --debounce;
        }
        button = (debounce == 0);
    }
    else {
        debounce = 20;
    }
}

void light(bool on) {
    if (on) {
        PORTB |= (1 << PB5);
    }
    else {
        PORTB &= ~(1 << PB5);
    }
}

void boop_on() {
    if (!on_boop) {
        on_boop = true;
        TCCR2A = (1 << COM2B0) | (1 << WGM21);
        TCCR2B = (1 << CS22) | (1 << CS20);  //  clk / 128
        OCR2A = 0xFF;
        OCR2B = 0x0;
    }
}

void boop_off() {
    TCCR2A = 0;
    TCCR2B = 0;
    on_boop = false;
}


bool motors_are_on = false;

void motors_start() {
    motors_are_on = true;
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10);
}

void motors_stop() {
    motors_are_on = false;
    TCCR1A = (1 << WGM10);
    PORTB &= ~((1 << PB1) | (1 << PB2));
}

//  tweakables
unsigned char fwdleft = 200;
unsigned char fwdright = 190;

void motors_power(int left, int right) {
    if (left < 0) {
        PORTD |= (1 << PD7);
        left = -left;
    }
    else {
        PORTD &= ~(1 << PD7);
    }
    if (left > 255) {
        left = 255;
    }
    if (right < 0) {
        PORTB |= (1 << PB0);
        right = -right;
    }
    else {
        PORTB &= ~(1 << PB0);
    }
    if (right > 255) {
        right = 255;
    }
    OCR1AH = 0;
    OCR1AL = left * fwdleft >> 8;
    OCR1BH = 0;
    OCR1BL = right * fwdright >> 8;
}

bool on_nav = false;
timer_t nav_start = 0;

void nav_on() {
    if (!on_nav) {
        on_nav = true;
        nav_start = ticks();
        light(true);
        motors_power(0, 0);
        motors_start();
    }
}

void nav_off() {
    light(false);
    on_nav = false;
    motors_stop();
}

bool leftSensor = false;
bool rightSensor = false;

void read_sensors() {
    leftSensor = (PIND & (1 << PD5)) == 0;
    rightSensor = (PINB & (1 << PB3)) == 0;
}

unsigned char state = 0;
unsigned char maxpower = 0;
timer_t state_start = 0;
enum {
    Forward = 0,
    BackwardThenLeft = 1,
    BackwardThenRight = 2,
    Left = 3,
    Right = 4,
    TurningLeft = 5,
    TurningRight = 6
};


void update_nav() {
    timer_t timeinstate = ticks() - state_start;
    unsigned char newstate = state;
    switch (state) {
    case TurningLeft:
    case TurningRight:
    case Forward:
        if (leftSensor) {
            if (rightSensor) {
                if (randbyte() & 1) {
                    newstate = BackwardThenLeft;
                    maxpower = 0;
                }
                else {
                    newstate = BackwardThenRight;
                    maxpower = 0;
                }
            }
            else {
                newstate = TurningRight;
            }
        }
        else if (rightSensor) {
            newstate = TurningLeft;
        }
        else {
            newstate = Forward;
        }
        break;
    case BackwardThenRight:
        if (timeinstate > SECOND_MULTIPLIER) {
            newstate = Right;
            maxpower = 200;
        }
        break;
    case BackwardThenLeft:
        if (timeinstate > SECOND_MULTIPLIER) {
            newstate = Left;
            maxpower = 200;
        }
        break;
    case Left:
    case Right:
        if (timeinstate > SECOND_MULTIPLIER / 2) {
            newstate = Forward;
            maxpower = 0;
        }
        break;
    }
    //  This loop runs too often, so this doesn't do much 
    //  of mechanical acceleration; it ramps up very quickly.
    if (maxpower < 255) {
        maxpower += 1;
    }
    if (newstate != state) {
        state_start = ticks();
        state = newstate;
    }
    int back = -maxpower / 3;
    int half = maxpower / 2;
    switch (state) {
    case TurningLeft:
        motors_power(maxpower / 2, maxpower);
        break;
    case TurningRight:
        motors_power(maxpower, maxpower / 2);
        break;
    case Forward:
        motors_power(maxpower, maxpower);
        break;
    case BackwardThenRight:
    case BackwardThenLeft:
        motors_power(back, back);
        break;
    case Right:
        motors_power(half, back);
        break;
    case Left:
        motors_power(back, half);
        break;
    }
}

unsigned short battery = 0xffff;

void read_adc_battery() {
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) {
        //  do nothing
    }
    battery = ADCL;
    battery |= ((unsigned short)ADCH << 8);
}

void battery_shutdown() {
    motors_power(0, 0);
    bool motors_were_on = motors_are_on;
    motors_stop();  //  turns off PWM, but timer keeps running
    timer_t t;
    while (true) {
        boop_on();
        OCR2A = 0x40;
        OCR2B = 0x20;
        light(false);
        t = ticks();
        while (ticks() - t < (SECOND_MULTIPLIER >> 2)) {
            wdt_reset();
        }
        boop_off();
        light(true);
        t = ticks();
        while (ticks() - t < (SECOND_MULTIPLIER >> 2)) {
            wdt_reset();
        }
        read_adc_battery();
        if (battery > BATTERY_WARNING + BATTERY_HYSTERESIS) {
            break;
        }
    }
    if (motors_were_on) {
        motors_start();
    }
}


timer_t last_update;

void update()
{
    timer_t this_update, diff;
    //  run no more than 500 times a second
    bool adc_read = false;
    do {
        //  read the ADC
        if (!adc_read) {
            read_adc_battery();
            if (battery <= BATTERY_WARNING) {
                battery_shutdown();
            }
        }
        adc_read = true;

        //  calculate sleep time
        this_update = ticks();
        diff = this_update - last_update;
    }
    while (diff < SECOND_MULTIPLIER >> 9);
    last_update = this_update;

    if (on_boop) {
        OCR2A = x;
        OCR2B = (x >> 1) + 1;
        x -= 1;
        if (x < 32) {
            x = 0xff;
            boop_off();
            nav_on();
        }
    }
    else if (on_nav) {
        timer_t nav_time = ticks() - nav_start;
        if (nav_time > (RUNTIME * SECOND_MULTIPLIER) || button) {
            nav_off();
        }
        else {
            light((diff % SECOND_MULTIPLIER) < (SECOND_MULTIPLIER >> 1));
            update_nav();
        }
    }
    else {
        if (button) {
            boop_on();
        }
    }
}

int main() {
    //  I can't use timers!
    //  So I can't use "after"!
    //setup_timers(F_CPU);
    //  buzzer
    PORTB = (1 << PB4);   //  pull-up
    DDRB = (1 << 5) | (1 << 2) | (1 << 1) | (1 << 0);
    PORTD = 0; //  pull-up
    DDRD = (1 << 3) | (1 << 7);

    power_timer2_enable();
    ASSR = 0;

    power_timer1_enable();
    TCCR1A = (1 << WGM10);
    TCCR1B = (1 << WGM12) | Timer1ClockBits;
    TCNT1H = 0;
    TCNT1L = 0;
    TIMSK1 = (1 << TOIE1);
    OCR1AH = 0;
    OCR1AL = 0;
    OCR1BH = 0;
    OCR1BL = 0;

    power_adc_enable();
    ADCSRA = (1 << ADEN) | (7 << ADPS0);    //  slow clock
    ADCSRB = 0;
    DIDR0 = (1 << ADC1D);
    ADMUX = (1 << REFS0) | (1 << ADLAR) | (1 << MUX0);


    enable_interrupts();

    while (true) {
        read_button();
        read_sensors();
        update();
    }

    return 0;
}


