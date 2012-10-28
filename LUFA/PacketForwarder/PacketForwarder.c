
#include "PacketForwarder.h"
#include <avr/io.h>
#include <util/atomic.h>


// SRAM is scarce -- the device stalls if we 
//  get to 256
#define BUFFER_SIZE 128

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
        UDR1 = ch;
        UCSR1B |= (1 << TXEN1);
    }
}

int main(void) {
	SetupHardware();
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
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

	LEDs_Init();
	USB_Init();

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
        ++i;
    }
    PORTD |= (1 << 7);
}

void EVENT_USB_Device_Connect(void) {
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

void EVENT_USB_Device_Disconnect(void) {
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

void EVENT_USB_Device_ConfigurationChanged(void) {
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

	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
    if (!ConfigSuccess) {
        DebugWrite('F');
        DebugWrite('A');
        DebugWrite('I');
        DebugWrite('L');
        while (true) {
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


void StartTransmit(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (tx_head != tx_tail) {
            //  If I am not currently transmitting
            if (!(UCSR1B & (1 << TXEN1))) {
                UDR1 = tx_buf[tx_tail & (sizeof(tx_buf) - 1)];
                ++tx_tail;
            }
            UCSR1B = UCSR1B | (1 << UDRIE1) | (1 << TXEN1);
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
    ++rx_head;
    if (rx_head - rx_tail > 127) {
        ++num_ovf;
    }
}

void PacketForwarder_Task(void) {
	if (USB_DeviceState != DEVICE_STATE_Configured) {
	  return;
    }

    Endpoint_SelectEndpoint(DATA_TX_EPNUM);
    if (Endpoint_IsConfigured() && Endpoint_IsINReady() && Endpoint_IsReadWriteAllowed()) {
        uint8_t n = DATA_EPSIZE;
        while (n > 0 && rx_head != rx_tail) {
            Endpoint_Write_8(rx_buf[rx_tail & (sizeof(rx_buf) - 1)]);
            ++rx_tail;
            --n;
        }
        if (n != DATA_EPSIZE) {
            /* wrote anything? */
            Endpoint_ClearIN();
        }
    }

    Endpoint_SelectEndpoint(DATA_RX_EPNUM);
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

    Endpoint_SelectEndpoint(INFO_EPNUM);
    if (Endpoint_IsConfigured() && Endpoint_IsINReady() && Endpoint_IsReadWriteAllowed()) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            Endpoint_Write_8(num_ovf);
            num_ovf = 0;
        }
        Endpoint_ClearIN();
    }
}

