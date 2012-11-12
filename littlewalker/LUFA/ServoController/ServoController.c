
#include "ServoController.h"
#include <avr/io.h>
#include <util/atomic.h>


#define BUFFER_SIZE DATA_EPSIZE

#define CMD_DDR 1
#define CMD_POUT 2
#define CMD_PIN 3

//  for CMD_WAVE, 'data' is pairs of <value, duration> until end of packet
#define CMD_WAVE 4

void Reconfig(void);
void bad_delay(void);

unsigned char nerrors;
unsigned char scratch[BUFFER_SIZE];
unsigned char indatasz;
unsigned char indata[BUFFER_SIZE];

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

void do_cmd(unsigned char cmd, unsigned char data) {
    switch (cmd >> 4) {
    case CMD_DDR:
        switch(cmd & 7) {
        case 0: DDRB = data; break;
        case 1: DDRC = data; break;
        case 2: DDRD = data; break;
        case 3: DDRE = data; break;
        case 4: DDRF = data; break;
        default: ++nerrors; break;
        }
        break;
    case CMD_POUT:
        switch (cmd & 7) {
        case 0: PORTB = data & DDRB; break;
        case 1: PORTC = data & DDRC; break;
        case 2: PORTD = data & DDRD; break;
        case 3: PORTE = data & DDRE; break;
        case 4: PORTF = data & DDRF; break;
        default: ++nerrors; break;
        }
        break;
    case CMD_PIN:
        switch (cmd & 7) {
        case 0: indata[indatasz++] = PINB & ~DDRB; break;
        case 1: indata[indatasz++] = PINC & ~DDRC; break;
        case 2: indata[indatasz++] = PIND & ~DDRD; break;
        case 3: indata[indatasz++] = PINE & ~DDRE; break;
        case 4: indata[indatasz++] = PINF & ~DDRF; break;
        default: ++nerrors; break;
        }
        break;
    default:
        ++nerrors; break;
    }
}

void do_wave(unsigned char reg, unsigned char const *data, unsigned char size) {

    PORTB = 0xff;
    volatile unsigned char *port = NULL;

    switch (reg) {
    case 0: port = &PORTB; break;
    case 1: port = &PORTC; break;
    case 2: port = &PORTD; break;
    case 3: port = &PORTE; break;
    case 4: port = &PORTF; break;
    default: ++nerrors; return;
    }

    unsigned short next = 0;
    unsigned short cur = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        TCCR3A = 0;
        TCCR3B = (1 << CS31);   //  divide by 8
        TCCR3C = 0;
        //  start timer
        TCNT3H = 0;
        TCNT3L = 0;
        while (size >= 2) {
            *port = *data++;
            next += (*data++ + 1);
            size -= 2;
            do {
                cur = TCNT3L;
                cur |= (TCNT3H << 8u);
            } while (cur < next);
        }
    }
    PORTB = 0;
}

void do_cmds(unsigned char const *cmd, unsigned char cnt) {
    while (cnt >= 2) {
        if ((cmd[0] >> 4) == CMD_WAVE) {
            do_wave(cmd[0] & 0xf, cmd + 1, cnt - 1);
            cmd += cnt;
            cnt = 0;
        }
        else {
            do_cmd(cmd[0], cmd[1]);
            cmd += 2;
            cnt -= 2;
        }
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

