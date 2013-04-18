
#include "Config.h"
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Drivers/Peripheral/TWI.h>
#include "MoneyPit2.h"
#include <avr/io.h>
#include <util/atomic.h>

#define MAX_IN_SIZE 32

//  how many ticks to blink a LED
#define BLINK_CNT 10
#define EMPTY_IN_TIMEOUT 20

#define CMD_BEEP (0x10 | 0x01)          //  cmd 1, sizeix 1
#define CMD_MOTOR_SPEEDS (0x10 | 0x02)  //  cmd 1, sizeix 2
#define CMD_SERVO_TIMES (0x10 | 0x05)   //  cmd 1, sizeix 5

#define RET_COUNTER_VALUES (0x10 | 0x08 | 0x05) //  ret 1, RET, sizeix 5

unsigned char motorpower[2] = { 0x80, 0x80 };
unsigned short countervalues[4] = { 0 };
unsigned short servotimes[4] = { 0 };



void Reconfig(void);

int main(void) {
    SetupHardware();
    wdt_enable(WDTO_8S);

    sei();

    while (1) {
        wdt_reset();
        USB_USBTask();
        MoneyPit2_Task();
    }
}


unsigned char last_seq = 0;
unsigned short nbeep = 0;


void setup_twi() {
    TWI_Init(TWI_BIT_PRESCALE_1, TWI_BITLENGTH_FROM_FREQ(1, 400000));
}

void SetupHardware(void) {
    wdt_disable();
    setup_status();
    set_status(0xff, 0xff);
    PORTE = 0;
    DDRE |= (1 << 6);

    clock_prescale_set(clock_div_1);
    set_status(0x7f, 0xff);
    setup_delay();
    set_status(0x3f, 0xff);
    setup_adc();
    set_status(0x1f, 0xff);
    setup_twi();
    set_status(0xf, 0xff);

    USB_Init();
    set_status(0x0, 0xff);
}

void EVENT_USB_Device_Connect(void) {
    set_status(CONNECTED_LED, CONNECTED_LED);
}

void EVENT_USB_Device_Disconnect(void) {
    set_status(0x0, CONNECTED_LED);
    PORTE |= (1 << 6);
    nbeep = getms() + 200;
}

void EVENT_USB_Device_ConfigurationChanged(void) {
    Reconfig();
}

void EVENT_USB_Device_Reset(void) {
    Reconfig();
}

void Reconfig() {
    bool ConfigSuccess = 1;

    ConfigSuccess &= Endpoint_ConfigureEndpoint(
        DATA_RX_EPNUM,
        EP_TYPE_BULK, 
        DATA_RX_EPSIZE,
        2);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(
        DATA_TX_EPNUM,
        EP_TYPE_BULK, 
        DATA_TX_EPSIZE,
        1);

    if (!ConfigSuccess) {
        while (1) {
            set_status(0xff, 0xff);
            delayms(100);
            set_status(0, 0xff);
            delayms(100);
        }
    }
}

static unsigned char payloadsizes[8] = {
    0, 1, 2, 4,
    6, 8, 12, 255  //  255 really means varsize -- next byte is length
};

void dispatch(unsigned char const *sbuf, unsigned char offset, unsigned char end) {
    while (offset < end) {
        unsigned char cmd = sbuf[offset];
        unsigned char cmdSize = payloadsizes[cmd & 0x7];
        ++offset;
        if (cmdSize == 255) {
            cmdSize = sbuf[offset];
            ++offset;
        }
        if ((end - offset) < cmdSize) {
            //  missing data!
            show_error(1, cmdSize);
            break;
        }
        switch (cmd) {
            case CMD_BEEP:
                if (sbuf[0]) {
                    PORTE |= (1 << 6);
                    nbeep = getms() + sbuf[0];
                }
                else {
                    PORTE &= ~(1 << 6);
                }
                break;
            case CMD_MOTOR_SPEEDS:
                memcpy(motorpower, &sbuf[offset], sizeof(motorpower));
                break;
            case CMD_SERVO_TIMES:
                memcpy(servotimes, &sbuf[offset], sizeof(servotimes));
                break;
            default:
                //  unknown command
                show_error(2, cmd);
                return;
        }
        offset += cmdSize;
    }
}


enum TWIState {
    tsIdle,
    tsStartedMotor,
    tsEndedMotor,
    tsStartedCounters,
    tsEndedCounters,
    tsStartedServos,
    tsEndedServos
};

enum TWIState twi_state = tsIdle;
unsigned char twibuf[16];
unsigned char sndcnt = 0;
unsigned char sndend = 0;
unsigned short clear_twi = 0;
unsigned char twi_has_error = 0;

#define TWI_ID_MOTOR 0x1
#define TWI_ID_COUNTERS 0x2
#define TWI_ID_SERVOS 0x3
#define TWI_BIT_WRITE 0x0
#define TWI_BIT_READ 0x1


void twi_error(void) {
    twi_has_error = true;
    set_status(TWI_ERROR_LED, TWI_ERROR_LED);
    clear_twi = getms() + 1000;
}

void service_twi(void) {
    switch (twi_state) {
    case tsIdle:
        if (TWI_StartTransmission((TWI_ID_MOTOR << 1) | TWI_BIT_WRITE, 2) != TWI_ERROR_NoError) {
            twi_error();
            twi_state = tsEndedMotor;
            break;
        }
        twi_state = tsStartedMotor;
        memcpy(twibuf, motorpower, sizeof(motorpower));
        sndcnt = 0;
        sndend = (unsigned char)sizeof(motorpower);
        break;
    case tsStartedMotor:
        if (sndcnt == sndend) {
            TWI_StopTransmission();
            twi_state = tsEndedMotor;
            break;
        }
        else {
            if (!TWI_SendByte(twibuf[sndcnt])) {
                twi_error();
                twi_state = tsEndedMotor;
                break;
            }
            ++sndcnt;
        }
        break;
    case tsEndedMotor:
        if (TWI_StartTransmission((TWI_ID_COUNTERS << 1) | TWI_BIT_READ, 2) != TWI_ERROR_NoError) {
            twi_error();
            twi_state = tsEndedCounters;
            break;
        }
        twi_state = tsStartedCounters;
        sndend = sizeof(countervalues);
        sndcnt = 0;
        break;
    case tsStartedCounters:
        if (sndcnt == sndend) {
            TWI_StopTransmission();
            twi_state = tsEndedCounters;
            memcpy(countervalues, twibuf, sizeof(countervalues));
            break;
        }
        if (!TWI_ReceiveByte(&twibuf[sndcnt], (sndcnt == sndend-1))) {
            twi_error();
            twi_state = tsEndedCounters;
            break;
        }
        ++sndcnt;
        break;
    case tsEndedCounters:
        if (TWI_StartTransmission((TWI_ID_SERVOS << 1) | TWI_BIT_WRITE, 5) != TWI_ERROR_NoError) {
            twi_error();
            twi_state = tsEndedServos;
            break;
        }
        memcpy(twibuf, servotimes, sizeof(servotimes));
        sndcnt = 0;
        sndend = sizeof(servotimes);
        twi_state = tsStartedServos;
        break;
    case tsStartedServos:
        if (sndcnt == sndend) {
            TWI_StopTransmission();
            twi_state = tsEndedServos;
            break;
        }
        if (!TWI_SendByte(twibuf[sndcnt])) {
            twi_error();
            twi_state = tsEndedServos;
            break;
        }
        ++sndcnt;
        break;
    case tsEndedServos:
        twi_state = tsIdle;
        break;
    }
}


unsigned char epic;
unsigned char epiir;
unsigned char epirwa;

unsigned short clear_received;
unsigned short last_in = 0;


void MoneyPit2_Task(void) {

    if (USB_DeviceState != DEVICE_STATE_Configured) {
        return;
    }

    /* see if there's data to send to host */
    unsigned short now = getms();
    Endpoint_SelectEndpoint(DATA_RX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
    epic = Endpoint_IsConfigured();
    epiir = epic && Endpoint_IsINReady();
    epirwa = epiir && Endpoint_IsReadWriteAllowed();
    if (epirwa) {
        if (now - last_in > EMPTY_IN_TIMEOUT) {
            set_status(POLLING_LED, POLLING_LED);
            clear_received = now + BLINK_CNT;
            last_in = now;
            Endpoint_Write_8(last_seq);
            Endpoint_Write_8(RET_COUNTER_VALUES);
            for (unsigned char i = 0; i != sizeof(countervalues); ++i) {
                Endpoint_Write_8(((unsigned char *)countervalues)[i]);
            }
            Endpoint_ClearIN();
        }
    }

    /* see if there's data from the host */
    Endpoint_SelectEndpoint(DATA_TX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_OUT);
    if (Endpoint_IsConfigured() && Endpoint_IsOUTReceived() && Endpoint_IsReadWriteAllowed()) {
        static unsigned char sbuf[MAX_IN_SIZE] = { 0 };
        set_status(RECEIVED_LED, RECEIVED_LED);
        clear_received = now + BLINK_CNT;
        uint8_t n = Endpoint_BytesInEndpoint();
        if (n > sizeof(sbuf) + 1) {
            show_error(3, n);
        }
        if (n) {
            last_seq = Endpoint_Read_8();
            --n;
            for (unsigned char c = 0; c < n; ++c) {
                sbuf[c] = Endpoint_Read_8();
            }
        }
        Endpoint_ClearOUT();
        if (n) {
            dispatch(sbuf, 0, n);
        }
    }

    service_twi();

    /* remove received status */
    if ((short)(now - clear_received) > 0) {
        set_status(0, RECEIVED_LED | POLLING_LED);
        clear_received = now + 1000;
    }

    if ((short)(now - nbeep) > 0) {
        PORTE &= ~(1 << 6);
        nbeep = now + 1000;
    }

    if ((short)(now - clear_twi) > 0) {
        set_status(0, TWI_ERROR_LED);
        clear_twi = now + 1000;
        if (twi_has_error) {
            twi_has_error = false;
            setup_twi();
        }
    }
}

