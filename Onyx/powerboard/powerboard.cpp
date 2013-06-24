
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/cpufunc.h>
#include <util/atomic.h>

#include "powerboard.h"
#include "usiTwiSlave.h"

#define AOUT_INDICATOR 0x4
#define AOUT_DEBUG (0x4^AOUT_INDICATOR)
#define AOUT_ARMED 0x8
#define AOUT_GUNSON 0x80
#define MUX_VSENSE 0
#define AIN_VSENSE (1 << MUX_VSENSE)
#define AOUT_SCL 0x10
#define AOUT_SDA 0x40

#define BOUT_PWRON 0x1
#define BOUT_SERVOSON 0x2
#define BOUT_FANSON 0x4

#define TWI_ADDR_7BIT 0x09


#define STATE_PWR 0x1
#define STATE_SERVOS 0x2
#define STATE_FANS 0x4
#define STATE_GUNS 0x8

unsigned short _jiffies;

ISR(TIM1_OVF_vect) {
    _jiffies += 1;
}

unsigned short get_jiffies() {
    unsigned short ret;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ret = _jiffies;
    }
    return ret;
}


static void _delay(unsigned short s) {
    while (s > 0) {
        _NOP();
        _NOP();
        _NOP();
        --s;
    }
}


unsigned short armed_out_jiffies = 0;
unsigned short armed_out_wait = 0;
unsigned short indicator_jiffies = 0;

void blink_armed(unsigned short duration_j) {
    armed_out_jiffies = (get_jiffies() + duration_j) | 1;
    PORTA |= AOUT_ARMED;
}

void check_armed(unsigned short jifs) {
    if (armed_out_jiffies) {
        if ((short)(jifs - armed_out_jiffies) > 0) {
            armed_out_jiffies = 0;
            PORTA &= ~(AOUT_ARMED);
        }
    } }


void init_time() {
    power_timer1_enable();
    TCCR1A = 0;             //  Normal mode
    TCCR1B = (1 << CS10);   //  clk/1, run at approximately 305 Hz
    TCCR1C = 0;
    TIMSK1 = (1 << TOIE1);  //  interrupt on overflow
}

#define INITIAL_ARMED_BLINK 110

void init_ports() {
    DDRA = AOUT_ARMED | AOUT_GUNSON | AOUT_INDICATOR | AOUT_DEBUG;
    PORTA = AOUT_ARMED | AOUT_INDICATOR;
    DDRB = BOUT_PWRON | BOUT_SERVOSON | BOUT_FANSON;
    PORTB = BOUT_PWRON | BOUT_FANSON;
    blink_armed(INITIAL_ARMED_BLINK);
}

unsigned short _last_adc = 0x3ff;

ISR(ADC_vect) {
    _last_adc = ADC;
}

unsigned short get_adc() {
    unsigned short ret;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ret = _last_adc;
    }
    return ret;
}

void init_adc() {
    ADCSRA = 0;
    power_adc_enable();
    ADMUX = MUX_VSENSE;  //  channel 0
    ADCSRA = (1 << ADEN) | (1 << ADATE) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    ADCSRB = (1 << ADTS2) | (1 << ADTS1);   //  trigger source is timer 1 overflow
    DIDR0 |= AIN_VSENSE;
}


unsigned char force_off = 0;
unsigned char write_state = STATE_FANS | STATE_PWR;
unsigned short read_state;


void initialize() {
    init_time();
    init_ports();
    init_adc();
    usiTwiSlaveInit( TWI_ADDR_7BIT );
}


void check_bit(unsigned char state, unsigned char check, unsigned char volatile &reg, unsigned char action) {
    if (state & check) {
        reg |= action;
    }
    else {
        reg &= ~action;
    }
}

void update_state(unsigned short jifs) {

    unsigned char use_state = write_state & ~force_off;

    check_bit(use_state, STATE_PWR, PORTB, BOUT_PWRON);
    check_bit(use_state, STATE_SERVOS, PORTB, BOUT_SERVOSON);
    check_bit(use_state, STATE_FANS, PORTB, BOUT_FANSON);
    check_bit(use_state, STATE_GUNS, PORTA, AOUT_GUNSON);
}

#define ARMED_OUT_WAIT 100
#define ARMED_OUT_BLINK 50
#define INDICATOR_OUT_WAIT 30
#define INDICATOR_OUT_BLINK 170

void update_blink(unsigned short jifs) {
    if (write_state & STATE_GUNS) {
        if (!armed_out_jiffies && !armed_out_wait) {
            armed_out_wait = (get_jiffies() + ARMED_OUT_WAIT) | 1;
        }
        else if (armed_out_wait) {
            if ((short)(jifs - armed_out_wait) > 0) {
                armed_out_wait = 0;
                blink_armed(ARMED_OUT_BLINK);
            }
        }
    }
    else {
        armed_out_wait = 0;
    }
    unsigned short delay = INDICATOR_OUT_WAIT;
    if (PORTA & AOUT_INDICATOR) {
        delay = INDICATOR_OUT_BLINK;
    }
    if (jifs - indicator_jiffies >= delay) {
        PORTA = PORTA ^ AOUT_INDICATOR;
        indicator_jiffies += delay;
    }
}   


void check_twi(unsigned short jifs) {
    if (usiTwiDataInReceiveBuffer()) {
        write_state = usiTwiReceiveByte();
    }
    if (usiTwiTransmitBufferEmpty()) {
        read_state = get_adc();
        cli();
        usiTwiTransmitByte(read_state & 0xff);
        usiTwiTransmitByte((read_state >> 8) & 0xff);
        usiTwiTransmitByte(write_state);
        usiTwiTransmitByte(force_off);
        sei();
    }
}

unsigned char indicator_state = -1;
unsigned short last_indicator = 0;

void update_indicator(unsigned short jifs) {
    if (jifs - last_indicator > 40) {
        bool bit = false;
        ++indicator_state;
        switch (indicator_state) {
        case 0:
            bit = false;
            break;
        case 1: case 2: case 3: case 4: case 5:
            bit = (write_state & (1 << (indicator_state - 1))) != 0;
            break;
        default:
            bit = true;
            if (indicator_state >= 15) {
                indicator_state = -1;
            }
            break;
        }
        if (bit) {
            PORTA |= AOUT_INDICATOR;
        }
        else {
            PORTA &= ~AOUT_INDICATOR;
        }
        last_indicator = jifs;
    }
}

unsigned short last_reading_jifs;
unsigned short last_reading;

void check_voltage(unsigned short jifs) {
    if (jifs - last_reading_jifs < 50) {
        return;
    }
    unsigned short reading = get_adc();
    unsigned short use_reading = reading;
    if (reading < last_reading) {
        use_reading = last_reading;
    }
    last_reading = reading;
    last_reading_jifs = jifs;

    //  0x34c == 16.8 Volts
    //  voltage conversion factor is between 0.0199 and 0.02
    if (use_reading < 0x283) {  //  12.8 Volts
        force_off |= STATE_PWR | STATE_SERVOS;
        PORTA |= AOUT_ARMED;
        PORTA &= ~AOUT_INDICATOR;
    }
    else if (use_reading < 0x298) { //  13.0 Volts
        force_off |= STATE_SERVOS;
    }
}

void loop() {
    unsigned short jifs = get_jiffies();
    check_armed(jifs);
    check_twi(jifs);
    update_state(jifs);
    update_blink(jifs);
    update_indicator(jifs);
    check_voltage(jifs);
}


int main() {
    initialize();
    sei();
    while (true) {
        loop();
    }
    return 0;
}
