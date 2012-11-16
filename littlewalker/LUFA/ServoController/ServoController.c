
#include "ServoController.h"
#include <avr/io.h>
#include <util/atomic.h>


#define BUFFER_SIZE DATA_EPSIZE

#define CMD_DDR 1
#define CMD_POUT 2
#define CMD_PIN 3
#define CMD_TWOBYTEARG 4
#define CMD_PWMRATE CMD_TWOBYTEARG
#define CMD_SETPWM 5
#define CMD_WAIT 6
#define CMD_TWOARG 7
#define CMD_LERPPWM CMD_TWOARG


void Reconfig(void);
void bad_delay(void);
void run_pwm(void);

unsigned char nerrors;
unsigned char scratch[BUFFER_SIZE];
unsigned char indatasz;
unsigned char indata[BUFFER_SIZE];

#define NUM_PWM_TIMERS 8
#define MIN_PWM_TIME 600        //  0.3 ms
#define MAX_PWM_TIME 5400       //  2.7 ms
#define DEFAULT_PWM_TIME 3200   //  1.6 ms

bool pwm_dirty = true;
unsigned short pwm_timers[NUM_PWM_TIMERS] = {
    DEFAULT_PWM_TIME,
    DEFAULT_PWM_TIME,
    DEFAULT_PWM_TIME,
    DEFAULT_PWM_TIME,
    DEFAULT_PWM_TIME,
    DEFAULT_PWM_TIME,
    DEFAULT_PWM_TIME,
    DEFAULT_PWM_TIME
    };
unsigned short pwm_targets[NUM_PWM_TIMERS] = {
    0
};
unsigned char pwm_lerp_counts[NUM_PWM_TIMERS] = {
    0
};
unsigned short pwm_rate = 60000;     //  30 ms
unsigned char c_pwm_values[NUM_PWM_TIMERS];
unsigned short c_pwm_times[NUM_PWM_TIMERS];

void DebugWrite(uint8_t ch) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        unsigned short n = 1000;
        uint8_t st = UCSR1B;
        if (st & (1 << TXEN1)) {
            while (n > 0) {
                st = UCSR1A;
                if (st & (1 << UDRE1)) {
                    break;
                }
                --n;
            }
            UCSR1A = st;
        }
        UCSR1B |= (1 << TXEN1);
        UDR1 = ch;
    }
}

int main(void) {
	SetupHardware();
	sei();

	while (true) {
		USB_USBTask();
		ServoController_Task();
	}
}

void SetupHardware(void) {

	MCUSR &= ~(1 << WDRF);
	wdt_disable();

    DDRB = 0;
    DDRC = 0;
    DDRD = 0;
    DDRE = 0;
    DDRF = 0;
    PORTB = 0;
    PORTC = 0;
    PORTD = 0;
    PORTE = 0;
    PORTF = 0;


	clock_prescale_set(clock_div_1);

    UCSR1B = 0;
    UCSR1C = 0;
    UCSR1A = 0;

    UBRR1H = 0;
    UBRR1L = 16;    //  B115200
    UCSR1A = (1 << U2X1);

    UCSR1C = (1 << USBS1) | (1 << UCSZ11) | (1 << UCSZ10); //  CS8, IGNPAR, STOP2
    UCSR1B = (1 << RXEN1) // | (1 << RXCIE1)
        ;

	USB_Init();

    run_pwm();
}

void EVENT_USB_Device_Connect(void) {
}

void EVENT_USB_Device_Disconnect(void) {
}

void EVENT_USB_Device_ConfigurationChanged(void) {
    Reconfig();
}

void Reconfig() {
	bool ConfigSuccess = true;

    ConfigSuccess &= Endpoint_ConfigureEndpoint(INFO_EPNUM,
        EP_TYPE_BULK, ENDPOINT_DIR_IN, INFO_EPSIZE,
        ENDPOINT_BANK_SINGLE);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(DATA_TX_EPNUM,
        EP_TYPE_BULK, ENDPOINT_DIR_IN, DATA_EPSIZE,
        ENDPOINT_BANK_SINGLE);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(DATA_RX_EPNUM,
        EP_TYPE_BULK, ENDPOINT_DIR_OUT, DATA_EPSIZE,
        ENDPOINT_BANK_SINGLE);

    if (!ConfigSuccess) {
        while (true) {
            DebugWrite(0xed);
            DebugWrite(0x4);
            DebugWrite('F');
            DebugWrite('A');
            DebugWrite('I');
            DebugWrite('L');
            for (unsigned char i = 0; i < 255; ++i) {
                bad_delay();
            }
        }
    }
}

void bad_delay() {
    memset(scratch, 0, sizeof(scratch));
}

void do_ddr(unsigned char cmd, unsigned char data) {
    switch(cmd & 7) {
        case 0: DDRB = data; break;
        case 1: DDRC = data; break;
        case 2: DDRD = data; break;
        case 3: DDRE = data; break;
        case 4: DDRF = data; break;
        default: ++nerrors; break;
    }
}

void do_pout(unsigned char cmd, unsigned char data) {
        DDRB |= 1;
        PORTB |= 1;
    switch (cmd & 7) {
        case 0: PORTB = data & DDRB; break;
        case 1: PORTC = data & DDRC; break;
        case 2: PORTD = data & DDRD; break;
        case 3: PORTE = data & DDRE; break;
        case 4: PORTF = data & DDRF; break;
        default: ++nerrors; break;
    }
        PORTB &= ~1;
}

void do_pin(unsigned char cmd, unsigned char data) {
    switch (cmd & 7) {
        case 0: indata[indatasz++] = PINB & ~DDRB; break;
        case 1: indata[indatasz++] = PINC & ~DDRC; break;
        case 2: indata[indatasz++] = PIND & ~DDRD; break;
        case 3: indata[indatasz++] = PINE & ~DDRE; break;
        case 4: indata[indatasz++] = PINF & ~DDRF; break;
        default: ++nerrors; break;
    }
}

void recalc_pwm(void) {
    unsigned char on = 0xff;
    unsigned char ix = 0;
    unsigned short base = 0;
    while (on != 0 && ix < NUM_PWM_TIMERS) {
        unsigned short next = 0xffff;
        for (unsigned char pw = 0; pw < NUM_PWM_TIMERS; ++pw) {
            unsigned short pwt = pwm_timers[pw];
            if (base == pwt) {
                on &= ~(1 << pw);
            }
            else if (base < pwt && pwt < next) {
                next = pwt;
            }
        }
        base = next;
        c_pwm_values[ix] = on;
        c_pwm_times[ix] = on ? next : 0;
        ++ix;
    }
}

void run_pwm(void) {
    OCR3A = pwm_rate;
    TCCR3A = (1 << WGM31) | (1 << WGM30);
    TCCR3B = (1 << WGM33) | (1 << WGM32) | (1 << CS31);
    if (pwm_rate > 0) {
        OCR3A = pwm_rate;
        TIMSK3 = (1 << TOIE3);
    }
    else {
        OCR3A = 0xffff;
        TIMSK3 = 0;
    }
}

void update_animated_pwm(void) {
    for (unsigned char i = 0; i < NUM_PWM_TIMERS; ++i) {
        unsigned char lt = pwm_lerp_counts[i];
        if (lt) {
            pwm_timers[i] += (int)(pwm_targets[i] - pwm_timers[i]) / lt;
            pwm_dirty = true;
            pwm_lerp_counts[i] -= 1;
        }
    }
}

void one_pwm(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        update_animated_pwm();
        if (pwm_dirty) {
            recalc_pwm();
            pwm_dirty = false;
        }
        if (pwm_rate == 0) {
            TIMSK3 = 0;
        }
        else {
            OCR3A = 0xffff;
            TIMSK3 = 0;
            TCCR3A = 0;
            TCCR3B = (1 << CS31);   //  divide by 8
            TCCR3C = 0;
            //  start timer
            TCNT3H = 0;
            TCNT3L = 0;
            unsigned short next = 0;
            unsigned char ix = 0;
            while (ix < NUM_PWM_TIMERS) {
                PORTB = c_pwm_values[ix];
                next = c_pwm_times[ix];
                if (!next) {
                    break;
                }
                ++ix;
                unsigned short cur;
                do {
                    cur = TCNT3L;
                    cur |= (TCNT3H << 8u);
                } while (cur < next);
            }
        }
        PORTB = 0;
        run_pwm();
    }
}

ISR(TIMER3_OVF_vect) {
    one_pwm();
    TIFR3 = TIFR3;
}

void do_pwmrate(unsigned char cmd, unsigned short arg) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (arg < MIN_PWM_TIME) {
            ++nerrors;
            return;
        }
        pwm_rate = arg;
        run_pwm();
    }
}

void do_setpwm(unsigned char cmd, unsigned short arg) {
    unsigned char ix = cmd & 0xf;
    if (ix >= NUM_PWM_TIMERS) {
        ++nerrors;
        return;
    }
    if ((arg < MIN_PWM_TIME && arg != 0) || (arg > MAX_PWM_TIME)) {
        ++nerrors;
        return;
    }
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        pwm_timers[ix] = arg;
        //  go directly to target, do not pass lerp
        pwm_lerp_counts[ix] = 0;
        pwm_dirty = true;
    }
}

void do_wait(unsigned char ctarg, unsigned short us) {
    OCR1A = 0xffff;
    TCCR1A = (1 << WGM11) | (1 << WGM10);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    TCNT1H = 0;
    TCNT1L = 0;
    while (us > 0) {
        unsigned short then = TCNT1L;
        then = then | (TCNT1H << 8u);
        unsigned short bit = us;
        //  Just wait a millisecond at a time; means we minimize the impact 
        //  of PWM interrupts happening and screwing with the timer.
        if (bit > 1000) {
            bit = 1000;
        }
        while (true) {
            unsigned short now = TCNT1L;
            now = now | (TCNT1H << 8u);
            if (now - then >= bit) {
                us -= bit;
                break;
            }
        }
    }
}

void do_lerppwm(unsigned char channel, unsigned short target, unsigned char count) {
    if (channel > NUM_PWM_TIMERS) {
        ++nerrors;
        return;
    }
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        pwm_targets[channel] = target;
        pwm_lerp_counts[channel] = count;
    }
}

void do_cmd(unsigned char ccode, unsigned char ctarg, unsigned short arg, unsigned short arg2) {
    switch (ccode) {
    case CMD_DDR:
        do_ddr(ctarg, arg);
        break;
    case CMD_POUT:
        do_pout(ctarg, arg);
        break;
    case CMD_PIN:
        do_pin(ctarg, arg);
        break;
    case CMD_PWMRATE:
        do_pwmrate(ctarg, arg);
        break;
    case CMD_SETPWM:
        do_setpwm(ctarg, arg);
        break;
    case CMD_WAIT:
        do_wait(ctarg, arg);
        break;
    case CMD_LERPPWM:
        do_lerppwm(ctarg, arg, arg2);
        break;
    default:
        ++nerrors;
        break;
    }
}

void do_cmds(unsigned char const *cmd, unsigned char cnt) {
    while (cnt > 0) {
        if (cnt > sizeof(scratch)) {
            ++nerrors;
            return;
        }
        unsigned char ccode = cmd[0] >> 4;
        unsigned char ctarg = cmd[0] & 0xf;
        unsigned char csz = 2;
        unsigned char arg2 = 0;
        if (ccode >= CMD_TWOBYTEARG) {
            csz = 3;
        }
        if (cnt < csz) {
            ++nerrors;
            return;
        }
        unsigned short arg = cmd[1];
        if (ccode >= CMD_TWOBYTEARG) {
            arg = ((unsigned short)arg << 8u) | cmd[2];
        }
        if (ccode >= CMD_TWOARG) {
            arg2 = cmd[csz];
            csz += 1;
        }
        cmd += csz;
        cnt -= csz;
        do_cmd(ccode, ctarg, arg, arg2);
    }
}


void ServoController_Task(void) {

	if (USB_DeviceState != DEVICE_STATE_Configured) {
	  return;
    }

    if (indatasz > 0) {
        Endpoint_SelectEndpoint(DATA_TX_EPNUM);
        Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
        if (Endpoint_IsConfigured() && Endpoint_IsINReady() && Endpoint_IsReadWriteAllowed()) {
            unsigned char g = 0;
            while (g < indatasz) {
                Endpoint_Write_8(indata[g]);
                ++g;
            }
            indatasz = 0;
            Endpoint_ClearIN();
        }
    }

    Endpoint_SelectEndpoint(DATA_RX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_OUT);
    if (Endpoint_IsConfigured() && Endpoint_IsOUTReceived() && Endpoint_IsReadWriteAllowed()) {
        uint8_t n = Endpoint_BytesInEndpoint();
        unsigned char g = 0;
        while (n > 0) {
            scratch[g & (sizeof(scratch) - 1)] = Endpoint_Read_8();
            ++g;
            --n;
        }
        Endpoint_ClearOUT();
        do_cmds(scratch, g);
    }

    Endpoint_SelectEndpoint(INFO_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
    if (Endpoint_IsConfigured() && Endpoint_IsINReady() && Endpoint_IsReadWriteAllowed()) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            Endpoint_Write_8(nerrors);
            nerrors = 0;
        }
        Endpoint_ClearIN();
    }
}

