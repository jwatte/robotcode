
#include "OnyxWalker.h"
#include <avr/io.h>
#include <util/atomic.h>

#include "my32u4.h"


#define CMD_RAW_MODE 0x01
//  param is baud rate
#define CMD_RAW_DATA 0x00
//  all data is data
#define CMD_SET_REG1 0x13
//  param is id, reg, data
#define CMD_SET_REG2 0x14
//  param is id, reg, datal, datah
#define CMD_GET_REGS 0x23
//  param is id, reg, count
#define CMD_NOP 0xf0
//  no param
#define CMD_DELAY 0x31
//  param is milliseconds

#define READ_COMPLETE 0x41

#define DXL_PING 1
#define DXL_REG_READ 2
#define DXL_REG_WRITE 3
#define DXL_PEND_WRITE 4
#define DXL_PEND_COMMIT 5
#define DXL_RESET 6
#define DXL_MULTIWRITE 0x83

#define BLINK_CNT 2550

void Reconfig(void);

int main(void) {
	SetupHardware();
	sei();

	while (true) {
		USB_USBTask();
		OnyxWalker_Task();
	}
}

//  Broadcast to turn off torque for all servos on the bus.
static const unsigned char notorque_packet[] = {
    0xff, 0xff, 0xfe, 0x4, 0x3, 0x18, 0, (unsigned char)~(0xfe + 0x4 + 0x3 + 0x18 + 0),
};

static unsigned char xbuf[64];
unsigned char xbufptr = 0;

void clear_xbuf(void) {
    xbufptr = 0;
}

void add_xbuf(unsigned char const *ptr, unsigned char len) {
    if (sizeof(xbuf) - xbufptr < len) {
        //  dropped packet
        show_error(7, len);
        return;
    }
    memcpy(&xbuf[xbufptr], ptr, len);
    xbufptr += len;
}

void SetupHardware(void) {
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

    //  weapons fire off
    PORTB &= ~((1 << PB4) | (1 << PB5));
    DDRB |= ((1 << PB4) | (1 << PB5));
    
    //  Status LEDs on
    DDRB |= 0xf;
    PORTB |= 0xf;

	clock_prescale_set(clock_div_1);
    setup_delay();
    delayms(50);   //  show off a little bit
    PORTB &= ~0x8;

    setup_uart(0);
    delayms(50);
    PORTB &= ~0x4;

    send_sync(notorque_packet, sizeof(notorque_packet));    //  turn off torque on all servos
    delayms(50);
    PORTB &= ~0x2;

    //  ... reserved for future expansion :-)
    delayms(50);
    PORTB &= ~0x1;

	USB_Init();
}

void EVENT_USB_Device_Connect(void) {
    PORTB |= 0x4;
}

void EVENT_USB_Device_Disconnect(void) {
    PORTB &= ~0x4;
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
            PORTB |= 0xf;
            delayms(100);
            PORTB &= ~0xf;
            delayms(100);
        }
    }
}

unsigned char pbuf[16];

unsigned char cksum(unsigned char const *ptr, unsigned char n) {
    unsigned char ck = 0;
    unsigned char const *end = ptr + n;
    while (end != ptr) {
        ck += *ptr;
        ++ptr;
    }
    return ~ck;
}

void reg_write(unsigned char id, unsigned char reg, unsigned char const *buf, unsigned char cnt) {
    if (cnt > 9) {
        show_error(5, cnt);
        cnt = 9;
    }
    pbuf[0] = 0xff;
    pbuf[1] = 0xff;
    pbuf[2] = id;
    pbuf[3] = cnt + 3;
    pbuf[4] = DXL_REG_WRITE;
    pbuf[5] = reg;
    memcpy(&pbuf[6], buf, cnt);
    pbuf[6 + cnt] = cksum(&pbuf[2], cnt + 4);
    send_sync(pbuf, cnt + 7);
    //  assume servos do not ack writes
}

unsigned char recv_packet(unsigned char *dst, unsigned char maxsz) {
    PORTB |= 0x1;
    unsigned char cnt = 0;
    UCSR1B = (1 << RXEN1);
    unsigned char tc = TCNT0;
    unsigned char ntc = tc;
    while (cnt < maxsz) {
        while (!(UCSR1A & (1 << RXC1))) {
            //  don't spend more than 100 microseconds waiting for something that won't come
            ntc = TCNT0;
            if (ntc - tc > 200) {
                show_error(8, cnt);
                break;
            }
        }
        tc = ntc;
        dst[cnt] = UDR1;
        ++cnt;
    }
    UCSR1B = (1 << RXEN1) | (1 << RXCIE1);
    PORTB &= ~0x1;
    return cnt;
}

void reg_read(unsigned char id, unsigned char reg, unsigned char cnt) {
    if (cnt > 9) {
        show_error(6, cnt);
        cnt = 9;
    }
    pbuf[0] = 0xff;
    pbuf[1] = 0xff;
    pbuf[2] = id;
    pbuf[3] = 4;
    pbuf[4] = DXL_REG_READ;
    pbuf[5] = reg;
    pbuf[6] = cnt;
    pbuf[7] = cksum(&pbuf[2], 5);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        send_sync(pbuf, 8);
        cnt = recv_packet(pbuf, 6 + cnt);
        pbuf[1] = READ_COMPLETE;
        pbuf[2] = id;
        pbuf[3] = reg;
        pbuf[4] = cnt - 6;
        if (cnt > 6) {
            add_xbuf(&pbuf[1], cnt - 2);
        }
    }
}


bool rawmode = false;
unsigned char sbuf[DATA_EPSIZE];

void wait_for_idle() {
    PORTB |= 0x2;
    while (true) {
        unsigned char av = recv_avail();
        unsigned char ts = TCNT0;
        //  Wait for 40 us without any data -- that's 80 bits; plenty of time 
        //  for the bus to clear!
        while ((unsigned char)(TCNT0 - ts) < 10) {
            ; // do nothing
        }
        if (av == recv_avail()) {
            break;
        }
    }
    PORTB &= ~0x2;
}

void dispatch(unsigned char const *sbuf, unsigned char offset, unsigned char end) {
    while (offset < end) {
        if (rawmode && sbuf[offset] == CMD_RAW_DATA) {
            wait_for_idle();
            send_sync(&sbuf[offset+1], end - offset - 1);
            break;
        }
        unsigned char cmdSize = (sbuf[offset] & 0x7) + 1;
        if ((end - offset) < cmdSize) {
            //  missing data!
            show_error(1, cmdSize);
            show_error(3, offset);
            show_error(3, end);
            break;
        }
        switch (sbuf[offset]) {
            case CMD_RAW_MODE:
                rawmode = true;
                setup_uart(sbuf[offset+1]);
                break;
            case CMD_SET_REG1:
                rawmode = false;
                reg_write(sbuf[offset+1], sbuf[offset+2], &sbuf[offset+3], 1); break;
            case CMD_SET_REG2:
                rawmode = false;
                reg_write(sbuf[offset+1], sbuf[offset+2], &sbuf[offset+3], 2);
                break;
            case CMD_GET_REGS:
                rawmode = false;
                reg_read(sbuf[offset+1], sbuf[offset+2], sbuf[offset+3]);
                break;
            case CMD_DELAY:
                rawmode = false;
                PORTB |= 0x2;
                delayms(sbuf[offset+1]);
                wait_for_idle();
                PORTB &= ~0x2;
                break;
            case CMD_NOP:
                break;
            default:
                rawmode = false;
                //  unknown command
                show_error(2, sbuf[offset]);
                return;
        }
        offset += cmdSize;
    }
}

unsigned short clearcnt = 0;

void OnyxWalker_Task(void) {
	if (USB_DeviceState != DEVICE_STATE_Configured) {
	  return;
    }

    if (true) {
        Endpoint_SelectEndpoint(DATA_TX_EPNUM);
        Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
        if (Endpoint_IsConfigured() && Endpoint_IsINReady() && Endpoint_IsReadWriteAllowed()) {
            unsigned char gg = xbufptr;
            if (gg) {
                for (unsigned char q = 0; q < gg; ++q) {
                    Endpoint_Write_8(xbuf[q]);
                }
                clear_xbuf();
            }
            unsigned char m = recv_avail();
            if (m) {
                unsigned char nm = m;
                PORTB &= ~0x4;
                if (rawmode) {
                    clearcnt = BLINK_CNT;
                    if (m + gg > DATA_EPSIZE) {
                        m = DATA_EPSIZE - gg;
                    }
                    nm = m;
                    unsigned char const *ptr = recv_buf();
                    while (m > 0) {
                        Endpoint_Write_8(*ptr);
                        ++ptr;
                        --m;
                    }
                }
                recv_eat(nm);
            }
            Endpoint_ClearIN();
        }
    }

    Endpoint_SelectEndpoint(DATA_RX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_OUT);
    if (Endpoint_IsConfigured() && Endpoint_IsOUTReceived() && Endpoint_IsReadWriteAllowed()) {
        uint8_t n = Endpoint_BytesInEndpoint();
        if (n) {
            PORTB &= ~0x4;
            clearcnt = BLINK_CNT;
            for (unsigned char c = 0; c < n; ++c) {
                sbuf[c] = Endpoint_Read_8();
            }
        }
        Endpoint_ClearOUT();
        if (n) {
            dispatch(sbuf, 0, n);
        }
    }

    if (true) {
        Endpoint_SelectEndpoint(INFO_EPNUM);
        Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
        if (Endpoint_IsConfigured() && Endpoint_IsINReady() && Endpoint_IsReadWriteAllowed()) {
            unsigned char nm = get_nmissed();
            if (nm > 0) {
                Endpoint_Write_8(nm);
                Endpoint_ClearIN();
            }
        }
    }
    if (clearcnt == 0) {
        PORTB |= 0x4;
    }
    else {
        --clearcnt;
    }
}

