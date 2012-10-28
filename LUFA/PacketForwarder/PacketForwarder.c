
#include "PacketForwarder.h"
#include <avr/io.h>
#include <util/atomic.h>


// SRAM is scarce -- the device stalls if we 
//  get to 256
#define BUFFER_SIZE 128

void Reconfig(void);
void bad_delay(void);

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
    DDRD |= 0x8;
    PORTD &= ~0x8;
    bad_delay();
    PORTD |= 0x8;
    bad_delay();
    PORTD &= ~0x8;
    bad_delay();
    PORTD |= 0x8;
    bad_delay();
	SetupHardware();
	//LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	sei();

	while (true) {
		USB_USBTask();
		PacketForwarder_Task();
	}
}

void SetupHardware(void) {
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
    DDRD |= (1 << 7);
    PORTD |= (1 << 7);

	clock_prescale_set(clock_div_1);

	//LEDs_Init();

    UCSR1B = 0;
    UCSR1C = 0;
    UCSR1A = 0;
    UCSR1D = 0;

    UBRR1H = 0;
    UBRR1L = 16;    //  B115200
    UCSR1A = (1 << U2X1);

    UCSR1C = (1 << USBS1) | (1 << UCSZ11) | (1 << UCSZ10); //  CS8, IGNPAR, STOP2
    UCSR1B = (1 << RXCIE1) | (1 << RXEN1);

    //  try to reset the remote board
    PORTD &= ~(1 << 7);
    //  ghetto delay loop
    for (unsigned char volatile i = 0; i < 255; ++i) {
        bad_delay();
    }
    PORTD |= (1 << 7);

	USB_Init();
}

void EVENT_USB_Device_Connect(void) {
	//LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

void EVENT_USB_Device_Disconnect(void) {
	//LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
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

	//LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
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

volatile uint8_t rx_buf[BUFFER_SIZE];
volatile uint8_t rx_head;
volatile uint8_t rx_tail;

volatile uint8_t tx_buf[BUFFER_SIZE];
volatile uint8_t tx_head;
volatile uint8_t tx_tail;

uint8_t num_ovf;

void bad_delay() {
    memset(rx_buf, 0, sizeof(rx_buf));
}

void StartTransmit(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (tx_head != tx_tail) {
            //  If I am not currently transmitting
            bool write1 = false;
            if (!(UCSR1B & (1 << TXEN1))) {
                write1 = true;
            }
            UCSR1B = UCSR1B | (1 << UDRIE1) | (1 << TXEN1);
            if (write1) {
                UDR1 = tx_buf[tx_tail & (sizeof(tx_buf) - 1)];
                ++tx_tail;
            }
        }
    }
}

ISR(USART1_UDRE_vect) {
    if (tx_head == tx_tail) {
        UCSR1B = UCSR1B & ~((1 << UDRIE1) | (1 << TXEN1));
    }
    else {
        UDR1 = tx_buf[tx_tail & (sizeof(tx_buf) - 1)];
        ++tx_tail;
    }
}

ISR(USART1_RX_vect) {
    uint8_t b = UDR1;
    rx_buf[rx_head & (sizeof(rx_buf)-1)] = b;
    if (rx_head - rx_tail >= 127) {
        ++num_ovf;
    }
    ++rx_head;
}

void PacketForwarder_Task(void) {
	if (USB_DeviceState != DEVICE_STATE_Configured) {
	  return;
    }

    uint8_t g = (rx_head - rx_tail) & (sizeof(rx_buf) - 1);
    if (g > 0) {
        Endpoint_SelectEndpoint(DATA_TX_EPNUM);
        Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
        if (Endpoint_IsConfigured() && Endpoint_IsINReady() && Endpoint_IsReadWriteAllowed()) {
            if (g > DATA_EPSIZE) {
                g = DATA_EPSIZE;
            }
            while (g > 0) {
                Endpoint_Write_8(rx_buf[rx_tail & (sizeof(rx_buf) - 1)]);
                ++rx_tail;
                --g;
            }
            Endpoint_ClearIN();
        }
    }

    Endpoint_SelectEndpoint(DATA_RX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_OUT);
    if (Endpoint_IsConfigured() && Endpoint_IsOUTReceived() && Endpoint_IsReadWriteAllowed()) {
        uint8_t n = Endpoint_BytesInEndpoint();
        while (n > 0) {
            tx_buf[tx_head & (sizeof(tx_buf) - 1)] = Endpoint_Read_8();
            ++tx_head;
            --n;
        }
        Endpoint_ClearOUT();
        StartTransmit();
    }

    if (num_ovf) {
        Endpoint_SelectEndpoint(INFO_EPNUM);
        Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
        if (Endpoint_IsConfigured() && Endpoint_IsINReady() && Endpoint_IsReadWriteAllowed()) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                Endpoint_Write_8(num_ovf);
                num_ovf = 0;
            }
            Endpoint_ClearIN();
        }
    }
}

