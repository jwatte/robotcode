
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <util/atomic.h>

#include "powerboard.h"

#define AOUT_ARMED 0x8
#define AOUT_GUNSON 0x80
#define MUX_VSENSE 0
#define AIN_VSENSE (1 << MUX_VSENSE)

#define BOUT_PWRON 0x1
#define BOUT_SERVOSON 0x2
#define BOUT_FANSON 0x4

#define VOLT_ADJUST 4
#define VOLT_DIVIDE 1

#define TWI_ADDR_READ 0x13
#define TWI_ADDR_WRITE 0x12


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



unsigned short armed_out_jiffies = 0;
unsigned short armed_out_wait = 0;

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
    }
}


void init_time() {
    power_timer1_enable();
    TCCR1A = 0;             //  Normal mode
    TCCR1B = (1 << CS10);   //  clk/1, run at approximately 305 Hz
    TCCR1C = 0;
    TIMSK1 = (1 << TOIE1);  //  interrupt on overflow
}

void init_ports() {
    DDRA = (AOUT_ARMED | AOUT_GUNSON);
    PORTA = AOUT_ARMED;
    DDRB = (BOUT_PWRON | BOUT_SERVOSON | BOUT_FANSON);
    PORTB = BOUT_PWRON | BOUT_FANSON;
    blink_armed(300);
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


enum {
    TWIStateWaiting = 0,
    TWIStateRecvAddress = 1,
    TWIStateReceiving = 2,
    TWIStateSending = 3
};
unsigned char twi_state = TWIStateWaiting;
unsigned char twi_ptr = 0;
unsigned short twi_last_state_time = 0;

unsigned char write_state = STATE_FANS | STATE_PWR;
unsigned short read_state;

void twi_recv_address(unsigned short jifs, unsigned char sr) {
    if (sr & (1 << USIOIF)) {
        twi_last_state_time = jifs;
        unsigned char br = USIBR;
        if (br == TWI_ADDR_READ) {
            USIDR = 0;
            sr = (1 << USISIF) | (1 << USIOIF) | 14;    //  ack
            twi_state = TWIStateSending;
            twi_ptr = 0;
            read_state = get_adc() * VOLT_ADJUST / VOLT_DIVIDE;
        }
        else if (br == TWI_ADDR_WRITE) {
            USIDR = 0;
            sr = (1 << USISIF) | (1 << USIOIF) | 14;    //  ack
            twi_state = TWIStateReceiving;
            twi_ptr = 0;
        }
        else {
            twi_state = TWIStateWaiting;
        }
        USISR = sr;
    }
}

void twi_receiving(unsigned short jifs, unsigned char sr) {
    if (sr & (1 << USIOIF)) {
        twi_last_state_time = jifs;
        //  
        write_state = USIBR;
        USISR = sr;
    }
}

void twi_sending(unsigned short jifs, unsigned char sr) {
    if (sr & (1 << USIOIF)) {
        twi_last_state_time = jifs;
        //  
        USIDR = (twi_ptr) ? (read_state & 0xff) : ((read_state >> 8) & 0xff);
        USISR = sr;
        twi_ptr = !twi_ptr;
    }
}

void check_twi(unsigned short jifs) {
    unsigned char sr = USISR;
    if ((short)(jifs - twi_last_state_time) > 100) {
        twi_state = TWIStateWaiting;
        twi_last_state_time = jifs;
    }
    if (sr & (1 << USISIF)) {
        twi_state = TWIStateRecvAddress;
        twi_ptr = 0;
    }
    switch (twi_state) {
    case TWIStateWaiting:       /* do nothing */ break;
    case TWIStateRecvAddress:   twi_recv_address(jifs, sr); break;
    case TWIStateReceiving:     twi_receiving(jifs, sr); break;
    case TWIStateSending:       twi_sending(jifs, sr); break;
    }
    //  stop condition?
    if (sr & (1 << USIPF)) {
        twi_state = TWIStateWaiting;
    }
}

void init_twi() {
    power_usi_enable();
    //  TWI mode, hold SCL low when receiving
    //  clock on negative edge
    USICR = (1 << USIWM1) | (1 << USIWM0) | (1 << USICS1) | (1 << USICS0);
}



void initialize() {
    init_time();
    init_ports();
    init_adc();
    init_twi();
}


void update_state(unsigned short jifs) {
    if (write_state & STATE_FANS) {
        PORTB |= BOUT_FANSON;
    }
    else {
        PORTB &= ~BOUT_FANSON;
    }
    if (write_state & STATE_SERVOS) {
        PORTB |= BOUT_SERVOSON;
    }
    else {
        PORTB &= ~BOUT_SERVOSON;
    }
    if (write_state & STATE_FANS) {
        PORTB |= BOUT_FANSON;
    }
    else {
        PORTB &= ~BOUT_FANSON;
    }
    if (write_state & STATE_GUNS) {
        PORTA |= AOUT_GUNSON;
    }
    else {
        PORTA &= ~AOUT_GUNSON;
    }
}

void update_blink(unsigned short jifs) {
    if (write_state & STATE_GUNS) {
        if (!armed_out_jiffies && !armed_out_wait) {
            armed_out_wait = (get_jiffies() + 300) | 1;
        }
        else if (armed_out_wait) {
            if ((short)(jifs - armed_out_wait) > 0) {
                armed_out_wait = 0;
                blink_armed(100);
            }
        }
    }
    else {
        armed_out_wait = 0;
    }
}   


void loop() {
    unsigned short jifs = get_jiffies();
    check_armed(jifs);
    check_twi(jifs);
    update_state(jifs);
    update_blink(jifs);
}


int main() {
    initialize();
    sei();
    while (true) {
        loop();
    }
    return 0;
}
